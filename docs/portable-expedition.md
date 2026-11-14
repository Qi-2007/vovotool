# 随身探索派遣

模块 0.1.16 / versionCode 17，适配国服 `com.miHoYo.Yuanshen`、`7.2.0_48751704_49008628`、ARM64。

## 入口定位

参考 [Fufu Automation.cpp](https://github.com/FufuLauncher/FufuLauncher.UnlockerIsland/blob/master/FufuLauncher.UnlockerIsland/Automation/Automation.cpp) 的 `InitExpedition` / `DoExpeditionLogic` 和 [Patterns.h](https://github.com/FufuLauncher/FufuLauncher.UnlockerIsland/blob/master/FufuLauncher.UnlockerIsland/Patterns/Patterns.h) 的 `ExpHashCmp`。Fufu 在页面分发函数内匹配哈希 `0x699073e1`，定位原探索派遣处理函数，临时调整其早期分支后调用。

用户从凯瑟琳对话打开原生探索派遣页后，通过 root 只读 `/proc/PID/mem` 读取现有 UI 栈，确认：

- 页面名 `ExplorationPage`，FNV-1a 哈希恰为 `0x699073e1`。
- 资源路径 `ART/UI/Menus/Page/ExplorePage`。
- 当前版本上下文类型 `NIOAOKFFFAC`，页面描述符类型 `DJJPPIKOCAC`。

在安卓 ELF 中找到对应哈希比较 `0xfdcb430`，相等分支 `0xfdcece8` 的尾调用指向 `0x83efec4`。此入口接收两个参数：返回动作和地区字符串。地区解析失败后使用游戏自己的默认地区 0，构造原派遣上下文并正常显示页面。

该版本的整数解析函数 `0x7d08984` 明确支持空字符串指针，返回解析失败。模块因此在游戏 Tick 中直接调用原处理函数 `(nullptr, nullptr)`，不需要复制 Fufu 在 Windows 上的临时分支改写。新增 `openExpedition` 地址和 16 字节代码指纹同时加入 profile 生成脚本及已生成 profile。未新增游戏函数 hook、构造假对象或改写派遣逻辑。

## 开关、快捷键及状态

`portable_expedition` 默认开启。默认数字键 7；`expedition_key` / `expedition_meta` 支持普通按键和 Ctrl、Alt、Shift、Meta 组合键，录制入口位于模块应用“随身探索派遣”下。三项快捷功能统一进行冲突校验，保留已有合成台、日委的配置。

Java 层检查窗口焦点及文本输入控件，按下边沿仅排队一次，按住和重复事件不连续触发。原生侧检查主开关、派遣开关、输入状态、窗口焦点、镜头模式和顶层 `InLevelMainPage`；其他菜单或对话不打开。请求陈旧上限 1500 ms，去重 800 ms，调用间隔至少 1200 ms，等待页面最多 8 秒。合成台仍在等待打开时拒绝派遣请求。

`expeditionState=[状态,开关,请求数,调用数,已打开数,拒绝数]`。状态 0 为就绪，1 为待执行，2 为等待页面，3 为已打开；-1 为条件不符，-2 为原生入口拒绝，-3 为等待超时。既有诊断数组长度不变。

模块负责打开原生页面。领取、选择角色、派遣时长及地区切换继续由游戏界面和原逻辑处理。

## 实机验证

2026-11-15，Lenovo TB710FU / Android 15，同一新游戏进程 PID 13063：

1. 新进程未从 NPC 对话打开派遣页时，ADB 数字键 7 成功打开完整探索派遣界面：`[3,1,1,1,1,0]`。
2. 派遣页内再按 7 被拒绝，调用数仍为 1：`[-1,1,2,1,1,1]`。
3. Esc 返回镜头模式；再次按 7 成功打开：`[3,1,3,2,2,1]`。
4. 再次 Esc 返回，进程未更换，键鼠和触控板辅助保持工作。
5. 模块设置页录制派遣快捷键时，输入现有合成台快捷键被拒绝且继续等待录制；输入 7 成功保存。现有合成台和日委快捷键标签保持不变。
6. 用户对远离凯瑟琳、实体键盘及自选领取或派遣的测试请求回复“测试正常，可以提交 commit”。未将该回复视为所有地区、奖励或派遣条件均已覆盖。

自动测试只执行界面打开和退出，未领取奖励或派遣角色。原生及 Java 构建、APK v2/v3 签名和 `git diff --check` 通过。证据位于忽略的 `build/expedition-original-page.json`、`build/expedition-device-checks.json`、`build/expedition-first.png` 和 `build/verified-expedition-0.1.16.json`。
