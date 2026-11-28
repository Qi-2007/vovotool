#!/usr/bin/env python3
"""把国服 profile 里的函数 RVA 迁到另一份构建（国际服）上。

流水线四级，逐级收紧：
  1. 16 字节精确命中（快，但只覆盖没改动的函数）
  2. 国服函数前 192 字节做模糊匹配（每 8 字节一颗种子），取头部候选
  3. 候选一对一算指令形态相似度（capstone 反汇编，剥掉立即数和地址）
  4. 用已定位锚点把国际服地址框住（窗口法），窗口内重搜一遍

输出 JSON：{ "分类/名字": {"os": rva, "sim": 相似度, "src": 来源阶段} }

依赖：capstone（pip install capstone）

用法：
    python3 tools/match_os.py \
        --base libyuanshen_cn.so --target libyuanshen_os.so \
        --profile profile.json --out os_profile.json
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from difflib import SequenceMatcher
from pathlib import Path

try:
    from capstone import CS_ARCH_ARM64, CS_MODE_ARM, Cs
except ImportError:
    sys.exit('缺少 capstone：pip install capstone')

IMM = re.compile(r'#-?0x[0-9a-fA-F]+|#-?\d+')
HEX = re.compile(r'0x[0-9a-fA-F]+')
MD = Cs(CS_ARCH_ARM64, CS_MODE_ARM)


def normalize(insn):
    """剥掉立即数和地址，只留指令形态（寄存器编号保留）。"""
    return insn.mnemonic + ' ' + HEX.sub('A', IMM.sub('#I', insn.op_str))


def tokenize(data: bytes, start: int, limit: int = 48) -> list[str]:
    out = []
    for insn in MD.disasm(data[start:start + limit * 4], start):
        out.append(normalize(insn))
        if insn.mnemonic.startswith('ret'):
            break
    return out


def similarity(a: bytes, a_rva: int, b: bytes, b_rva: int) -> float:
    ta, tb = tokenize(a, a_rva), tokenize(b, b_rva)
    if not ta or not tb:
        return 0.0
    return SequenceMatcher(None, ta, tb).ratio()


def seed_votes(base: bytes, base_rva: int, target: bytes, window: tuple[int, int] | None):
    """种子投票：返回 {目标地址: 票数}，window 给定时只在窗口内找。"""
    body = base[base_rva:base_rva + 192]
    lo, hi = window if window else (0, len(target))
    seg = target[lo:hi]
    votes: dict[int, int] = {}
    for off in range(0, len(body) - 8, 8):
        seed = body[off:off + 8]
        pos = 0
        for _ in range(8):
            hit = seg.find(seed, pos)
            if hit < 0:
                break
            pos = hit + 1
            start = hit - off + lo
            if start >= 0:
                votes[start] = votes.get(start, 0) + 1
    return votes


def rank_candidates(base, base_rva, target, votes, limit=10):
    scored = []
    for addr in sorted(votes, key=lambda k: -votes[k])[:limit]:
        s = similarity(base, base_rva, target, addr)
        if s > 0:
            scored.append((s, addr))
    scored.sort(reverse=True)
    return scored


def load_profile(path: Path):
    prof = json.loads(path.read_text(encoding='utf8'))
    for cat, items in prof.items():
        if isinstance(items, list):
            for item in items:
                yield cat, item['name'], item['rva'], bytes.fromhex(item['bytes'])


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument('--base', required=True, help='国服 .so（与 profile 同版本）')
    ap.add_argument('--target', required=True, help='目标构建 .so')
    ap.add_argument('--profile', required=True, help='profile.json')
    ap.add_argument('--out', required=True, help='输出 json')
    ap.add_argument('--min-sim', type=float, default=0.85)
    ap.add_argument('--min-margin', type=float, default=0.10)
    args = ap.parse_args()

    base = Path(args.base).read_bytes()
    target = Path(args.target).read_bytes()
    entries = list(load_profile(Path(args.profile)))

    resolved: dict[str, dict] = {}
    pending = []
    for cat, name, rva, sig in entries:
        key = f'{cat}/{name}'
        exact = target.find(sig)
        if exact >= 0:
            resolved[key] = {'os': exact, 'sim': 1.0, 'src': '精确16字节'}
        else:
            pending.append((cat, name, rva))

    # 第一轮：全文模糊 + 指令级确认
    anchor_scores = []
    for cat, name, rva in pending:
        votes = seed_votes(base, rva, target, None)
        ranked = rank_candidates(base, rva, target, votes)
        if not ranked:
            continue
        best = ranked[0][0]
        second = ranked[1][0] if len(ranked) > 1 else 0.0
        if best >= args.min_sim and best - second >= args.min_margin:
            resolved[f'{cat}/{name}'] = {'os': ranked[0][1], 'sim': round(best, 3), 'src': '指令级'}
        anchor_scores.append((cat, name, rva, ranked))

    # 第二轮：窗口法，只对还没定的下手。
    # 地标 = 精确命中 + 指令级高置信（sim>=0.98）的结果，按国服 RVA 排序
    rva_of = {f'{c}/{n}': r for c, n, r, _ in entries}
    anchor_pairs = [(rva_of[key], entry['os'])
                    for key, entry in resolved.items()
                    if entry['src'] == '精确16字节' or entry['sim'] >= 0.98]
    anchor_pairs.sort()
    cn_index = [a[0] for a in anchor_pairs]

    import bisect
    PAD = 0x80000

    def window_of(cn_rva):
        i = bisect.bisect_left(cn_index, cn_rva)
        lo = anchor_pairs[i - 1][1] if i > 0 else 0
        hi = anchor_pairs[i][1] if i < len(anchor_pairs) else len(target)
        if hi < lo:
            return None
        return max(0, lo - PAD), min(len(target), hi + PAD)

    # 窗口法跑两遍：第一遍的结果能当地标，帮第二遍收窄范围
    for _pass in range(2):
        anchor_pairs = sorted((rva_of[key], entry['os']) for key, entry in resolved.items()
                              if entry['src'] == '精确16字节' or entry['sim'] >= 0.85)
        cn_index[:] = [a[0] for a in anchor_pairs]
        added = 0
        for cat, name, rva, _ranked in anchor_scores:
            key = f'{cat}/{name}'
            if key in resolved:
                continue
            win = window_of(rva)
            if not win:
                continue
            votes = seed_votes(base, rva, target, win)
            ranked2 = rank_candidates(base, rva, target, votes)
            if not ranked2:
                continue
            best = ranked2[0][0]
            second = ranked2[1][0] if len(ranked2) > 1 else 0.0
            if best >= args.min_sim - 0.05 and best - second >= args.min_margin:
                resolved[key] = {'os': ranked2[0][1], 'sim': round(best, 3), 'src': '窗口法'}
                added += 1
        if not added:
            break

    Path(args.out).write_text(json.dumps(resolved, indent=1, ensure_ascii=False), encoding='utf8')
    total = len(entries)
    print(f'共 {total} 条，定位 {len(resolved)} 条 -> {args.out}')
    for src in ('精确16字节', '指令级', '窗口法'):
        n = sum(1 for v in resolved.values() if v['src'] == src)
        print(f'  {src}: {n}')
    missing = [f'{c}/{n}' for c, n, _, _ in entries if f'{c}/{n}' not in resolved]
    if missing:
        print('未定位:', '、'.join(missing))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
