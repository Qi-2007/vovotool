# 沃沃头 0.1.7 更名验证

2026-11-14，项目由 `D:\code\gi\gi-lsposed` 迁至 `D:\code\gi\vovotool`，初始化为独立本地 Git 仓库。

- 应用显示名称：沃沃头。
- 项目名与 APK 前缀：`vovotool`。
- 应用包名与 Java 命名空间：`com.qi.vovotool`。
- 配置 Provider：`com.qi.vovotool.config`。
- LSPosed Java 入口：`com.qi.vovotool.XposedEntry`。
- 原生加载入口继续使用 `libgipc.so`，全部九个 JNI 导出已改为 `Java_com_qi_vovotool_NativeBridge_*`。
- Android 版本名 `0.1.7`，版本号 8；原神作用域和目标游戏版本保持原适配值。

重新构建通过 javac、D8、NDK clang++、aapt2、zipalign 与 apksigner。使用 aapt2 检查 APK 的应用名、包名和 Provider；检查 `classes.jar` 及 DEX 无旧包名类；检查动态符号表全部 JNI 导出匹配新命名空间；APK v2/v3 签名及 16KB 对齐校验通过。

输出：`dist/vovotool-0.1.7-arm64.apk`，877297 字节。

SHA-256：`f9cec68f1219a81155a298ab6662d70498a0bba06ca4989f8d51380013e18d0c`。

本次没有安装新包或迁移设备数据。新包作为独立应用，需要重新启用 LSPosed 模块及原神作用域；使用触控板辅助时需要新应用的 root 授权。旧应用的设置不会自动转移。原有功能的实机验证记录属于 0.1.6 及更早版本，0.1.7 的更名验证范围为源码、构建产物与包结构。
