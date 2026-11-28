"""从 profile_os.hpp 生成 os_profile.json，结构与国服 profile.json 对齐。"""
import json, re
from pathlib import Path

ROOT = Path('.')
hpp = (ROOT/'app/src/main/cpp/profile_os.hpp').read_text()

# 1) 解析所有 inline constexpr 地址常量
consts = {}
for m in re.finditer(r'inline constexpr uintptr_t (\w+)\s*=\s*(0x[0-9a-fA-F]+|\d+);', hpp):
    name, val = m.group(1), m.group(2)
    consts[name] = int(val, 16) if val.startswith('0x') else int(val)

# 2) 解析 Pin 数组：{rva, bytes[16]}
pins = {}
for m in re.finditer(r'inline constexpr Pin (\w+)\[\]\s*=\s*\{(.*?)\};', hpp, re.S):
    name, body = m.group(1), m.group(2)
    entries = []
    for e in re.finditer(r'\{\s*(0x[0-9a-fA-F]+)\s*,\s*\{([^}]*)\}\s*\}', body):
        rva = int(e.group(1), 16)
        bs = ''.join(x.strip().replace('0x', '') for x in e.group(2).split(','))
        entries.append({'rva': rva, 'bytes': bs})
    pins[name] = entries

# 3) 段 -> (名字前缀, 对应 Pin 数组) —— 完全对齐国服 profile.json 的 11 段
segments = {
    'functions':   ([k for k in consts if k in (
        'tick','setDevice','setController','selectMaps','mouseUpdate','keyboardUpdate',
        'escapeRoutePredicate','escapeRouteCall','escapeHandler','uidTextSetter',
        'textPopulateMesh','objectName','stringFromUtf16','switchInputMode',
        'componentTransform','componentGameObject','transformParent','objectSetActive',
        'objectActiveSelf','openPageByName','dailyClaimCallback','dailyRewardTaken','openExpedition')], 'pins'),
    'graphics':    ([k for k in consts if k.startswith('graphics') and k not in ('graphicsFpsSlot',)], 'graphicsPins'),
    'photo':       ([k for k in consts if k.startswith('photo')], 'photoPins'),
    'look':        ([k for k in consts if k.startswith('look')], 'lookPins'),
    'touch':       ([k for k in consts if k.startswith('touch')], 'touchPins'),
    'mapMouse':    ([k for k in consts if k.startswith('map')], 'mapMousePins'),
    'teamLoading': ([k for k in consts if k.startswith('teamLoading')], 'teamLoadingPins'),
    'avatarFade':  ([k for k in consts if k.startswith('avatarFade')], 'avatarFadePins'),
    'avatarSwitch':([k for k in consts if k.startswith('avatarSwitch') or k.startswith('switchTrace')], 'avatarSwitchPins'),
    'regionUnlock':([k for k in consts if k.startswith('regionUnlock')], 'regionUnlockPins'),
}

# Pin 数组按 rva 建索引，给地址常量补 bytes
pin_by_rva = {}
for arr in pins.values():
    for p in arr:
        pin_by_rva[p['rva']] = p['bytes']

out = {
    'version': '7.2.0_GenshinImpact_International',
    'arch': 'arm64-v8a',
    '_source': 'app/src/main/cpp/profile_os.hpp (international version)',
}
for seg, (names, pinarr) in segments.items():
    items = []
    for name in names:
        rva = consts[name]
        if rva == 0:  # 未定位的地址跳过，与国服只列有效地址一致
            continue
        bs = pin_by_rva.get(rva, '')
        items.append({'name': name, 'rva': rva, 'bytes': bs})
    out[seg] = items

# data 段：非函数数据地址
data = {}
for k in ('singletonRoot','graphicsFpsSlot','vulkanResolverSlot','engineOutputScreen','readyFlag'):
    if k in consts: data[k] = consts[k]
out['data'] = data

p = ROOT/'os_profile.json'
p.write_text(json.dumps(out, indent=2, ensure_ascii=False) + '\n')
print('wrote', p)
for k, v in out.items():
    print(f'  {k}: {len(v) if isinstance(v,(list,dict)) else v}')
