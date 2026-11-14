from pathlib import Path
import hashlib,json
ROOT=Path(__file__).resolve().parents[1]
research=ROOT.parent/'android-input-research'
library=research/'libyuanshen.so'
if not library.exists():
 if (ROOT/'app/src/main/cpp/profile.hpp').is_file() and (ROOT/'profile.json').is_file():
  print('Using the checked-in game profile; no local research library is present')
  raise SystemExit(0)
 raise FileNotFoundError('Provide the matching game library or the checked-in profile.hpp and profile.json')
targets={'tick':0xe1e63d8,'setDevice':0xe1e2d04,'setController':0xd104b18,'selectMaps':0xe25e560,'mouseUpdate':0xd12b960,'keyboardUpdate':0xd38b7b0,'escapeRoutePredicate':0xe1d414c,'escapeRouteCall':0xe2df338,'escapeHandler':0xe30ffa8}
targets.update({'uidTextSetter':0xcc4f230,'textPopulateMesh':0xcc4fbe4,'objectName':0x8ffe494,'stringFromUtf16':0x8ed2e94})
targets['switchInputMode']=0xe1cb1fc
targets.update({'componentTransform':0x80e5930,'componentGameObject':0x80e5b50,'transformParent':0x8fffc18,'objectSetActive':0x95779f8,'objectActiveSelf':0x95779fc})
targets.update({'openPageByName':0xfdba624})
rows=[]
with library.open('rb') as f:
 for name,rva in targets.items():
  f.seek(rva);raw=f.read(16);rows.append({'name':name,'rva':rva,'bytes':raw.hex()})
text='#pragma once\n#include <cstdint>\nnamespace profile {\n'
text+='inline constexpr char version[]="7.2.0_48751704_49008628";\n'
text+='inline constexpr uintptr_t singletonRoot=0x158095a0;\n'
for row in rows:
 text+=f'inline constexpr uintptr_t {row["name"]}=0x{row["rva"]:x};\n'
text+='struct Pin { uintptr_t rva; unsigned char bytes[16]; };\ninline constexpr Pin pins[]={\n'
for row in rows:text+='{0x%x,{%s}},\n'%(row['rva'],','.join('0x'+row['bytes'][i:i+2] for i in range(0,32,2)))
text+='};\n}\n'
p=ROOT/'app/src/main/cpp/profile.hpp';p.parent.mkdir(parents=True,exist_ok=True);p.write_text(text)
(ROOT/'profile.json').write_text(json.dumps({'version':'7.2.0_48751704_49008628','arch':'arm64-v8a','functions':rows},indent=2))
print('Generated version-specific profile')
