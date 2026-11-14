"""Download official portable build tools to D: and verify Google package checksums."""
from pathlib import Path
import concurrent.futures,hashlib,json,time,urllib.request,xml.etree.ElementTree as ET,zipfile
ROOT=Path(__file__).resolve().parents[1]
CACHE=ROOT.parent/'downloads';DEST=ROOT.parent/'toolchain'
CACHE.mkdir(exist_ok=True);DEST.mkdir(exist_ok=True)
opener=urllib.request.build_opener(urllib.request.ProxyHandler({}))
def download(url,path,sha1=None):
 if path.exists() and (not sha1 or hashlib.file_digest(path.open('rb'),'sha1').hexdigest()==sha1):return
 started=time.monotonic()
 with opener.open(url,timeout=40) as src,path.with_suffix(path.suffix+'.part').open('wb') as out:
  while data:=src.read(1024*1024):out.write(data)
 path.with_suffix(path.suffix+'.part').replace(path)
 if sha1 and hashlib.file_digest(path.open('rb'),'sha1').hexdigest()!=sha1:raise RuntimeError('Checksum mismatch '+str(path))
 print(json.dumps({'downloaded':path.name,'bytes':path.stat().st_size,'seconds':round(time.monotonic()-started)}),flush=True)
def package(key):
 root=ET.parse(CACHE/'repository2-3.xml').getroot()
 p=next(p for p in root.findall('remotePackage') if p.attrib['path']==key)
 a=next(a for a in p.findall('./archives/archive') if a.findtext('host-os') in ('windows',None))
 url='https://dl.google.com/android/repository/'+a.findtext('complete/url');sha=a.findtext('complete/checksum')
 path=CACHE/url.rsplit('/',1)[1];download(url,path,sha)
 target={'build-tools;35.0.0':'build-tools-35','platforms;android-35':'platform-35','ndk;27.2.12479018':'ndk-27'}[key]
 if not (DEST/target/'.complete').exists():
  (DEST/target).mkdir(exist_ok=True)
  with zipfile.ZipFile(path) as z:z.extractall(DEST/target)
  (DEST/target/'.complete').write_text(sha)
 print('Ready '+target,flush=True)
def jdk():
 path=CACHE/'amazon-corretto-21-windows.zip'
 download('https://corretto.aws/downloads/latest/amazon-corretto-21-x64-windows-jdk.zip',path)
 if not (DEST/'jdk-21/.complete').exists():
  (DEST/'jdk-21').mkdir(exist_ok=True)
  with zipfile.ZipFile(path) as z:z.extractall(DEST/'jdk-21')
  (DEST/'jdk-21/.complete').write_text(hashlib.file_digest(path.open('rb'),'sha256').hexdigest())
 print('Ready jdk-21',flush=True)
def xposed():
 path=ROOT/'app/libs/api-82.jar';path.parent.mkdir(parents=True,exist_ok=True)
 expected='f48c635f1c7469fdec0e00ad2ea0b7a6b2f5b55065784a35b7ca3a84615e8e25'
 if not path.exists():
  download('https://maven.aliyun.com/repository/public/de/robv/android/xposed/api/82/api-82.jar',path)
 if hashlib.file_digest(path.open('rb'),'sha256').hexdigest()!=expected:raise RuntimeError('Xposed compile-only API checksum mismatch')
 print('Ready Xposed API',flush=True)
with concurrent.futures.ThreadPoolExecutor(5) as ex:
 tasks=[ex.submit(package,key) for key in ['build-tools;35.0.0','platforms;android-35','ndk;27.2.12479018']]+[ex.submit(jdk),ex.submit(xposed)]
 for task in concurrent.futures.as_completed(tasks):task.result()
