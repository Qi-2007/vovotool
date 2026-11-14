# 派蒙菜单 UID 与外框

2026-11-14，Lenovo TB710FU / Android 15 / LSPosed 2.2.1（7941），国服原神 `7.2.0_48751704_49008628`。

现有 HUD 逻辑只匹配 `TxtUID`，且只缓存一个 Text 实例。通过临时诊断记录符合 UID 格式的组件名称，定位到菜单的 `PlayerID`。0.1.9 将缓存改为有界的多目标记录，每个组件单独保存原文、原生对象身份及恢复状态，并在文字网格生成前清空显示数字。

进一步读取该组件的 Transform 父链，实测路径为：

```text
/Canvas/Pages/PlayerProfilePage/GrpProfile/Right/GrpPlayerCard/UID/Layout/PlayerID
```

0.1.10 逐级校验这条完整路径后，隐藏 `UID` GameObject。该节点包含菜单 UID 数字、标签、外框和复制按钮，头像、冒险等阶等其他资料位于节点之外。首次调整前记录 activeSelf，关闭隐藏或关闭模块主开关时恢复原来的状态。不改账号 UID、复制回调、游戏资源或分区。

0.1.11 移除菜单 `PlayerID` 的单独文字替换：它仅作为定位父节点的入口，识别后交给 `groupOnly` 分支，原始 setter 和网格生成保持原参数。不会保存菜单 UID 原文，也不会清空或恢复其 Text 字段。路径校验不符时不调整菜单；HUD 的独立文字隐藏逻辑继续保留。0.1.10 同时调整菜单文字与父节点的做法属于已删除的重复操作。

新增入口均使用当前版本指纹校验：

| 接口 | RVA |
| --- | --- |
| Component.get_transform | `0x80e5930` |
| Component.get_gameObject | `0x80e5b50` |
| Transform.get_parent | `0x8fffc18` |
| GameObject.SetActive | `0x95779f8` |
| GameObject.get_activeSelf | `0x95779fc` |

最终代码不保留诊断阶段的固定元数据地址、方法枚举及候选组件日志。显示原文仅保存在进程内存，日志只记录组件识别结果。节点开关在 Tick 中执行，避开 Canvas 文字网格重建阶段；锁允许 Unity 生命周期同步回调 Text hook。对象身份无效后不再操作旧对象。

## 验证

- 编译、APK v2/v3 签名及 16 KB 对齐检查通过。
- 0.1.10 已安装，包名 `com.qi.vovotool`，versionCode 11。
- 同一游戏进程 PID 28914 完成隐藏、恢复、再次隐藏及重新打开菜单，无需中途重启。
- 关闭时 `uidState=[3,0,0,2,8]`，最终 `uidState=[2,1,1,2,12]`；识别到 HUD 与菜单两个文字实例，没有文字/节点校验失败。
- 截图确认菜单 UID 数字、标签、外框与复制按钮同时隐藏；关闭开关后它们和右下角 UID 均恢复。再次开启及重开菜单后仍正常。
- 触控板兼容辅助进程已恢复运行；当前 UID 隐藏开关为开启。

本地截图及诊断位于被 Git 忽略的 `build/` 下：`uid-frame-hidden-0.1.10.png`、`uid-frame-restored-0.1.10.png`、`uid-frame-final-0.1.10.png`、`verified-uid-frame-0.1.10.json`。其他 UID 页面及节点销毁重建场景没有逐一穷尽测试。

输出 `dist/vovotool-0.1.10-arm64.apk`，SHA-256：`1ff53954a7a6c061242a36d497bb4550085efee04cf96ed44fb24ade9f4de51b`。

## 0.1.11 简化后验证

编译、签名及 16 KB 对齐检查通过，versionCode 12 已安装。在同一游戏进程 PID 32034 中验证隐藏、恢复及重新隐藏；截图确认菜单数字、标签、外框与复制按钮共同恢复，HUD UID 也能恢复。关闭时 `uidState=[3,0,0,2,5]`，最终 `uidState=[2,1,1,2,7]`，当前保持隐藏开启，触控板辅助进程已就绪。

仅隐藏父节点后，菜单原始 UID 文本由游戏保留，模块不再修改该字段。截图与诊断保存在 `build/uid-frame-hidden-0.1.11.png`、`build/uid-frame-restored-0.1.11.png` 及 `build/verified-uid-frame-0.1.11.json`。

输出 `dist/vovotool-0.1.11-arm64.apk`，SHA-256：`f0bd8cf1cafaf83ff399c77829380eb6221d7a42f811255cd7743b788cb4d651`。
