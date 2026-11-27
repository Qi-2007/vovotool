"""Linux 版构建脚本：与 tools/build.py 流程一致，只换了工具链路径与可执行文件后缀。

作者原版只支持 Windows（硬编码 .exe / windows-x86_64 的 NDK）。本脚本不改动原文件，
供在 Linux 上验证 Java 链路或用 Linux NDK 出完整 APK。

用法：
    python3 tools/build_linux.py              # 完整构建（需要 NDK）
    python3 tools/build_linux.py --skip-native # 只构建 Java + 资源 + 签名（不需要 NDK）

工具链目录默认 ~/work/toolchain-linux，结构：
    android-15/                    build-tools 35（Linux）
    android-35/android.jar         platform 35
    api-82.jar                     LSPosed API
    android-ndk-r27c/              NDK（可选）
可用环境变量 VOVOTOOL_TC 覆盖。
"""
from pathlib import Path
import hashlib
import json
import os
import shutil
import subprocess
import sys
import zipfile
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
TC = Path(os.environ.get("VOVOTOOL_TC", Path.home() / "work/toolchain-linux"))
BUILD = ROOT / "build"
OUT = ROOT / "dist"
SKIP_NATIVE = "--skip-native" in sys.argv

BUILD.mkdir(exist_ok=True)
OUT.mkdir(exist_ok=True)

manifest = ET.parse(ROOT / "app/src/main/AndroidManifest.xml").getroot()
APP_PACKAGE = manifest.attrib["package"]
VERSION = manifest.attrib["{http://schemas.android.com/apk/res/android}versionName"]

TOOLS = TC / "android-15"
ANDROID = TC / "android-35/android.jar"
API_JAR = TC / "api-82.jar"
LLVM = TC / "android-ndk-r27c/toolchains/llvm/prebuilt/linux-x86_64/bin"

JDK = Path(shutil.which("java") or sys.exit("找不到 java")).parent
JAVAC = JDK / "javac"
JAVA = JDK / "java"
KEYTOOL = JDK / "keytool"

env = os.environ.copy()
env["JAVA_HOME"] = str(JDK.parent)
env["JAVA_TOOL_OPTIONS"] = "-Dfile.encoding=UTF-8 -Duser.language=en -Duser.country=US"


def run(args):
    print("Running", Path(str(args[0])).name, flush=True)
    subprocess.run([str(a) for a in args], check=True, env=env, cwd=ROOT)


def clean_output_directory(path):
    if path.resolve().parent != BUILD.resolve():
        raise RuntimeError("Refusing to clean outside the build directory")
    if path.exists():
        shutil.rmtree(path)
    path.mkdir()


for p, what in ((TOOLS, "build-tools 35"), (ANDROID, "platform-35 android.jar"), (API_JAR, "LSPosed api-82.jar")):
    if not Path(p).exists():
        sys.exit(f"缺少 {what}: {p}")

run([sys.executable, ROOT / "tools/generate_profile.py"])

native = BUILD / "lib/arm64-v8a"
native.mkdir(parents=True, exist_ok=True)
if SKIP_NATIVE:
    print("跳过 native（--skip-native），APK 里不会有 .so", flush=True)
else:
    if not (LLVM / "clang++").exists():
        sys.exit(f"缺少 NDK: {LLVM}；先装 android-ndk-r27c 或不加 --skip-native 重跑")
    common = [LLVM / "clang++", "--target=aarch64-linux-android26", "-std=c++17", "-O2",
              "-Wall", "-Wextra", "-fvisibility=hidden", "-static-libstdc++",
              "-Wl,-z,max-page-size=16384"]
    # 国际服构建：换用 profile_os.hpp 里的地址表
    for i,a in enumerate(sys.argv):
        if a=="--define" and i+1<len(sys.argv):
            common.append(f"-D{sys.argv[i+1]}")
    if "--no-hooks" in sys.argv:
        common += ["-DVOVO_NO_HOOKS"]
        print("诊断模式：不装任何钩子", flush=True)
    if "--os" in sys.argv:
        common += ["-DVOVO_OS_BUILD"]
        print("国际服模式：链接 profile_os.hpp", flush=True)
    run(common + ["-fPIC", "-shared", ROOT / "app/src/main/cpp/module.cpp", "-llog", "-ldl",
                  "-o", native / "libgipc.so"])
    run(common + ["-fPIE", "-pie", ROOT / "app/src/main/cpp/touchpad_proxy.cpp",
                  "-o", native / "libtouchpad_proxy.so"])
    run([LLVM / "llvm-strip", "--strip-unneeded",
         native / "libgipc.so", native / "libtouchpad_proxy.so"])

resources = BUILD / "resources.zip"
run([TOOLS / "aapt2", "compile", "--dir", ROOT / "app/src/main/res", "-o", resources])
unsigned = BUILD / "unsigned.apk"
run([TOOLS / "aapt2", "link", "-o", unsigned, "-I", ANDROID,
     "--manifest", ROOT / "app/src/main/AndroidManifest.xml", resources])

classes = BUILD / "classes"
clean_output_directory(classes)
source = sorted((ROOT / "app/src/main/java").rglob("*.java"))
args_file = BUILD / "javac-args.txt"
args_file.write_text("\n".join('"' + str(p) + '"' for p in source), encoding="utf8")
run([JAVAC, "-encoding", "UTF-8", "-source", "8", "-target", "8", "-Xlint:-options",
     "-bootclasspath", f"{ANDROID}{os.pathsep}{TOOLS/'core-lambda-stubs.jar'}",
     "-classpath", API_JAR, "-d", classes, "@" + str(args_file)])

jar = BUILD / "classes.jar"
with zipfile.ZipFile(jar, "w") as z:
    for path in classes.rglob("*.class"):
        z.write(path, path.relative_to(classes).as_posix())
with zipfile.ZipFile(jar) as z:
    namespace = APP_PACKAGE.replace(".", "/") + "/"
    assert namespace + "MainActivity.class" in z.namelist()
    assert all(name.startswith(namespace) for name in z.namelist()), "Stale Java classes from another package"

dex = BUILD / "dex"
clean_output_directory(dex)
run([JAVA, "-cp", TOOLS / "lib/d8.jar", "com.android.tools.r8.D8", "--min-api", "26",
     "--lib", ANDROID, "--classpath", API_JAR, "--output", dex, jar])

with zipfile.ZipFile(unsigned, "a") as z:
    for path in dex.glob("*.dex"):
        z.write(path, path.name)
    for path in (ROOT / "app/src/main/assets").rglob("*"):
        if path.is_file():
            z.write(path, "assets/" + path.relative_to(ROOT / "app/src/main/assets").as_posix())
    for path in native.glob("*.so"):
        z.write(path, "lib/arm64-v8a/" + path.name, compress_type=zipfile.ZIP_STORED)

aligned = BUILD / "aligned.apk"
run([TOOLS / "zipalign", "-f", "-P", "16", "4", unsigned, aligned])

key = ROOT / "signing/debug.keystore"
key.parent.mkdir(exist_ok=True)
if not key.exists():
    run([KEYTOOL, "-genkeypair", "-keystore", key, "-storepass", "android", "-keypass", "android",
         "-alias", "androiddebugkey", "-keyalg", "RSA", "-keysize", "2048", "-validity", "3650",
         "-dname", "CN=vovotool Development"])

apk = OUT / f"vovotool-{VERSION}-arm64-linuxbuild.apk"
run([JAVA, "-jar", TOOLS / "lib/apksigner.jar", "sign", "--ks", key, "--ks-pass", "pass:android",
     "--key-pass", "pass:android", "--out", apk, aligned])
run([JAVA, "-jar", TOOLS / "lib/apksigner.jar", "verify", "--verbose", apk])

with zipfile.ZipFile(apk) as z:
    assert z.read("assets/xposed_init").decode().strip() == APP_PACKAGE + ".XposedEntry"
    assert z.read("assets/native_init").decode().strip() == "libgipc.so"
    if not SKIP_NATIVE:
        assert len(z.read("lib/arm64-v8a/libtouchpad_proxy.so")) > 0

print(json.dumps({
    "apk": str(apk),
    "bytes": apk.stat().st_size,
    "sha256": hashlib.file_digest(apk.open("rb"), "sha256").hexdigest(),
    "native": "skipped" if SKIP_NATIVE else "included",
}, indent=2), flush=True)
