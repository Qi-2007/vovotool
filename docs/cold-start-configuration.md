# 游戏冷启动配置

设备重启后，原神可能无法直接解析模块的配置 Provider，日志为
`Failed to find provider info for com.qi.vovotool.config`。此前打开模块应用会调用
`grantUriPermission`，因此先打开模块后测试会掩盖这个问题。

配置读取失败时，现在从已恢复的游戏 Activity 启动模块的无界面
`ConfigurationBootstrapActivity`，建立面向原神的配置 URI 授权，并按保存的开关
恢复触控板兼容服务。入口立即结束，沿用原神当前任务；游戏现有配置轮询负责重读。
Provider 的调用方检查继续限定为模块自身和原神。

`Application.attach` 阶段只尝试读取配置，不启动 Activity，也不消耗唤起重试间隔。
首次唤起由游戏恢复后的轮询发起，避免启动被后台限制后等待五秒才重试，错过图形初始化。

## 实机回归

2026-11-22，Lenovo TB710FU / Android 15 / ZUI，最终修复包覆盖安装后重启验证：

1. 启动游戏前，`adb shell pidof com.qi.vovotool` 无输出。
2. 只启动原神，不打开模块，也不提前调用模块的 Provider。
3. 先用 `adb shell dumpsys activity providers` 确认原神已连接配置 Provider。
4. 此后才读取诊断，确认游戏持续上报，原生状态前两项为 `[2, 2]`，
   触控板兼容为开启、端口有效、服务已就绪。
5. 图形诊断确认已应用 2080×1300、120 FPS 上限，待重启标记为 0；
   `dumpsys activity activities` 确认前台仍是原神 MainActivity。

调用诊断接口本身会启动 Provider，所以不能用它作为冷启动验证的第一步。
诊断中的 120 FPS 是配置上限，不是实际帧率测量。
