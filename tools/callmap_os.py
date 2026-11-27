#!/usr/bin/env python3
"""国际服定位：调用边外推 + 指令形态校验。

前提（浅笑 2026-11-27 12:31 确认）：国服与国际服版本同步、同源同编译器，
函数体指令序列基本一一对应，差别只有地址和立即数。

流程：
  1. 从已知锚点出发，逐个函数取「调用目标序列」（bl + 尾调用 b）
  2. 两侧数量一致就按顺序配出新候选对
  3. 候选对必须过校验：规范化指令序列相似度 >= --min-sim，且长度差在 --max-len-gap 内
  4. 过校验才入库，反复迭代

为什么要校验：只靠边数相等会掺进大量错配（实测一轮能冒出六千多对假对应）。
宁可少配，不许把假映射塞进 profile_os.hpp。

用法：
    python3 tools/callmap_os.py --cn CN.so --os OS.so --known os_profile_draft.json \
        --out os_callmap.json --rounds 12 --min-sim 0.9
"""
from __future__ import annotations

import argparse
import json
from difflib import SequenceMatcher
from pathlib import Path

from capstone import CS_ARCH_ARM64, CS_MODE_ARM, Cs

MAX_INSNS = 4000
MD = Cs(CS_ARCH_ARM64, CS_MODE_ARM)
MD.detail = True

_cache: dict[tuple[int, int], tuple[list[str], list[int]]] = {}


def analyze(image: bytes, rva: int, key: int) -> tuple[list[str], list[int]] | None:
    """返回 (规范化指令序列, 调用目标序列)。同一 rva 只反汇编一次。"""
    hit = _cache.get((key, rva))
    if hit is not None:
        return hit
    if rva <= 0 or rva + 4 > len(image) or rva % 4:
        return None
    norm: list[str] = []
    calls: list[int] = []
    ins_list = []
    for ins in MD.disasm(image[rva:rva + MAX_INSNS * 4], rva):
        ins_list.append(ins)
        if ins.mnemonic.startswith('ret') or len(ins_list) > MAX_INSNS:
            break
    if not ins_list:
        return None
    end = ins_list[-1].address + 4
    for ins in ins_list:
        op = ins.op_str
        # 立即数/地址一律抹平，比较的是「形态」
        if op:
            norm.append(f'{ins.mnemonic} ' + ' '.join(
                '#IMM' if tok.lstrip('#').startswith('0x') else tok for tok in op.split(', ')
            ))
        else:
            norm.append(ins.mnemonic)
        if ins.mnemonic == 'bl':
            calls.append(ins.operands[0].imm)
        elif ins.mnemonic == 'b':
            tgt = ins.operands[0].imm
            if tgt < rva or tgt >= end:
                calls.append(tgt)
    _cache[(key, rva)] = (norm, calls)
    return norm, calls


def similarity(a: list[str], b: list[str]) -> float:
    if not a or not b:
        return 0.0
    return SequenceMatcher(None, a, b, autojunk=False).ratio()


def load_known(draft: dict, prof_path: Path) -> dict[int, int]:
    known: dict[int, int] = {}
    for key, hit in draft.get('pins', {}).items():
        if isinstance(hit, dict) and hit.get('os') and hit.get('cn'):
            known[int(hit['cn'])] = int(hit['os'])
    if prof_path.exists():
        prof = json.loads(prof_path.read_text(encoding='utf8'))
        for cat, items in prof.items():
            if not isinstance(items, list):
                continue
            for item in items:
                hit = draft.get('pins', {}).get(f'{cat}/{item["name"]}')
                if hit and hit.get('os'):
                    known[int(item['rva'])] = int(hit['os'])
    return known


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument('--cn', required=True)
    ap.add_argument('--os', required=True)
    ap.add_argument('--known', required=True)
    ap.add_argument('--out', required=True)
    ap.add_argument('--rounds', type=int, default=12)
    ap.add_argument('--min-sim', type=float, default=0.9)
    ap.add_argument('--max-len-gap', type=float, default=0.12)
    ap.add_argument('--max-insns', type=int, default=1200, help='太长的函数不参与（反汇编慢且易误配）')
    args = ap.parse_args()

    cn = Path(args.cn).read_bytes()
    os_img = Path(args.os).read_bytes()
    known = load_known(json.loads(Path(args.known).read_text(encoding='utf8')),
                       Path(args.known).with_name('profile.json'))
    reverse = set(known.values())
    print(f'起点 {len(known)} 对')

    stats = {'candidates': 0, 'accepted': 0, 'rejected_sim': 0, 'rejected_dup': 0, 'skipped_shape': 0}
    evidence: dict[str, str] = {}

    for rnd in range(args.rounds):
        new: dict[int, int] = {}
        for cn_rva, os_rva in sorted(known.items()):
            a = analyze(cn, cn_rva, 0)
            b = analyze(os_img, os_rva, 1)
            if not a or not b:
                continue
            (na, ca), (nb, cb) = a, b
            if len(na) > args.max_insns or len(nb) > args.max_insns:
                stats['skipped_shape'] += 1
                continue
            if not ca or len(ca) != len(cb):
                stats['skipped_shape'] += 1
                continue
            for ta, tb in zip(ca, cb):
                if ta in known or ta in new:
                    continue
                stats['candidates'] += 1
                if tb in reverse or tb in new.values():
                    stats['rejected_dup'] += 1
                    continue
                va = analyze(cn, ta, 0)
                vb = analyze(os_img, tb, 1)
                if not va or not vb:
                    stats['rejected_sim'] += 1
                    continue
                la, lb = len(va[0]), len(vb[0])
                if max(la, lb) > args.max_insns * 1.5:
                    stats['rejected_sim'] += 1
                    continue
                if abs(la - lb) / max(la, lb, 1) > args.max_len_gap:
                    stats['rejected_sim'] += 1
                    continue
                sim = similarity(va[0], vb[0])
                if sim < args.min_sim:
                    stats['rejected_sim'] += 1
                    continue
                if len(va[1]) != len(vb[1]):
                    stats['rejected_sim'] += 1
                    continue
                new[ta] = tb
                stats['accepted'] += 1
                evidence[f'0x{ta:x}'] = (f'国服 0x{cn_rva:x} 的调用点 -> 国际服 0x{os_rva:x} 对应点；'
                                         f'相似度 {sim:.3f}，长度 {la}/{lb}')
        if not new:
            print(f'第 {rnd + 1} 轮无新增，收敛')
            break
        known.update(new)
        reverse.update(new.values())
        print(f'第 {rnd + 1} 轮新增 {len(new)} 对，累计 {len(known)}')

    out = {
        'pairs': {f'0x{k:x}': f'0x{v:x}' for k, v in sorted(known.items())},
        'evidence': evidence,
        'stats': stats,
        'params': {'min_sim': args.min_sim, 'max_len_gap': args.max_len_gap},
    }
    Path(args.out).write_text(json.dumps(out, indent=1, ensure_ascii=False), encoding='utf8')
    print(f'累计 {len(known)} 对 -> {args.out}')
    print('统计', stats)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
