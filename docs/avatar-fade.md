# 反角色虚化

适配模块 0.1.29，国服 Android ARM64 `7.2.0_48751704_49008628`。

## 参考与定位

参考 [Fufu Visual.cpp](https://github.com/FufuLauncher/FufuLauncher.UnlockerIsland/blob/master/FufuLauncher.UnlockerIsland/Visual/Visual.cpp) 的 `hk_PlayerPerspective`：开启 `disable_character_fade` 时，将传入原函数的浮点参数设为 1。Fufu 的 Windows 特征码和 RVA 不能用于安卓；本模块根据用户提供的 SDK 和 ARM64 反汇编独立定位镜头可见度入口。

SDK 对应：`LGHCNGLBDGE.DAONHIDBPKO(float)`，RVA `0xa3297e0`。它保存镜头因子到 `+0x184`，并调用原重算函数 `0xa329828`。重算将普通淡入淡出因子 `+0x160` 与镜头因子取较小值，另按原标志考虑 `+0x178`，更新最终值 `+0x168` 和原生材质数据。

`BMPMEOKKKCF(float)` (`0xa3287a8`) 是更通用的最终可见度入口，也用于 EntityActor、剧情和替身等；本模块选择更窄的镜头因子入口。`Camera.ditherValue` 没有找到直接调用引用，未以其作为功能入口。

设备上的当前角色专用组件类为 `LOFPEGCAEFG : LGHCNGLBDGE`。它与镜头的关联链：

`singletonRoot + 0x9070 -> LFEMMJADHPE + 0x50 -> BDBIBJBCPOJ + 0x508 -> BGMCJLBBGJF + 0x200 -> PPEAKFODDID`

PPEAKFODDID 的 `+0x20` / `+0x80` 为两个角色专用虚化组件，组件 `+0x28` 指向 `PECHLEKMHDF` 所属实体。镜头更新中可见 BGMCJLBBGJF 对此浮点入口的调用和原字段读写。

修改前，用户将镜头放到角色明显半透明的位置，实机读取普通因子 1.0、镜头因子与最终值均约 0.397578；截图在忽略目录 `build/fade-before.png`。

## 实现

只对当前镜头绑定中的两个组件生效，验证整条关联链的类名、组件身份和实体身份，其他对象原样调用。窗口失焦不会自行改变画面可见度。

最终 hook 原可见度重算函数 `0xa329828`：先读取游戏当前镜头因子 `+0x184`，启用时仅在本次原函数执行期间临时置为 1，原函数生成最终可见度和材质数据后立即恢复原镜头因子。因此原游戏读取的镜头因子保留，普通因子继续参与原生取最小值计算。保留原函数及 MethodInfo 参数，数值必须有限且在 [0,1] 内。

每个游戏 tick 只更新组件身份、检查开关是否变化；绑定或开关变化时额外调用一次上述重算，保证静止镜头也可即时切换。关闭本功能或模块主开关时，直接用当前原生因子重算即可，无需使用过期的历史值。角色离开当前绑定后不主动调用旧对象，重新绑定时重新核验。

新增独立 `avatarFadePins`，校验 setter 和重算入口、镜头因子与普通因子结合指令、镜头更新入口及镜头因子内联写入、镜头绑定读取指令；不匹配仅停用本功能。

选择此拦截点的依据：仅 hook `DAONHIDBPKO` 的实验没有命中，`BGMCJLBBGJF.HFBAEODABDO(float)` (`0xa0993f8`) 在 `0xa0994b8` / `0xa099560` 会内联写入因子后调用重算；在镜头更新之后补偿虽可恢复不透明，但用户观察到移动镜头瞬间虚化。因此最终在生成当帧材质数据之前临时替换该因子，覆盖内联写入路径。

不调整普通因子 `+0x160`，不强制 renderer 激活，不改树叶虚化、场景雾、碰撞或模型几何。拍照、剧情及特殊角色状态的兼容范围需以实测为准；它不是取消所有形式的角色隐藏。

配置 `disable_character_fade` 默认开启，独立开关约 1 秒生效。Provider root 调试接口 `set_avatar_fade --arg true|false`。诊断 `avatarFadeState`：`[hookStatus, enabled, boundComponents, recalculations, overrides, restores, lastRequestedPermille, lastOutputPermille]`，状态 1 成功，-1 指纹失败，-2 安装失败。两个组件共用最后输入/输出诊断项，不能单凭最后一项代表当前显示角色。

## 验证

- ARM64 `tests/avatar_fade_test.cpp` 执行通过：仅绑定组件覆盖、普通因子保留、原调用及 MethodInfo 透传、材质生成前校正并恢复原镜头因子、独立及主开关即时恢复、组件身份变化、非法值不改动、静止且开关未变化时不主动重算。
- APK 构建及 v2/v3 签名校验通过。
- 2026-11-16，用户确认「正常，移动也不再虚化」。
- 固定近镜头视角进行开启→关闭→重新开启验证：普通因子均为 1，原镜头因子始终约 0.510892；最终可见度分别为 1 → 0.510892 → 1。开关关闭截图呈原生网格半透明，重新开启截图不透明，构图相同。证据保存在忽略目录 `build/verified-avatar-fade-0.1.29.json` 与 `build/fade-{enabled,disabled,reenabled}.png`。
- 已安装最终 APK，SHA256 `4a9f5236a006aa19193a30077284d603888a46ac37eba5bc66eeedf5e713b318`。保留反角色虚化开启，其他设置未改。未进行剧情、联机或全部角色状态测试。
