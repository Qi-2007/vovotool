# 每日委托：游戏原生对话领取路径研究

2026-11-14，Lenovo TB710FU / Android 15 / LSPosed，国服 ARM64 `7.2.0_48751704_49008628`。下列地址均为该版 `libyuanshen.so` 的 RVA。

## 结论与范围

已经从游戏元数据、ARM64 指令和用户正常点击对话选项的运行记录中确认：领取选项能够进入游戏自己的每日委托模块函数。模块可以复用游戏原生业务入口，由游戏构造、发送请求，无需由模块自行创建协议对象。已在联网、大世界主页面且没有打开对话的状态下，主动调用原生选项回调一次，并观察到奖励到账弹窗和状态变化。0.1.14 已加入独立快捷键和开关；远离 NPC 的距离条件及其他奖励分支仍待验证。

## 已证实的调用链

1. `JIAGHGHELBJ` 枚举包含 `NONE`、`QUICK_EXIT`、`TAKE_DAILY_TASK_REWARD`。配置对象 `NHEHIJHFCFJ.KDLCFEFMABD` (`0xfd90ea8`) 检查配置中的动作列表是否含指定值。
2. `MonoGrpSelect.SetupSelectItemByConfig` (`0x101e36a0`) 在 `0x101e3bf8` 以值 2 检查该标记。成立时，将 root `+0xbed0` 的 `PBICDDIHDAP` 实例和方法槽 `0x159dd000` 对应的 `NHPNFNOFKDE` (`0xe1293b8`) 绑定到选项回调。
3. `PBICDDIHDAP.NHPNFNOFKDE` 保存选项索引到实例 `+0x158`，调用 `HEHCONMDGIC` (`0xe124668`) 判断是否需要额外确认。需要确认时打开 `MonoTakeDailyConfirmDialog` 对应上下文；普通分支调用同模块的 `MCNEHJFPOEI(false)`。
4. 用户正常点击后，PID 24007 多次记录 `claimTask value=0 caller=0xe129438`。栈同时包含选项执行入口 `0x11c187b8`，对应上述原生分支，证明这条路径实际执行，而非仅凭方法名推断。
5. `MCNEHJFPOEI` (`0xe1247fc`) 经现有游戏分配函数 `0xda0d424` 创建请求类型 `FAMBOGIFBBD`，并经 root `+0x8e80` 的发送模块调用 `0xec74384`。布尔参数写入请求对象；其准确业务字段名称未解析。确认对话的回调 `BEBIKIBDLOM.EKNHJADGPPP` (`0x8535ec0`) 对此函数传入 true。

原生入口调用模型可表述为：`MCNEHJFPOEI(dailyModule, false, methodInfo)`，其中 `dailyModule` 是既有 root `+0xbed0` 实例。它的声明参数中没有 NPC、对话页面或场景交互对象；其已分析普通发送分支同样不读取这些对象。后续联网单次实验已经证明不需要打开对话即可领取；但该次角色仍站在凯瑟琳附近，因此没有验证距离条件。优先复用较高层业务回调以保留原生确认判断；不要盲目省略确认页面。

另找到 `PBICDDIHDAP.BMHPLAOJJOF` (`0xe126e48`)，其无业务参数，使用另一种请求类型，直接调用点来自 `BNHCLKOHEFI.LGEOHAPCNMJ` (`0xe076c24`)。本轮没有记录其被凯瑟琳选项调用，不能将它直接认定为已验证的额外奖励入口，也不据此复制外部插件的固定双请求和 600 ms 调度。

## 断网追踪

可在联网进入目标对话后切断网络，再点击选项，观察客户端原生回调及请求提交路径。此时不能验证服务器响应、材料/历练点最终变更或奖励到账。游戏可能在检查网络状态后提前返回，因此断网日志缺少某段调用也不能证明该段不存在。

恢复网络前应先结束游戏进程，以丢弃仍在用户态等待发送的本轮请求，再重开游戏。若断网前已经发出请求，结束进程不能撤销服务器已经处理的领取。应确认所有联网通道均已断开，而非只看 Wi-Fi 图标。

本轮观察到断网状态，也在稍后的检查中观察到 Wi-Fi 重新连接；没有持续隔离证据，不能保证所有点击均在离线状态，不能据日志断言今日奖励未消耗。测试结束时先 force-stop 游戏，再恢复正式 0.1.13 APK 并重新启动，清除了诊断版进程中的 hook 和待处理状态。

## 诊断 hook 崩溃与处理

第一版诊断对 `0xe1293b8` 的入口做 inline hook。该函数首次初始化分支从 `0xe129470` 跳回 `0xe1293cc`，该地址位于 hook 覆盖区；回跳进入跳转数据后触发 SIGILL。tombstone 的 PC 为 `libyuanshen.so+0xe1293cc`，并有该次点击 `claimButton value=0` 的先行日志。

已移除该入口 hook，重新启动后只在下游函数记录调用，检查 `0xe1293b8` 前 16 字节与原库一致；随后捕获到上述 `MCNEHJFPOEI(false)` 调用。本轮不将首次崩溃归因于“领取函数不能调用”。后续入口 hook 必须检查首次初始化和内部回跳，不能只校验前 16 字节。

诊断工程独立位于 `D:\code\gi\vovotool-daily-research`；正式项目功能源码没有加入诊断 hook。观测代码仅记录调用和栈地址，继续原来的参数与返回值，不额外执行领取、不修改协议或网络响应。研究期间未修改任何分区。

## 证据

- 正式工程忽略目录：`build/verified-daily-dialog-trace.json`。
- 本地研究目录：`D:\code\gi\android-input-research`，包括 `commission-dialog-live.log`、`commission-native-daily-module.json`、`commission-native-action-classes.json`、`commission-claim-callers.json`、`commission-trace-crash-tombstone.txt`。
- `commission_metadata.py` / `commission_params.py`：依据本版 IL2CPP 元数据读取代码还原字段和参数类型，用于定位动作枚举的使用者。
- 正式 APK 恢复为 `dist/vovotool-0.1.13-arm64.apk`，SHA-256 `7d5678dcb7aced848940947acc8d2a71e30696059467fb61893842fbbb4eff23`。快捷键配置继续保留用户设置。

## 联网主动调用验证

用户明确要求进行一次联网主动调用后，创建独立研究版 `0.1.13-dailyonce`。使用现有游戏 Tick 执行原生 `PBICDDIHDAP.NHPNFNOFKDE(index=0)`，隐藏 MethodInfo 参数传 nullptr；未再挂接该函数或任何新的游戏入口。调用前检查模块启用、游戏焦点、`InLevelMainPage`、镜头模式、模块对象类型及原函数指纹。root 受限配置命令只排队一次请求，同一进程使用后不能重复触发，条件不符则拒绝且不延后重试。

在 PID 28330 中实测：

1. 联网，游戏处于大世界主页面，角色在凯瑟琳附近，没有打开对话。
2. 调用前 `dailyState=[0,0]`；23:58:08 的游戏线程日志记录执行一次原生回调，并正常返回。
3. 调用后 `dailyState=[3,1]`，游戏进程 PID 不变，无崩溃。
4. 游戏弹出原生“获得”界面：20 原石、500 冒险阅历、55 好感经验、7250 摩拉、1 个涤净青金碎屑。
5. 每日委托模块实例 `+0x16c` 的只读观测值由 0 变为 1；其他采样标量不变。该字段的混淆名称尚未解释，不单凭偏移给它命名。

该结果证明：通过游戏原生回调，可以在没有打开 NPC 对话时完成本次联网领取。没有在模块里手工分配请求、设置协议字段、调用底层 sender 或伪造服务器结果；原生回调自己执行其业务判断和网络流程。这里只验证了本次已满足条件的奖励，没有执行另一条尚未确认的请求分支，也没有重复调用。

证据：正式工程忽略目录 `build/verified-daily-online-once.json`；研究目录 `commission-online-once.log`、`commission-once-before.json`、`commission-once-result.json`、`daily-once-ready.png`、`daily-once-result.png`。测试结束后已结束测试进程、恢复正式 0.1.13 并重新启动游戏；正式 APK 不含单次调用入口。

## 0.1.14 正式功能

使用 `daily_claim.hpp` 在现有 Tick 中调用上述高层回调。原生 `PBICDDIHDAP.BGMMLFGBKLA` (`0xe124320`) 是只读 getter，返回实例 `+0x16c` 的布尔状态；服务端数据处理路径初始化此字段，领取响应处理 `DJLBLJPBLCM` (`0xe1242c0`) 将其设为 true。两个新增入口都纳入当前版本指纹校验，不新增 inline hook。

- `portable_daily` 默认开启；`daily_key` 默认 Android F9（139），`daily_meta` 保存修饰键。主页面新增独立录制按钮，支持普通按键和 Ctrl/Alt/Shift/Meta；两个功能不能录制同一组快捷键，保留已有合成台设置。
- 每次 Down 边沿只排队一次，按住及等待期间不会重复调用。须启用模块、启用功能、游戏有焦点、输入已应用、镜头模式 `cursor=-1`、顶层 `InLevelMainPage`、请求不超过 1500 ms。关闭功能会清除待执行请求，不延迟到再次启用时弹出。
- 调用间隔至少 2 秒。先读取领取状态，已经领取则不调用；否则执行原生回调一次，保留游戏确认窗口。等待最多 30 秒，无自动重试。
- 只有观察到原生领取状态从未领取变为已领取，才计入确认次数并显示成功。调用返回或超时均不直接当作领取成功或服务器拒绝。
- `dailyState=[状态,开关,按键请求数,原生调用数,状态确认数,拒绝数]`。0 就绪，1 排队，2 已调用待结果，3 已确认，4 已领取且未重复提交；-1 焦点/页面/时效条件不符，-2 对象未就绪或身份变化，-3 等待超时，-4 冷却中。其他诊断数组保持原长度。

`tests/daily_claim_test.cpp` 将生产头文件接入隔离的模拟游戏状态，不调用游戏或服务器。使用便携 NDK 的 clang++ 以 `--target=aarch64-linux-android26 -std=c++17 -static-libstdc++ -fPIE -pie -I app/src/main/cpp tests/daily_claim_test.cpp -o build/daily-claim-test` 编译，然后经 ADB 推送至 `/data/local/tmp/vovotool-daily-claim-test` 并运行。已经在当前设备通过：单次调用、忙碌拦截、状态确认、已领取、失焦、菜单、过期、关闭、对象无效、等待超时和冷却。

输出 `dist/vovotool-0.1.14-arm64.apk`。构建、APK v2/v3 签名、16 KB 对齐和新增 JNI 导出检查通过。

### 正式版实机结果

0.1.14 已安装并重新启动游戏。在此前已经领取奖励的账号中，实体键盘触发的 `dailyState` 状态为 4，原生调用数始终为 0，证明重复提交保护生效。用户起初反馈按键看似没有反应后，加入仅在按键请求/结果变化时显示的短提示，并限制提示频率。用户随后确认能看到“日委奖励已领取，无需重复提交”。失焦时不弹提示；主开关或功能关闭后不处理领取快捷键。

记录 `build/verified-daily-0.1.14.json`。本次正式版没有再次消耗奖励；领取成功的正向证据来自前述联网单次实验。尚未验证距离条件、未满足条件时各种游戏提示以及确认界面停留超过 30 秒的全部组合。

本次输出 SHA-256：`4c7215427eaa9338cd677bbf36ea930c606db1a38beb1929ae60b662cccec2b9`。
