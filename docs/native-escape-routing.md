# 原生 Esc / Back 路径（7.2 ARM64）

## 现象与定位

0.1.2 为特殊键补入 Rewired 状态后，Enter 恢复正常，但场景 Esc 仍弹出“确认退出游戏”。0.1.3 曾将场景 Esc 直接交给派蒙按钮；这能打开主世界菜单，但秘境中的原生退出入口不是派蒙按钮，因此不能通用。

本次在秘境追踪原始键盘状态，确认 Esc 产生动作 ID 38。它与其他输入一起由输入管理器遍历当前界面栈，再进入主页面的 `HandleInputActionEvent`。该分支调用输入模式判断：模式 3 返回 true；模块使用的模式 2 返回 false。false 路径调用 SDK 退出接口，true 路径进入游戏自身的共享返回处理函数。

| RVA | 对象 / 含义 |
| --- | --- |
| `0xe1cf05c` | `MHCNPJOGAGH.OJBPECANOKI`：输入管理器遍历界面栈 |
| `0xe1cc174` | `MHCNPJOGAGH.BPCKBJPFBGG`：当前界面的输入事件处理 |
| `0xe2def3c` | `BFIFKIIDFKN.HandleInputActionEvent`：主页面动作分发 |
| `0xe2df338` | 动作 38 分支调用输入模式判断的 BL 指令 |
| `0xe1d414c` | `MHCNPJOGAGH.JLHNDFBHDKN`：该分支调用的模式判断 |
| `0xe30ffa8` | `BFIFKIIDFKN` 原生共享 Back 处理函数 |
| `0x119d47e4` | false 路径调用的 SDK 退出接口 |

这些是该版本的观察结果，不是通用符号或跨版本偏移。

## 实现

保留 Java 特殊键接收与 Rewired 键值索引转换，移除场景 Esc 队列、派蒙按钮查找及 `Button.Press()` 调用。

LSPosed 挂接模式判断函数，检查实际返回地址是否恰为 `base + 0xe2df33c`。只有本模块启用、stage 2、窗口有焦点、当前输入模式 2、对象为当前输入管理器时，才在该调用处返回 true。原有 `HandleInputActionEvent` 随后自然进入原生 Back 分支。其他调用点，包括其他系统用于检测控制器的逻辑，继续调用原判断函数。

因此，没有在模块中判断“主世界”“秘境”或具体地图，也没有主动寻找场景按钮或构造退出请求。其他界面仍先接收原来的 Esc 事件并按自身规则处理。

## 验证与调试边界

构建会检查判断函数、调用点和共享 Back 处理函数的 16 字节代码指纹。`state[9]` 记录分支进入次数，用于区分按键未到达与游戏原生处理未生效。

临时 Frida 原生追踪得到调用路径及动作 38；随后一次追踪触发 Frida agent SIGSEGV，已停止该方式。最终模块验证使用新游戏进程中的 LSPosed hook，不依赖 Frida 或 Java bridge。

2026-11-14：0.1.4 已构建、通过 APK v2/v3 签名校验并覆盖安装。重新启动游戏后，诊断为 stage 2、键鼠模式 2，原生 Esc 分支计数已增长；用户在秘境验证后确认“经过验证功能正常”。验证记录为 `build/verified-native-escape-0.1.4.json`。结论限于本版本和实测流程，未穷尽所有秘境与界面。
