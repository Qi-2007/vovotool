# 打开队伍配置时跳过等待

适配版本：国服 Android ARM64 `7.2.0_48751704_49008628`，模块 0.1.28。

## 参考与定位

参考 [Fufu Visual.cpp 的 hk_OpenTeam](https://github.com/FufuLauncher/FufuLauncher.UnlockerIsland/blob/master/FufuLauncher.UnlockerIsland/Visual/Visual.cpp) 以及 [Patterns.h 的 Team 布局和 OpenTeam 特征](https://github.com/FufuLauncher/FufuLauncher.UnlockerIsland/blob/master/FufuLauncher.UnlockerIsland/Patterns/Patterns.h)。Fufu 在协程恢复到状态 1、主页面过渡完成且处于 Ready=3 时临时将等待时长设为 0，调用原 MoveNext 后恢复。不能直接结束协程或在页面过渡未完成时强制执行最后分支。

用户提供的 `com.miHoYo.Yuanshen.cs` 找到同名 `BFIFKIIDFKN.GDMACLLPFAL`，`MoveNext` RVA `0x128f4590`。安卓静态反汇编和设备只读数据核对如下：

| 对象 | 字段 | 安卓偏移 |
|---|---|---|
| GDMACLLPFAL | owner / HJIIHBHDIGF | 0x18 |
| GDMACLLPFAL | state / CPMNEOHGFKE | 0x20 |
| BFIFKIIDFKN | duration / OHIILMFCBDH (float) | 0x610 |
| BFIFKIIDFKN | elapsed / EKJBFGJOBDP (float) | 0x6a4 |
| 页面上下文 | transition | 0x1b4 |
| 页面上下文 | state (Ready=3) | 0x1e4 |

不能套用 PC 的 0x60c / 0x67c 等偏移。设备初始读到 duration=2.0 秒。

`MoveNext` 状态 0 执行进度条初始化和原生输入锁定；状态 1 执行 `elapsed < duration` 的等待，超时后调用原清理 `0xe30f184`、原生可切换检查 `0xe323254`（包括 CanChangeAvatarEntity）、主页面 Ready 检查 `OLGAOPDJCBM.MPIKOFABALF`，然后进入队伍页面。Ready 函数的直接分支读取 transition=0、state=3。

## 实现边界

新增独立 `teamLoadingPins`，校验 MoveNext 入口、时长与 elapsed 读取指令以及 Ready 的两个字段读取指令，任一不符则仅停用本功能。

只在模块主开关、本功能、窗口焦点均有效，协程类及 owner 类匹配、state=1、owner 是当前主页面、transition=0 且 state=3 时调整。duration 和 elapsed 必须有限且在合理非负范围。仅在本次原 MoveNext 调用中将 duration 设为 0，随后恢复原值；不改 elapsed、不手动改变协程状态、不代替原入口、不跳过原生战斗等条件校验。

配置 `skip_team_loading` 默认开启，Provider 每秒同步，关闭可恢复原本读条，无需重启。功能安装需要重启游戏进程。没有修改磁盘游戏文件或分区。

诊断 `teamLoadingState`：`[hookStatus, enabled, calls, skipped, waiting, lastDurationMs, lastElapsedMs]`。状态 1 为安装成功，-1 为指纹不符，-2 为安装失败。skipped 统计临时调整次数，不代表已经切换角色或队伍。

## 验证

- `tests/team_loading_test.cpp` 已在 ARM64 设备执行通过：原始返回值和调用参数保留、状态 0 直通、状态 1 短路等待、页面过渡与非 Ready 保留等待、时长恢复、开关与焦点及类名校验、非法数值直通。
- APK 构建及 v2/v3 签名校验通过。
- 实机关闭开关后，通过 L 打开队伍配置，协程调用数从 2 增至 124（本次 122 次），elapsed=2.0148 秒，duration=2.0 秒；保留原读条。
- 重新开启后，通过 L 打开，调用数从 124 增至 126（本次 2 次），skipped 从 1 增至 2，跳过时 elapsed=16 毫秒；已进入 TeamPage，duration 恢复为 2.0 秒。
- 从派蒙菜单点击「队伍配置」后，调用数从 126 增至 158，waiting 增加 30 次，保留约 0.517 秒的菜单收起过渡；Ready 后 skipped 增加 1 次，并进入 TeamPage。最后诊断为 `[1,1,158,3,30,2000,517]`，duration 仍为 2.0 秒。
- 用户进一步确认「派蒙菜单也正常了」，要求结束测试并提交。未操作角色选择、队伍保存或配队调整。功能保持开启，不再进行游戏操作。
