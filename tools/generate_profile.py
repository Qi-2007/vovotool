from pathlib import Path
import hashlib,json,sys
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
targets.update({'openPageByName':0xfdba624,'dailyClaimCallback':0xe1293b8,'dailyRewardTaken':0xe124320})
targets['openExpedition']=0x83efec4
graphics_targets={'graphicsFrameRate':0x4a807c0,'graphicsQualityFloat':0xfa71090,'graphicsVulkanInitialize':0x55ce2b8,'graphicsVulkanResolverLoad':0x55ce2f0,'graphicsScreenWidth':0x44388c0,'graphicsScreenHeight':0x44d829c}
graphics_targets.update({'graphicsMainCamera':0xc806d20,'graphicsRebuildScene':0x947982c,'graphicsPostProcessUpdate':0x947a8b8})
graphics_targets['graphicsPrepareScene']=0x947c074
graphics_targets['graphicsSetResolution']=0x4a8f998
photo_targets={'photoRead':0x4aa3f84,'photoResize':0x4aa3eac}
look_targets={'lookRefresh':0xe1cb71c,'lookHorizontal':0xe1d35b8,'lookVertical':0xe1d7ccc,'lookSpeed':0xe1d2d64,'lookAxisSetting':0xe1cb88c,'lookSourceLayout':0xe1cb7d8,'lookSpeedDefault':0xe1d8ce8}
touch_targets={'touchRotate':0xcc6f6c8}
map_mouse_targets={'mapUpdateTouches':0x97dad60,'mapMouseHeld':0x4a92efc,'mapMouseDown':0x4a92f3c,'mapMouseUp':0x4a92f70}
team_loading_targets={'teamLoadingMoveNext':0x128f4590,'teamLoadingLayout':0x128f465c,'teamLoadingReady':0xe81c0cc}
avatar_fade_targets={'avatarFadeSetter':0xa3297e0,'avatarFadeLayout':0xa3297f4,'avatarFadeBinding':0xa0a1edc,'avatarFadeCamera':0xa0993f8,'avatarFadeInline':0xa0994b8,'avatarFadeRecalculate':0xa329828,'avatarFadeCombine':0xa329840}
avatar_switch_targets={'avatarSwitchCheck':0xbb7a550,'avatarSwitchFlag':0xbb7a7f0,'avatarSwitchCooldown':0xbb7a934,'avatarSwitchMessage':0xbb4a72c,'avatarSwitchRequest':0xce0e610,'avatarSwitchRate':0xce0e7f4,'avatarSwitchReady':0xce10bf4,'avatarSwitchReadyLayout':0xce10c0c,'switchTraceUi':0xe1ecacc,'switchTraceData':0xce439b8,'switchTraceCurrent':0x7e67f20,'switchTraceTarget':0x7e7e434,'switchTraceSend':0xecab420,'switchTraceToast':0x7fb7ff8,'switchTraceWarning':0xe42c8fc,'switchTraceInfo':0xe419998,'switchTraceInput':0xce2205c,'switchTraceResponse':0xcdf3ac4,'switchTraceCooldown':0xce0ff34}
region_unlock_targets={'regionUnlockSetLocal':0xce1aa5c,'regionUnlockCacheSetter':0xe802b98}
rows=[]
with library.open('rb') as f:
 for name,rva in targets.items():
  f.seek(rva);raw=f.read(16);rows.append({'name':name,'rva':rva,'bytes':raw.hex()})
text='#pragma once\n#include <cstdint>\nnamespace profile {\n'
text+='inline constexpr char version[]="7.2.0_48751704_49008628";\n'
text+='inline constexpr uintptr_t singletonRoot=0x158095a0;\n'
for row in rows:
 text+=f'inline constexpr uintptr_t {row["name"]}=0x{row["rva"]:x};\n'
graphics=[]
with library.open('rb') as f:
 for name,rva in graphics_targets.items():
  f.seek(rva);graphics.append({'name':name,'rva':rva,'bytes':f.read(16).hex()})
for row in graphics:text+=f'inline constexpr uintptr_t {row["name"]}=0x{row["rva"]:x};\n'
photos=[]
with library.open('rb') as f:
 for name,rva in photo_targets.items():
  f.seek(rva);photos.append({'name':name,'rva':rva,'bytes':f.read(16).hex()})
for row in photos:text+=f'inline constexpr uintptr_t {row["name"]}=0x{row["rva"]:x};\n'
looks=[]
with library.open('rb') as f:
 for name,rva in look_targets.items():
  f.seek(rva);looks.append({'name':name,'rva':rva,'bytes':f.read(16).hex()})
for row in looks:text+=f'inline constexpr uintptr_t {row["name"]}=0x{row["rva"]:x};\n'
touches=[]
with library.open('rb') as f:
 for name,rva in touch_targets.items():
  f.seek(rva);touches.append({'name':name,'rva':rva,'bytes':f.read(16).hex()})
for row in touches:text+=f'inline constexpr uintptr_t {row["name"]}=0x{row["rva"]:x};\n'
map_mouse=[]
with library.open('rb') as f:
 for name,rva in map_mouse_targets.items():
  f.seek(rva);map_mouse.append({'name':name,'rva':rva,'bytes':f.read(16).hex()})
for row in map_mouse:text+=f'inline constexpr uintptr_t {row["name"]}=0x{row["rva"]:x};\n'
teams=[]
with library.open('rb') as f:
 for name,rva in team_loading_targets.items():
  f.seek(rva);teams.append({'name':name,'rva':rva,'bytes':f.read(16).hex()})
for row in teams:text+=f'inline constexpr uintptr_t {row["name"]}=0x{row["rva"]:x};\n'
fades=[]
with library.open('rb') as f:
 for name,rva in avatar_fade_targets.items():
  f.seek(rva);fades.append({'name':name,'rva':rva,'bytes':f.read(16).hex()})
for row in fades:text+=f'inline constexpr uintptr_t {row["name"]}=0x{row["rva"]:x};\n'
switches=[]
with library.open('rb') as f:
 for name,rva in avatar_switch_targets.items():
  f.seek(rva);switches.append({'name':name,'rva':rva,'bytes':f.read(16).hex()})
for row in switches:text+=f'inline constexpr uintptr_t {row["name"]}=0x{row["rva"]:x};\n'
regions=[]
with library.open('rb') as f:
 for name,rva in region_unlock_targets.items():
  f.seek(rva);regions.append({'name':name,'rva':rva,'bytes':f.read(16).hex()})
for row in regions:text+=f'inline constexpr uintptr_t {row["name"]}=0x{row["rva"]:x};\n'
text+='struct Pin { uintptr_t rva; unsigned char bytes[16]; };\ninline constexpr Pin pins[]={\n'
for row in rows:text+='{0x%x,{%s}},\n'%(row['rva'],','.join('0x'+row['bytes'][i:i+2] for i in range(0,32,2)))
text+='};\ninline constexpr Pin graphicsPins[]={\n'
for row in graphics:text+='{0x%x,{%s}},\n'%(row['rva'],','.join('0x'+row['bytes'][i:i+2] for i in range(0,32,2)))
text+='};\ninline constexpr Pin photoPins[]={\n'
for row in photos:text+='{0x%x,{%s}},\n'%(row['rva'],','.join('0x'+row['bytes'][i:i+2] for i in range(0,32,2)))
text+='};\ninline constexpr Pin lookPins[]={\n'
for row in looks:text+='{0x%x,{%s}},\n'%(row['rva'],','.join('0x'+row['bytes'][i:i+2] for i in range(0,32,2)))
text+='};\ninline constexpr Pin touchPins[]={\n'
for row in touches:text+='{0x%x,{%s}},\n'%(row['rva'],','.join('0x'+row['bytes'][i:i+2] for i in range(0,32,2)))
text+='};\ninline constexpr Pin mapMousePins[]={\n'
for row in map_mouse:text+='{0x%x,{%s}},\n'%(row['rva'],','.join('0x'+row['bytes'][i:i+2] for i in range(0,32,2)))
text+='};\ninline constexpr Pin teamLoadingPins[]={\n'
for row in teams:text+='{0x%x,{%s}},\n'%(row['rva'],','.join('0x'+row['bytes'][i:i+2] for i in range(0,32,2)))
text+='};\ninline constexpr Pin avatarFadePins[]={\n'
for row in fades:text+='{0x%x,{%s}},\n'%(row['rva'],','.join('0x'+row['bytes'][i:i+2] for i in range(0,32,2)))
text+='};\ninline constexpr Pin avatarSwitchPins[]={\n'
for row in switches:text+='{0x%x,{%s}},\n'%(row['rva'],','.join('0x'+row['bytes'][i:i+2] for i in range(0,32,2)))
text+='};\ninline constexpr Pin regionUnlockPins[]={\n'
for row in regions:text+='{0x%x,{%s}},\n'%(row['rva'],','.join('0x'+row['bytes'][i:i+2] for i in range(0,32,2)))
text+='};\n}\n'
p=ROOT/'app/src/main/cpp/profile.hpp'
if p.is_file() and '--force' not in sys.argv:
    sys.exit('Refusing to overwrite the checked-in profile.hpp without --force. '
             'This generator uses a built-in address table that may be stale; '
             'the checked-in profile.hpp is authoritative for builds. '
             'Pass --force only when intentionally retargeting a new game version.')
p.parent.mkdir(parents=True,exist_ok=True);p.write_text(text)
(ROOT/'profile.json').write_text(json.dumps({'version':'7.2.0_48751704_49008628','arch':'arm64-v8a','functions':rows,'graphics':graphics,'photo':photos,'look':looks,'touch':touches,'mapMouse':map_mouse,'teamLoading':teams,'avatarFade':fades,'avatarSwitch':switches,'regionUnlock':regions},indent=2))
print('Generated version-specific profile')
