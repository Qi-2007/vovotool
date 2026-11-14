# 随身合成台

适配国服 `com.miHoYo.Yuanshen`，`7.2.0_48751704_49008628`，ARM64。模块版本 0.1.13 / versionCode 14。

## 入口

参考 [Fufu Hooks.cpp](https://github.com/FufuLauncher/FufuLauncher.UnlockerIsland/blob/ccc78e452e8534460d74fbc47e3ed1203d77da4c/FufuLauncher.UnlockerIsland/Core/Hooks.cpp) 的 `RequestOpenCraft` / `g_RequestCraft` 处理及 [SharedState.h](https://github.com/FufuLauncher/FufuLauncher.UnlockerIsland/blob/ccc78e452e8534460d74fbc47e3ed1203d77da4c/FufuLauncher.UnlockerIsland/Core/SharedState.h) 的 `tCraftPartner` 签名：创建 `SynthesisPage` 托管字符串，将其作为第一个参数传给原生页面分发入口，其他四个参数为空。

用户通过场景合成台打开一次原生页面后，读取当前 UI 栈确认页面上下文为 `PHOPLACCBHL`，描述符 `DJJPPIKOCAC` 的页面名为 `SynthesisPage`，资源路径为 `ART/UI/Menus/Page/SynthesisPage`。构造函数为 `0x106e37f0`。沿构造函数唯一的直接调用点 `0xfdbedbc`，结合 ELF unwind 表定位到大型页面名分发函数起点 `0xfdba624`。

该函数接收五个指针参数，按页面名字符串选择原生上下文；Synthesis 分支创建上述合成上下文，场景交互对象为空时使用原函数自己的空参数分支。实现直接调用此原函数，未复制页面构造、配方、材料检查或提交合成逻辑，也不修改游戏资源与分区。

最终代码不保留诊断阶段的 `showContextHook`、方法枚举或固定元数据地址。新增的唯一游戏入口地址 `openPageByName=0xfdba624` 纳入 16 字节指纹校验，继续使用已验证的托管 UTF-16 字符串构造入口。

## 触发与状态

- 模块开关 `portable_craft` 默认开启。默认快捷键为安卓 `KEYCODE_F8`（138），可在模块应用点击「设置快捷键」后录制普通按键或 Ctrl、Alt、Shift、Meta 组合键。Esc/Back 取消录制，切到其他应用同样取消。
- 按键及修饰键分别保存为 `craft_key` / `craft_meta`，经配置 Provider 的现有轮询约 1 秒实时加载。组合键匹配规范化后的修饰键；按住不重复请求，释放主键时即使修饰键已释放，也能正确清除按住状态。
- Java 层继续检查游戏 Activity、窗口焦点和原生文本编辑控件。快捷键 Down 边沿排队，Up 及失焦清除按住状态。
- 请求只在游戏 Tick 中执行。要求模块启用、输入状态为已应用、游戏有焦点、镜头模式 `cursor=-1`、顶层描述符页面为 `InLevelMainPage`。其他菜单/对话页面拒绝请求，不延后到恢复焦点时自动弹出。
- 请求去重 800 ms，调用间隔至少 1200 ms，陈旧请求 1500 ms 后拒绝。原函数接受请求后等待 UI 栈出现 `SynthesisPage`，最多等待 8 秒；成功后菜单指针沿原捕获逻辑释放，退出后恢复镜头捕获。
- 配置开关、状态文字和诊断均接入现有配置 Provider，不增加对外任意页面调用接口。

`craftState=[状态,开关,请求数,调用数,已打开数,拒绝数]`。状态 0 就绪，1 待执行，2 等待页面，3 上次成功打开，-1 当前页面/焦点/冷却条件不符，-2 原生入口未接受，-3 等待超时。既有键鼠、UID 和焦点诊断数组长度不变。

## 验证

2026-11-14，Lenovo TB710FU / Android 15 / LSPosed 2.2.1（7941）。重新启动加载 0.1.12 后，在未通过场景合成台打开页面的该游戏进程 PID 29494 中验证：

1. F8 成功打开完整原生合成界面，`craftState=[3,1,1,1,1,0]`。
2. 合成界面内再次按 F8 被拒绝，调用数仍为 1，`craftState=[-1,1,2,1,1,1]`，未堆叠页面。
3. Esc 退出后回到镜头模式 `cursor=-1`；再次 F8 打开成功，`craftState=[3,1,3,2,2,1]`。
4. 再次 Esc 退出，游戏进程不变，键鼠、UID 和触控板辅助进程保持工作。

构建、APK v2/v3 签名检查通过。主机自动测试只操作打开/关闭页面，没有提交消耗材料的合成。用户随后针对远离场景合成台、实体键盘及自行选择配方的验证请求回复“测试正常”；战斗、飞行、联机和各类秘境尚未逐一验证。

验证数据位于 Git 忽略的 `build/verified-portable-craft-0.1.12.json`，截图为 `build/portable-craft-first.png`。

## 0.1.13 自定义快捷键验证

0.1.13 已安装并重新启动游戏加载。模块界面已通过实体键盘保存快捷键 `9`；新版进程的合成页面打开计数已增长，截图可见完整原生合成界面。用户随后确认“我已确认 9 正常，无需继续测试”，因此结束设备输入测试，保留 `9`，未继续变更组合键或测试旧 F8。组合键支持已编译及检查匹配逻辑，尚未完成实体组合键的实机验证。

配置保存后无需重启游戏；更新模块 APK 后仍需重新启动游戏加载新版代码。默认 F8 仅用于没有自定义设置的安装。APK 签名、16 KB 库对齐和新增 JNI 符号检查通过；证据记录为 `build/verified-shortcut-0.1.13.json`。

输出 `dist/vovotool-0.1.13-arm64.apk`，SHA-256：`503d21ce615af55fedc0cf7d274b1a2752ccb4f9ba6c32358b578150e8d2b220`。
