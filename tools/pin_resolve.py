#!/usr/bin/env python3
"""补定位：给「落在函数中间」的 pin 找国际服地址。

有些 pin 不是函数开头，是函数体里某条指令的地址（作者当初用 16 字节签名卡在那里）。
这类地址用调用边外推永远碰不到，因为它们不是任何一次调用的目标。

做法：
  1. 在国服里找 pin 之前最近的一个「已知函数起点」（来自已验证的映射表）
  2. 算出 pin 相对该起点的指令偏移 delta
  3. 在国际服同一函数里按 delta 取候选地址（允许 ±几指令的抖动）
  4. 拿 pin 处的 8 条指令形态跟候选处逐条比相似度，>= 阈值才算命中

用法：
    python3 tools/pin_resolve.py --cn CN.so --os OS.so --pins profile.json \
        --map os_callmap2.json --draft os_profile_draft.json --report pin_report.json
"""
from __future__ import annotations

import argparse
import json
from difflib import SequenceMatcher
from pathlib import Path

from capstone import CS_ARCH_ARM64, CS_MODE_ARM, Cs

MD = Cs(CS_ARCH_ARM64, CS_MODE_ARM)
WINDOW = 8


def norm_ins(image: bytes, rva: int, count: int = WINDOW) -> list[str]:
    out = []
    for ins in MD.disasm(image[rva:rva + count * 4], rva):
        op = ins.op_str
        out.append(ins.mnemonic + ' ' + ' '.join(
            '#IMM' if tok.lstrip('#').startswith('0x') else tok for tok in op.split(', ')) if op else ins.mnemonic)
    return out


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument('--cn', required=True)
    ap.add_argument('--os', required=True)
    ap.add_argument('--pins', required=True, help='profile.json')
    ap.add_argument('--map', required=True, help='callmap 输出')
    ap.add_argument('--draft', required=True, help='os_profile_draft.json，就地更新')
    ap.add_argument('--report', required=True)
    ap.add_argument('--min-sim', type=float, default=0.9)
    ap.add_argument('--shake', type=int, default=4, help='允许的指令抖动')
    args = ap.parse_args()

    cn = Path(args.cn).read_bytes()
    os_img = Path(args.os).read_bytes()
    callmap = json.loads(Path(args.map).read_text(encoding='utf8'))
    pairs = {int(k, 16): int(v, 16) for k, v in callmap['pairs'].items()}
    prof = json.loads(Path(args.pins).read_text(encoding='utf8'))
    draft = json.loads(Path(args.draft).read_text(encoding='utf8'))

    starts = sorted(pairs)
    report = {'resolved': {}, 'failed': {}}

    for cat, items in prof.items():
        if not isinstance(items, list):
            continue
        for item in items:
            key = f'{cat}/{item["name"]}'
            if draft['pins'].get(key, {}).get('os'):
                continue
            pin = int(item['rva'])
            # 1. 候选基准：pin 之前若干个已知函数起点（最近的未必是真正的外层函数）
            bases = [s for s in starts if 0 < pin - s <= 0x3000][-6:]
            if not bases:
                report['failed'][key] = '前面 0x3000 内没有已知函数起点'
                continue
            want = norm_ins(cn, pin)
            if not want:
                report['failed'][key] = 'pin 处无法反汇编'
                continue
            best = (0.0, None, None, None)
            for base in bases:
                delta = pin - base
                about = pairs[base]
                for k in range(-args.shake, args.shake + 1):
                    cand = about + delta + 4 * k
                    if cand <= 0 or cand + WINDOW * 4 > len(os_img):
                        continue
                    got = norm_ins(os_img, cand)
                    if not got:
                        continue
                    sim = SequenceMatcher(None, want, got, autojunk=False).ratio()
                    if sim > best[0]:
                        best = (sim, cand, base, delta)
            if best[1] and best[0] >= args.min_sim:
                sim, cand, base, delta = best
                draft['pins'][key] = {
                    'os': cand,
                    'sim': round(sim, 3),
                    'src': f'函数内偏移：国服 0x{base:x}+0x{delta:x} -> 国际服 0x{pairs[base]:x}',
                }
                report['resolved'][key] = {'cn': pin, 'os': cand, 'sim': round(sim, 3),
                                           'base_cn': base, 'base_os': pairs[base], 'delta': delta}
            else:
                report['failed'][key] = f'相似度不足（最佳 {best[0]:.3f} @ {hex(best[1]) if best[1] else "无"}）'

    Path(args.draft).write_text(json.dumps(draft, indent=1, ensure_ascii=False), encoding='utf8')
    Path(args.report).write_text(json.dumps(report, indent=1, ensure_ascii=False), encoding='utf8')
    print(f'补上 {len(report["resolved"])} 条，仍失败 {len(report["failed"])} 条')
    for k, v in report['resolved'].items():
        print(f'  + {k} -> 0x{v["os"]:x} (相似度 {v["sim"]})')
    for k, v in report['failed'].items():
        print(f'  - {k}: {v}')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
