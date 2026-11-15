"""Collect the module's passive Vulkan burst; no pixels, accounts or tokens.

Run from the repo: py tools/render_probe.py build/render-probe-old.json
Path selection is diagnostic only (0=game, 1=legacy Vulkan, 2=experimental,
3=native Screen resolution plus Vulkan scene sizing; current default).
Changing it requires a game restart; this collector never changes settings.
"""
import collections,json,subprocess,sys,time
from pathlib import Path
ADB=Path(r'D:\Program Files\platform-tools\adb.exe')
def call(method):
    return subprocess.check_output([str(ADB),'shell','su','-c',f'content call --uri content://com.qi.vovotool.config --method {method}'],text=True,encoding='utf8')
def describe(data):
    print('request',data.get('request'),'hookMask',data.get('hookMask'))
    for index,f in enumerate(data.get('frames',[])):
        print('frame',index,{k:f[k] for k in ('submits','commandBuffers','missingCommandBuffers','dropped')},'passes',len(f['passes']),'presented',[(i.get('width'),i.get('height')) for i in f['presented']])
    if not data.get('frames'):return
    for index,p in enumerate(data['frames'][0]['passes']):
        if not p['draws']:continue
        rasters=p['raster'];geo=[r for r in rasters if r['pipelineKnown'] and r['depthTest']]
        dims=collections.Counter()
        for r in rasters:dims[tuple(r['viewport'])]+=r['calls']
        image=lambda i:(i.get('width'),i.get('height'),i.get('format'))
        source=[image(i) for i in p['sampledInputs'] if i.get('width',0)>=320 and .9<((i.get('width',0)/max(i.get('height',1),1))/1.6)<1.1]
        print(index,'draws',p['draws'],'depth',sum(r['calls'] for r in geo),'area',p['area'],'attachments',[image(i) for i in p['attachments']],'vp',dims.most_common(3),'landscape inputs',sorted(set(source)))
if __name__=='__main__':
    out=Path(sys.argv[1]);response=call('capture_render');print(response.strip())
    token=int(response.split('token=',1)[1].split('}',1)[0])
    deadline=time.monotonic()+25
    while time.monotonic()<deadline:
        time.sleep(.5);raw=call('render_probe');start=raw.find('probe=')
        if start<0:raise RuntimeError(raw)
        data=json.JSONDecoder().raw_decode(raw[start+6:])[0]
        if data and data.get('request')==token:
            out.write_text(json.dumps(data,indent=2),encoding='utf8');print('saved',out,'bytes',out.stat().st_size);describe(data);break
    else:raise TimeoutError('No completed capture; game must be presenting frames with module connected')
