# 地图鼠标拖动及空白处标记

## 证据与定位

用户提供 `com.miHoYo.Yuanshen.cs` SDK，报告鼠标和触控板均能选择锚点，鼠标可以滚轮缩放，但无法拖动地图，也不能在空白处打开标记面板。

SDK 给出了 `MihoyoStandaloneInputModule.ProcessDrag`、`MonoExUITouch`、`EasyTouch.UpdateTouches`、`EasyTouchInput.GetMouseTouch` 的完整签名与 RVA。与当前游戏库静态指令及运行时对象核对后，地图页面为 `InLevelMapPage`，EasyTouch 的 Swipe、SimpleTap、TouchStart 等事件实际订阅到 `MonoExUITouch`。锚点按钮与这些地图背景手势属于不同输入链。

先排查了 UI 拖动对 Unity 光标锁定的检查：此 Android 屏幕管理器的 Cursor.lockState getter 虚表项指向返回 0 的实现，排除“Unity 光标锁定仍为 Locked”的假设。

Android 的 `EasyTouch.Update` 直接读取真实触摸点数，并以 `realTouch=true` 调用 `UpdateTouches`，不会自然使用库中仍存在的 `GetMouseTouch`。因此鼠标能操作 UI 按钮，却没有送入地图的背景手势回调。

| 入口 | 当前 ARM64 RVA | 用途 |
| --- | --- | --- |
| EasyTouch.UpdateTouches | `0x97dad60` | 唯一新增 inline hook |
| EasyTouchInput.GetMouseTouch | `0x915ccfc` | 由游戏原 UpdateTouches 自行调用，生成原生 Finger/触屏手势 |
| Input.GetMouseButton | `0x4a92efc` | 原生左键持续状态，只调用 |
| Input.GetMouseButtonDown | `0x4a92f3c` | 原生按下边沿，只调用 |
| Input.GetMouseButtonUp | `0x4a92f70` | 原生释放边沿，只调用 |
| MonoExUITouch Swipe / SimpleTap | `0xbba304c` / `0xbba3ccc` | 地图原生手势订阅，未 hook 或重写 |

## 0.1.27 实现边界

仅在模块已应用 PC 模式、游戏有焦点、菜单光标状态、顶层为地图、对象为当前 InputManager 的 EasyTouch，且没有真实触摸点或近期触屏事件时转换左键手势。必须在地图内收到新的左键按下才开始会话，不将进入地图前已经按住的攻击键当作地图拖动。保留释放帧，让原生手势系统产生结束或单击事件；离开地图或失焦时不把会话延续到其他界面。

转换时调用原 `UpdateTouches(self, false, 1)`，让它使用自己的鼠标手势转换。`enableRemote` 会再次强制进入真实触摸分支，因此仅在这一原调用期间暂时置 0，返回后恢复原值。没有修改地图坐标、点击/拖动阈值或地图回调，也不自行保存标记。按钮命中、UI 遮挡与手势验证仍由游戏处理。

独立指纹组验证上述入口；不匹配或 hook 失败时透传。诊断 `mapMouseState=[安装状态,地图采样帧,转换帧,左键Down帧,左键Up帧,真实触屏帧]`；安装成功为 1，代码不匹配为 -1，hook 失败为 -2。

## 验证

- `tests/map_mouse_test.cpp` 已在 ARM64 设备通过：地图内按下/持续/释放、进入地图前已按住、真实触屏优先、临时字段恢复，以及非地图/世界镜头/失焦/模块关闭/非 PC 模式透传。
- 0.1.27 APK 构建及 v2/v3 签名校验通过，已覆盖安装、恢复触控板辅助服务并重启游戏。
- 用户实测鼠标/触控板拖动、单击空白处弹出标记面板、锚点点击和滚轮缩放后回复“正常”。未要求用户保存测试标记，验证后没有继续操作地图。

验证限于当前游戏版本的大世界地图；未扩展到其他活动地图页面。真实触屏优先已验证代码分支，本轮未单独要求用户做触屏手势回归。
