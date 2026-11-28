"""Build the APK with portable JDK/Android tools; no global SDK or Gradle cache required."""
from pathlib import Path
import hashlib,json,os,shutil,subprocess,sys,zipfile,xml.etree.ElementTree as ET
ROOT=Path(__file__).resolve().parents[1];TC=ROOT.parent/'toolchain';BUILD=ROOT/'build';OUT=ROOT/'dist'
BUILD.mkdir(exist_ok=True);OUT.mkdir(exist_ok=True)
manifest=ET.parse(ROOT/'app/src/main/AndroidManifest.xml').getroot()
APP_PACKAGE=manifest.attrib['package'];VERSION=manifest.attrib['{http://schemas.android.com/apk/res/android}versionName']
JDK=next((TC/'jdk-21').glob('*/bin/java.exe')).parent
TOOLS=TC/'build-tools-35/android-15';ANDROID=TC/'platform-35/android-35/android.jar'
LLVM=TC/'ndk-27/android-ndk-r27c/toolchains/llvm/prebuilt/windows-x86_64/bin'
env=os.environ.copy();env['JAVA_HOME']=str(JDK.parent);env['PATH']=str(JDK)+os.pathsep+env['PATH'];env['JAVA_TOOL_OPTIONS']='-Dfile.encoding=UTF-8 -Duser.language=en -Duser.country=US'
def run(args):
 print('Running',Path(str(args[0])).name,flush=True)
 subprocess.run([str(a) for a in args],check=True,env=env,cwd=ROOT)
def clean_output_directory(path):
 if path.resolve().parent!=BUILD.resolve():raise RuntimeError('Refusing to clean outside the build directory')
 if path.exists():shutil.rmtree(path)
 path.mkdir()
# profile.hpp is checked in and verified; do NOT regenerate it during build.
# generate_profile.py reads a local research library when present and would
# overwrite profile.hpp with a stale address table (missing newer fields such
# as graphicsFpsSlot / readyFlag / pinsCount). It is a manual retargeting tool:
# run it explicitly only when intentionally adapting to a new game version.
native=BUILD/'lib/arm64-v8a';native.mkdir(parents=True,exist_ok=True)
common=[LLVM/'clang++.exe','--target=aarch64-linux-android26','-std=c++17','-O2','-Wall','-Wextra','-fvisibility=hidden','-static-libstdc++','-Wl,-z,max-page-size=16384']
run(common+['-fPIC','-shared',ROOT/'app/src/main/cpp/module.cpp','-llog','-ldl','-o',native/'libgipc.so'])
run(common+['-fPIE','-pie',ROOT/'app/src/main/cpp/touchpad_proxy.cpp','-o',native/'libtouchpad_proxy.so'])
run([LLVM/'llvm-strip.exe','--strip-unneeded',native/'libgipc.so',native/'libtouchpad_proxy.so'])
resources=BUILD/'resources.zip'
run([TOOLS/'aapt2.exe','compile','--dir',ROOT/'app/src/main/res','-o',resources])
unsigned=BUILD/'unsigned.apk'
run([TOOLS/'aapt2.exe','link','-o',unsigned,'-I',ANDROID,'--manifest',ROOT/'app/src/main/AndroidManifest.xml',resources])
classes=BUILD/'classes';clean_output_directory(classes)
source=sorted((ROOT/'app/src/main/java').rglob('*.java'))
args=BUILD/'javac-args.txt';args.write_text('\n'.join('"'+str(p).replace('\\','/')+'"' for p in source),encoding='utf8')
run([JDK/'javac.exe','-encoding','UTF-8','-source','8','-target','8','-Xlint:-options','-bootclasspath',str(ANDROID)+os.pathsep+str(TOOLS/'core-lambda-stubs.jar'),'-classpath',ROOT/'app/libs/api-82.jar','-d',classes,'@'+str(args)])
jar=BUILD/'classes.jar'
with zipfile.ZipFile(jar,'w') as z:
 for path in classes.rglob('*.class'):z.write(path,path.relative_to(classes).as_posix())
with zipfile.ZipFile(jar) as z:
 namespace=APP_PACKAGE.replace('.','/')+'/'
 assert namespace+'MainActivity.class' in z.namelist()
 assert all(name.startswith(namespace) for name in z.namelist()),'Stale Java classes from another package'
dex=BUILD/'dex';clean_output_directory(dex)
run([JDK/'java.exe','-cp',TOOLS/'lib/d8.jar','com.android.tools.r8.D8','--min-api','26','--lib',ANDROID,'--classpath',ROOT/'app/libs/api-82.jar','--output',dex,jar])
with zipfile.ZipFile(unsigned,'a') as z:
 for path in dex.glob('*.dex'):z.write(path,path.name)
 for path in (ROOT/'app/src/main/assets').rglob('*'):
  if path.is_file():z.write(path,'assets/'+path.relative_to(ROOT/'app/src/main/assets').as_posix())
 for path in native.glob('*.so'):z.write(path,'lib/arm64-v8a/'+path.name,compress_type=zipfile.ZIP_STORED)
aligned=BUILD/'aligned.apk'
run([TOOLS/'zipalign.exe','-f','-P','16','4',unsigned,aligned])
key=ROOT/'signing/debug.keystore';key.parent.mkdir(exist_ok=True)
if not key.exists():run([JDK/'keytool.exe','-genkeypair','-keystore',key,'-storepass','android','-keypass','android','-alias','androiddebugkey','-keyalg','RSA','-keysize','2048','-validity','3650','-dname','CN=vovotool Development'])
apk=OUT/f'vovotool-{VERSION}-arm64.apk'
run([JDK/'java.exe','-jar',TOOLS/'lib/apksigner.jar','sign','--ks',key,'--ks-pass','pass:android','--key-pass','pass:android','--out',apk,aligned])
run([JDK/'java.exe','-jar',TOOLS/'lib/apksigner.jar','verify','--verbose',apk])
with zipfile.ZipFile(apk) as z:
 assert z.read('assets/xposed_init').decode().strip()==APP_PACKAGE+'.XposedEntry'
 assert z.read('assets/native_init').decode().strip()=='libgipc.so'
 assert len(z.read('lib/arm64-v8a/libtouchpad_proxy.so'))>0
print(json.dumps({'apk':str(apk),'bytes':apk.stat().st_size,'sha256':hashlib.file_digest(apk.open('rb'),'sha256').hexdigest()},indent=2),flush=True)
