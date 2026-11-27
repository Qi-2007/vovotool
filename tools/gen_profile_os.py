#!/usr/bin/env python3
"""用国际服定位结果生成 profile_os.hpp。

profile.hpp 是国服的硬编码表，国际服要另起一份：
  - 函数常量：定位到了就写国际服 RVA，没定位到写 0（module.cpp 里见到 0 就跳过对应 hook）
  - pin 数组：只收已定位的，字节在编译期做校验用的签名叫「国际服自己的 16 字节」，
    所以必须从国际服 .so 里现读，不能照抄 profile.json 的国服字节

用法：
    python3 tools/gen_profile_os.py \
        --profile profile.json --draft os_profile_draft.json \
        --target /path/libyuanshen_os.so --out app/src/main/cpp/profile_os.hpp
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path

CATS = ['functions', 'graphics', 'photo', 'look', 'touch', 'mapMouse',
        'teamLoading', 'avatarFade', 'avatarSwitch', 'regionUnlock']


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument('--profile', required=True)
    ap.add_argument('--draft', required=True, help='match_os.py 的输出，或手工整理的 {分类/名字: {os, sim, src}}')
    ap.add_argument('--target', required=True, help='国际服 .so，用来取 pin 字节')
    ap.add_argument('--out', required=True)
    args = ap.parse_args()

    prof = json.loads(Path(args.profile).read_text(encoding='utf8'))
    draft = json.loads(Path(args.draft).read_text(encoding='utf8'))
    pins = draft.get('pins', draft)
    data = draft.get('data', {})
    image = Path(args.target).read_bytes()

    lines = [
        '// 自动生成，别手改。国服表见 profile.hpp。',
        '#pragma once',
        '#include <cstdint>',
        '',
        'namespace profile {',
        'struct Pin { uintptr_t rva; unsigned char bytes[16]; };',
        '',
    ]

    unresolved = []
    for cat in CATS:
        for item in prof.get(cat, []):
            key = f'{cat}/{item["name"]}'
            hit = pins.get(key)
            rva = hit['os'] if hit else 0
            if not rva:
                unresolved.append(key)
            lines.append(f'inline constexpr uintptr_t {item["name"]}=0x{rva:x};')

    # 数据地址
    for name, entry in data.items():
        if not isinstance(entry, dict):
            continue
        lines.append(f'inline constexpr uintptr_t {name}=0x{entry.get("os", 0):x};')

    lines.append('')
    for cat in CATS:
        if not prof.get(cat):
            continue
        # 国服表里 functions 分类的签名数组就叫 pins，不是 functionsPins
        name = 'pins' if cat == 'functions' else f'{cat}Pins'
        rows = []
        for item in prof[cat]:
            hit = pins.get(f'{cat}/{item["name"]}')
            if not hit or not hit.get('os'):
                continue
            rva = hit['os']
            raw = image[rva:rva + 16]
            if len(raw) != 16:
                continue
            rows.append('{0x%x,{%s}},' % (rva, ','.join(f'0x{b:02x}' for b in raw)))
        if not rows:
            continue
        lines.append(f'inline constexpr Pin {name}[]={{')
        lines += rows
        lines.append('};')
        lines.append(f'inline constexpr int {name}Count={len(rows)};')
        lines.append('')

    # 国服表里用到的 Pin 数组，国际服没有对应定位时也要留空壳，
    # 否则模块代码里 for(const auto& pin:profile::xxxPins) 编译不过。
    import re
    cn_header = Path(args.out).with_name('profile.hpp')
    if cn_header.exists():
        emitted = set(re.findall(r'inline constexpr Pin (\w+)\[\]', '\n'.join(lines)))
        for name in re.findall(r'inline constexpr Pin (\w+)\[\]', cn_header.read_text(encoding='utf8')):
            if name in emitted:
                continue
            lines.append(f'inline constexpr Pin {name}[]={{}};')
            lines.append(f'inline constexpr int {name}Count=0;')
            lines.append('')

    lines.append('} // namespace profile')
    Path(args.out).write_text('\n'.join(lines) + '\n', encoding='utf8')

    total = sum(len(prof.get(c, [])) for c in CATS)
    print(f'生成 {args.out}：{total} 条常量，未定位 {len(unresolved)} 条')
    for key in unresolved:
        print('  未定位', key)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
