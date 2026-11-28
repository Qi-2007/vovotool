# HyperOS 系统帧率限制解除

原神自身解锁帧率后，HyperOS 仍会通过 `com.miui.powerkeeper` 的每应用刷新率策略把画面压回 60 Hz。本功能在 PowerKeeper 进程内把模块中选择的帧率上限同步到该系统策略，使显示/合成链路不再把游戏限制在 60 Hz。

功能只在 HyperOS 上提供：非 HyperOS（含 MIUI 14 及更早版本）不显示开关、不安装 Hook、保存时也不会写入开启状态。开关默认关闭。

## 适用平台

| 项目 | 条件 |
|---|---|
| 系统 | HyperOS 1.0 及以上（MIUI 版本代码 ≥ 816） |
| 模块作用域 | `com.miHoYo.Yuanshen` 与 `com.miui.powerkeeper` 均需在 LSPosed 中勾选 |
| 依赖实现 | `com.miui.powerkeeper.statemachine.DisplayFrameSetting#setScreenEffect(String,int,int)`、`#applyFpsViaPowerkeeper(int)` |

平台判定依次检查 `ro.mi.os.version.name`、`ro.miui.ui.version.code ≥ 816`、`Build.VERSION.INCREMENTAL` 以 `OS` 开头。三项都匹配不到时视为非 HyperOS。判定结果在进程内缓存，修改系统属性需重启进程才重新判断。

## 映射规则

系统策略值由 `SystemFpsPolicy.resolve(用户设置, 面板最高刷新率)` 统一计算：

| 用户设置 | 144Hz 面板 | 120Hz 面板 | 说明 |
|---|---:|---:|---|
| 跟随游戏（0） | 144 | 120 | 取面板最高刷新率，帧率仍由游戏自身决定 |
| 30 / 45 / 60 | 不干预 | 不干预 | 不高于 60 FPS 时无需改写系统策略 |
| 90 | 90 | 90 | 原样应用 |
| 120 | 120 | 120 | 原样应用 |
| 144 | 144 | 120 | 超出面板时按面板上限截断 |
| 165 | 144 | 120 | 超出面板时按面板上限截断 |

- 面板最高刷新率取自 `DisplayManager` 中默认显示支持的最大 `Mode.refreshRate`；查询失败时回退 120 Hz。
- 面板本身只有 60Hz 时，「跟随游戏」不产生改写，因为没有可解锁的更高档位。
- 界面会在开关下显示当前选择实际对应的系统策略值；选择高于面板的档位时会直接提示被截断到多少。
- 「跟随游戏」把系统策略设到面板最高，是为了让系统按游戏实际渲染帧率放行；代价是在只渲染 60 FPS 的场景里，显示链路可能仍以高刷运行。需要省电时请显式选择较低档位。
- 配置读取有 3 秒缓存；实际生效还取决于 PowerKeeper 下一次刷新策略的时机（通常是切换应用、修改画面设置或游戏重新进入前台），因此不保证固定的秒数，但不需要重启游戏或手机。

## 实现

- `XposedEntry` 在 `com.miui.powerkeeper` 进程只安装 `SystemFpsUnlock`；`HyperOs.detected()` 为 false 时直接跳过，不运行原神侧的 native hook。
- `SystemFpsUnlock` 挂接 `setScreenEffect` 与 `applyFpsViaPowerkeeper`，只处理 `com.miHoYo.Yuanshen`：
  - `setScreenEffect`：临时把目标值写入请求参数，并在调用期间把 `mUserFps` 改成同一值，绕过方法内部基于 `mUserFps` 的二次钳制；`afterHookedMethod` 立即还原 `mUserFps`，不污染系统刷新率偏好。
  - `applyFpsViaPowerkeeper`：当 PowerKeeper 自己请求的值低于目标值时抬到目标值。
- 依赖 PowerKeeper 私有字段 `mUserFps`、`mCurrentFgPkg` 和上述方法签名；HyperOS 版本升级后若改名或改签名，Hook 会安装失败并记录日志，不会修改系统状态。

## 读取配置

PowerKeeper 使用独立接口 `content://com.qi.vovotool.config/system_fps`，只返回 `enabled`、`system_fps_unlock`、`target_fps` 三列，不返回代理端口、令牌等其它配置，也不触发 `RootProxyService.reconcile()`。调用方 UID 必须等于 `com.miui.powerkeeper` 的 UID（包可见性受限时退化为该 UID 的包名匹配）。原配置接口 `content://com.qi.vovotool.config/state` 恢复为仅原神可读。

## 状态与诊断

- 图形设置状态行会追加系统帧率结果，例如 `· 系统帧率：策略 120 FPS`、`· 系统帧率：Hook 未加载`、`· 系统帧率：未收到 HyperOS 上报`。
- PowerKeeper 通过 `report_system_fps` 上报 `{installed, applied, lastRequested, errors}`，同样只接受该 UID，写入后由模块首页读取。
- LSPosed 日志关键字：
  - `vovotool: system frame rate hook installed`
  - `vovotool: system frame rate hook unavailable`（类或方法不存在）
  - `vovotool: system frame rate hook skipped on non-HyperOS`
  - `vovotool: com.miHoYo.Yuanshen system frame rate 60 -> 144`
  - `vovotool: system fps configuration unavailable`（读取模块配置失败）

## 限制

- 依赖 PowerKeeper 内部实现，未做跨版本兼容承诺；当前验证范围见下文。
- 「跟随游戏」与「实际渲染帧率」不是同一概念：这里只解除系统层对显示刷新率的压制，游戏能否跑满仍取决于设备负载与游戏内设置。
- 系统策略只对本模块配置的国服 `com.miHoYo.Yuanshen` 生效，其它应用和其它服未覆盖。
- 修改模块设置后需等待 3 秒缓存与 PowerKeeper 下一次刷新策略，不保证固定秒数；重启手机后由 PowerKeeper 进程重新加载模块。

## 验证

映射逻辑由 `tests/SystemFpsPolicyTest.java` 覆盖（跟随游戏、各档位、面板截断、未知面板回退），运行：

```bash
javac -d build/test app/src/main/java/com/qi/vovotool/SystemFpsPolicy.java tests/SystemFpsPolicyTest.java
java -cp build/test com.qi.vovotool.SystemFpsPolicyTest
```

输出：`PASS: follow-game, explicit limits, panel clamping and unknown-display fallback`。

实机验证（2026-11-28）：

| 项目 | 结果 |
|---|---|
| 设备 / 系统 | Xiaomi 25091RP04C（2136×3200，面板最高 144Hz）/ HyperOS 4.0 `OS4.0.0.44.XPYCNXM` |
| 原神 / 模块 | `7.2.0_48751704_49008628` / 0.1.34 本地构建 `sha256 b23bd8f3736d5efcb6cb99f12c13939c26817e35587fc3bc96dff8c20099ee00` |
| 系统未解除 + 上限 120 | 59.0 FPS（系统把游戏压回 60，问题复现） |
| 系统解除 + 上限 120 | 104.9 FPS；LSPosed 日志 `system frame rate 60 -> 120`、`120 -> 120` |
| 系统解除 + 上限 165 | 105.9 FPS；日志 `144 -> 144`、`60 -> 144`，即按面板上限 144 截断 |
| 系统解除 + 跟随游戏 | 游戏内 60 FPS；模块界面显示「系统策略将按设备最高 144 FPS 放行」 |
| 重启游戏后 | 多次重启游戏后仍出现改写日志（00:41:59、00:42:34、00:44:58 的 `60 -> 120`） |
| 重启手机后 | 本次日志会话起始于 00:41:25，之后 PowerKeeper 进程重新注入并继续出现改写日志 |
| 首页状态回显 | 图形状态行显示 `· 系统帧率：策略 …` |
| 非 HyperOS | 未实测；代码路径为非 HyperOS 不安装 Hook、不显示开关、保存强制 false、状态行不追加系统帧率文字 |

PowerKeeper 进程日志时间轴：

```
00:41:59  60 -> 120
00:42:13 120 -> 120
00:42:34  60 -> 120
00:44:38 120 -> 120
00:44:58  60 -> 120
00:48:28 144 -> 144
00:48:45  60 -> 144
```

补充说明：

- 该机在 3200×2136 下 120/165 档实际约 105 FPS，未跑满上限，属于渲染负载限制。判断系统是否放行应看日志中的目标值是否明显高于 60，而不是要求实际 FPS 等于设定值。
- `note()` 只在 PowerKeeper 请求值变化时写日志。部分重启场景请求值仍是 60，因此「跟随游戏」档没有新的 `60 -> 144` 行；该档的策略值以模块界面显示为准，映射本身由单测覆盖。
- 本次 LSPosed 日志中没有 `system frame rate hook installed` 行（PowerKeeper 进程启动早于日志文件建立），但 `system frame rate` 改写行证明 Hook 已安装。排查时以改写行为准。
- 90 / 30 / 45 / 60 档未单独上机，由 `tests/SystemFpsPolicyTest.java` 覆盖。
- 验证用构建之后又补了一处非 HyperOS 状态行文字修正（不影响上表结果）。



