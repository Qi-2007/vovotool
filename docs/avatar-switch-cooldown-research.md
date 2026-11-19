# 切换角色冷却研究记录

研究日期：2026-11-16～2026-11-17（Asia/Shanghai）。

适配对象：国服 `com.miHoYo.Yuanshen`，`7.2.0_48751704_49008628`，ARM64；Lenovo TB710FU / Android 15 / LSPosed。测试模块版本为 0.1.30。以下地址均为匹配版本 `libyuanshen.so` 的 RVA，不能用于其他版本。

## 结论与收尾

2026-11-19 续研已取得实机进展：将外层 `MADILIEPJJA` 同时传入本地检查和发送函数后，约 190 毫秒间隔的 8 次发送全部收到 `0`，用户确认可以手动快速切换角色。此前的 `101` 结果只适用于原参数组合，不能推导成客户端调用无法实现。头像倒计时另由剩余冷却 getter 控制，详见文末续研记录。

最终对照：约 200 毫秒间隔输入时，8 次按键触发 7 次发送，其中 2 次响应为 `0`、5 次为 `101`；约 1.5 秒间隔输入时，4 次发送均响应 `0`。原响应处理函数对 `101` 单独进入冷却提示分支。这里的 `101` 来自实际切人响应对象，不是客户端检查的失败类型。

2026-11-17 的收尾状态：实验开关关闭；界面中的相关开关、说明和状态移除，代码中的 `no_avatar_switch_cd` 默认关闭。2026-11-19 按用户要求继续研究，设备临时开启实验，界面仍无开关。研究没有修改响应码、伪造成功或屏蔽原提示，也没有修改分区、boot、游戏 APK 或资源。

## 分析材料

- 用户提供的 SDK：`C:\Users\L\Downloads\com.miHoYo.Yuanshen.cs`。
- 匹配的本地库：`D:\code\gi\android-input-research\libyuanshen.so`。
- 辅助方法表：同目录 `runtime-method-map.json`。
- 实验代码：`app/src/main/cpp/avatar_switch.hpp`；版本指纹由 `tools/generate_profile.py` 生成至 `profile.hpp` / `profile.json`。
- 参数与范围验证：`tests/avatar_switch_test.cpp`，曾在 ARM64 设备运行通过。此测试验证 hook 行为，不代表服务端允许快速切人。

## 原生调用链

```text
数字键输入
  HBIJFOJIFAH.NOEOHNDNCKB                 0xce2205c
    独立节流谓词                         0xce10bf4
    HBIJFOJIFAH.LBNFNPHFKON              0xce0e610
      UI 阻塞检查                        0xe1ecacc
      目标角色数据查找                   0xce439b8
      当前实体 getter                    0x7e67f20
      目标实体查找                       0x7e7e434
      Miscs.CanChangeAvatarEntity        0xbb7a550
      本地 0.4 秒请求节流
      DHABIHPMOAH.MGILLDKLPFM            0xecab420
        原生构建请求并交给 DEDCMIKCBCJ    0xec74384
  切人响应
    HBIJFOJIFAH.BEGNBEFMFCI              0xcdf3ac4
      JDOIEGICOAD + 0x18：int 返回码
      0   -> 更新时间戳、清除待处理状态
      101 -> 原生冷却提示
      其他 -> 原生通用错误处理
```

请求入口可能创建重试协程 `HBIJFOJIFAH.JDBLALJGIIM`，其 `MoveNext` 为 `0xf32a41c`。它逐帧重试，超时或被新输入替换时调用 `Miscs.ShowChangeAvatarFailMessage`，所以请求调用次数不等于用户按键数或发送数。

## 本地限制与已尝试的绕过

### 常规切人合法性检查

`Miscs.CanChangeAvatarEntity` 在 `0xbb7a550`，SDK 参数如下：

```csharp
bool CanChangeAvatarEntity(
    PECHLEKMHDF entity, PECHLEKMHDF targetEntity, LOPOKBINJOP targetData,
    ref ChangeAvatarFailType failType, bool ignoreStateLayer,
    bool ignoreChangeCD, bool isSpecialChange, int nyxChangeType,
    GLGMIFGBFID specialChangeTypeEnum);
```

ARM64 原生签名还包含末尾隐藏的 `MethodInfo*`。实验只将 `ignoreChangeCD` 改为 true，保留原调用、结果和其他限制；仅在模块及实验启用、窗口有焦点、顶层为 `InLevelMainPage` 且实体类型匹配时生效。

`0xbb7a7fc` 根据 `ignoreChangeCD` 跳过普通冷却分支；该分支从 `0xbb7a934` 开始，不满足时间条件时设置本地失败类型 `12` 并返回 false。此前尝试仅修改此参数，用户仍观察到约 1 秒限制。

### 共用时间戳和冷却值

```text
root = *(base + 0x158095a0)
HBIJFOJIFAH = *(root + 0x8c40)
HOMIEDIPBBJ = *(HBIJFOJIFAH + 0x80)
HOMIEDIPBBJ + 0x138：ulong HEAODNCKJMB，切人时间戳
原生时钟函数：0x9ba95e0
```

本地冷却比较使用两个 float 之和：`base + 0x1580ce24` 与 `base + 0x1580cf30`。现场分别读到 `0.0` 与 `1050.0`，即本地约 1050 毫秒。此值不是对服务端精确阈值的测量。

`HBIJFOJIFAH.KKPMMNNIKEH`（`0xce0ff34`）计算剩余冷却，读取上述时间戳和值；角色状态不满足时也可能返回完整冷却值。实验只读取其结果，没有统一强制返回零。

### 独立的请求节流

`HBIJFOJIFAH + 0x1e8` 是 float 字段 `LDOEKBDJMNL`。请求函数在 `0xce0e7f4` 附近检查它与 Unity 时间的差值是否超过 0.4 秒；入口前的 `0xce10bf4` 也检查它。

实验在原请求调用期间保存该字段、临时写入负值并在返回后恢复，同时让入口节流谓词在同样的范围内通过。绕过这两层以后，快速请求能到达原生发送入口，但响应仍可能为 `101`。

## 请求出口追踪

通过线程局部上下文关联一次请求内的 UI、数据查找、实体查找、检查和发送事件；不直接 patch 函数中间的分支。退出原因编码：

| 编码 | 含义 |
| --- | --- |
| 0 | 请求函数返回 true；不等于响应成功 |
| 1 | UI 阻塞 |
| 2 | 目标数据不存在 |
| 3 | 当前实体不存在 |
| 4 | 请求 GUID 与当前角色 GUID 相同 |
| 5 | 目标实体不存在 |
| 6 | 目标实体未就绪（实体 `+0x38b` 的低位） |
| 7 | 原生合法性检查返回 false |
| 8 | 检查通过，但尚未到达发送入口 |
| 9 | 其他未分类出口 |

一轮较早的追踪记录 43 次请求：5 次到达发送，38 次在“目标就是当前角色”的检查处返回。不能据此推断其他快速切人均成功；它遗漏了响应层的失败。

在关闭实验后的对照中，本地失败类型读到 `12`，出口追踪为原因 `7`。启用绕过时同类快速输入可以检查通过并发送，随后响应 `101`。

## 2026-11-17 实机证据

以下为原始 `switchflow` 日志中快速测试的发送与响应配对。时间为设备单调时钟的毫秒数，不是北京时间；测试时无其他切人输入。

| 发送时刻 | 响应时刻 | 响应返回码 |
| --- | --- | --- |
| 14138121 | 14138223 | 0 |
| 14138321 | 14138422 | 101 |
| 14138506 | 14138604 | 101 |
| 14138707 | 14138822 | 101 |
| 14139107 | 14139268 | 0 |
| 14139321 | 14139421 | 101 |
| 14139522 | 14139637 | 101 |

随后正常间隔对照：

| 发送时刻 | 响应时刻 | 响应返回码 |
| --- | --- | --- |
| 14178256 | 14178356 | 0 |
| 14179739 | 14179857 | 0 |
| 14181239 | 14181323 | 0 |
| 14182758 | 14182856 | 0 |

由此可区分两种拒绝：未绕过时，客户端合法性检查先拒绝；绕过后，请求能发送，但快速切换仍在响应层被拒绝。服务端限制的判断基于响应回调、原生 `101` 分支及正常间隔对照；不是仅凭界面倒计时推测。

## 容易误判的追踪点与 ABI

- `Miscs.ShowChangeAvatarFailMessage`（`0xbb4a72c`）只能覆盖部分本地失败；重试协程还会传入 `0`，这个分支直接返回，不显示冷却提示。
- 通用错误码方法 `DHABIHPMOAH.JHEEGLDIMIM`（`0xeca9c48`）在前期追踪未命中。没有命中不代表没有失败响应；最终应以 `BEGNBEFMFCI` 收到的对象字段为准。
- 提示方法 `0x7fb7ff8`、`0xe42c8fc`、`0xe419998` 的字符串可能是本地化 key。仅搜索中文“冷却”未命中，不能排除提示发生。
- `MGILLDKLPFM` 接收 `Vector3`、`ulong guid`、`uint id`、`bool special` 及隐藏参数；ARM64 下 Vector3 的三个 float 通过 S0～S2 传递。错误声明会破坏原请求。
- 记录的输入/发送 `id` 在普通切人测试中为 `0`，不是足以直接识别目标角色的角色配置 ID。
- 地址须按版本指纹校验；不能把很短的 getter 当作普通长函数直接做 16 字节 inline hook。

## 继续研究与复现

当前保留 root/self 可调用的 `set_avatar_switch_cd` 诊断入口，界面没有开关。实验默认关闭，需继续研究时再显式启用；测试结束应关闭。诊断输出包括 `avatarSwitchState`、`avatarSwitchTraceState` 和有数量上限的 `switchflow` 日志。

```powershell
# 以下命令会实际切换角色，必须在游戏前台大世界且无其他输入时执行。
$adbPath = 'D:\Program Files\platform-tools\adb.exe'
& $adbPath shell input -d 2 keyevent --duration 80 --delay 100 8 9 10 11 8 9 10 11
& $adbPath shell input -d 2 keyevent --duration 80 --delay 1400 8 9 10 11
& $adbPath logcat -d -s vovotool:I
```

Android keycode `8`～`11` 对应数字键 `1`～`4`。无持续时间的瞬时按键可能被 Unity/Rewired 漏读，不能将其无响应当成冷却证据。

中间反汇编及完整日志保存在本地忽略目录 `build/`，如 `avatar-switch-check.txt`、`avatar-switch-request.txt`、`avatar-switch-send.txt`、`avatar-switch-flow-fast.txt`、`avatar-switch-flow-comparison.txt`；关键证据已抄录于本文，结论不依赖这些未纳入 Git 的文件存在。

## 2026-11-19：GenshinImpact.CT 的新线索

来源：`D:\code\gi\references\GenshinImpact.CT`。本节描述最初的静态核对，当时未执行 CT 或改动游戏；随后按用户要求开展的实机实验见下一节。

表内只有一个实际功能脚本“无冷却”，内容针对切人；不是所有技能的通用无冷却。另有 212 条保存的代码记录（按地址去重为 65 条），以及 6 份未命名结构（元素数为 71/14/210，各重复一次），不足以仅凭这些记录判断对应功能。

脚本限定国际服 PC 7.2.0：PE timestamp `0x6AD8992C`，SizeOfImage `0x1B690000`，并检查两个运行时替换标志及原始指令。现有 `E:\Game\Genshin Impact Game\YuanShen.exe` 的这两个字段分别为 `0x6AA122D0`、`0x1B0AD000`，不匹配；不能直接在该文件或安卓上套用 CT 地址。

### 五处有效补丁

| CT 标签及 PC RVA | 实际字节行为 | 与原安卓实验的关系 |
| --- | --- | --- |
| `SwitchLocalFlagV3`，`0x107BFC5A` | 将栈中布尔参数的读取替换为 `eax=1`；参数传给 `0x131F9090` | 对应本地合法性检查方向，原实验已改 `ignoreChangeCD` |
| `SwitchRequestFlagV3`，`0x107BFD3A` | 同样强制布尔值为 1，但这次传给另一个调用 `0x1123A4A0` | **新增重点：原实验没有改变发送时的布尔值** |
| `SwitchRequestReadyV3`，`0x107BFF95` | `seta al` 改为 `mov al,1` | 与安卓入口节流谓词放行方向相同 |
| `SwitchRetryGateV3`，`0x107D47E2` | NOP 掉 `jbe` 条件跳转 | 与请求/重试节流研究相关，不能仅凭标签断言完全等价 |
| `SwitchCooldownLeftV3`，`0x107E0D20` | 函数入口改为 `xorps xmm0,xmm0; ret`，返回浮点零 | 原安卓剩余冷却 getter 只做了追踪，没有强制归零 |

`assert(GenshinImpact.exe+131F9247,...)` 仅检查原始字节，不是第六处有效写补丁。`[DISABLE]` 恢复五处字节；它不是“仅隐藏提示”的脚本，也没有出现将响应码 `101` 改为 `0` 的处理。

### 安卓端已经确认的参数数据流

安卓 `HBIJFOJIFAH.LBNFNPHFKON`（`0xce0e610`）的 `MADILIEPJJA` 参数由 W7 进入并保存到 W22；它是 `self, guid, id` 后六个 bool 中的第五个，对应实验 wrapper 中的 `e`。

同一个值流向两处：

```text
0xce0e764: and w5,w22,#1
0xce0e780: bl  Miscs.CanChangeAvatarEntity       # ignoreChangeCD

0xce0e878: and w3,w22,#1
0xce0e888: bl  DHABIHPMOAH.MGILLDKLPFM           # 发送函数的 bool
```

发送函数的 SDK 签名为：

```csharp
void MGILLDKLPFM(Vector3 position, ulong guid, uint id, bool MADILIEPJJA);
```

在该函数中，参数 W3 保存到 W19，并在 `0xecab51c`～`0xecab520` 写入请求对象 `+0x29` 的 bool 字段；随后请求交给 `0xec74384`。

原实验在 `CanChangeAvatarEntity` 的 hook 内单独修改 `ignoreChangeCD`，没有修改外层请求函数的 `e`，也没有改变发送函数的 bool。因此，本地检查使用 true，并不意味着网络请求中的相应字段也是 true。此前 `switchflow send ... b=0` 与这条数据流一致。

CT 的两个强制参数写入提示：它可能同时改变了本地检查与请求字段，这比原实验覆盖得更完整。这是基于参数位置、调用形状和安卓数据流的跨平台对应推断；由于缺少匹配的 PC 二进制，还没有逐函数确认 PC 调用目标的完整语义。

下一步若继续研究，应优先确认 `+0x29` 字段的协议语义和原生合法调用场景，再对照其为 true 时的真实响应。仅凭 CT 的名称和补丁内容不能断言服务器会接受，不能把该字段直接命名为已确认的“服务端绕过开关”。返回剩余冷却零还可能影响调用侧行为或显示，应与发送字段分开验证。

此前的 `101` 观测和正常间隔对照仍有效，但它们只覆盖原参数组合。原先将这些结果直接概括为“客户端方法无法实现”过于绝对；本节保留这项修正，避免后续研究遗漏发送参数这一差异。

## 2026-11-19：CT 思路的安卓实机验证

### 请求参数验证

设备两个 ADB 连接 `HA2BXXRH` 与 `192.168.1.240:5555` 经序列号核对为同一台 TB710FU，命令均显式选择 USB 序列号。游戏版本仍与指纹匹配。

实验修改 `avatarSwitchRequestHook`：只在原范围检查通过时，将原函数的第五个 bool（wrapper 变量 `e`）设为 true。这样原函数自己把同一值传到合法性检查和发送函数；没有手工构建请求，也没有在响应中改返回码。原有参数、返回结果及临时节流字段的恢复仍保留。

第一阶段未修改剩余冷却 getter。PID 26597 的快速交替切人记录如下，所有 `send` 的 `b` 都为 1：

| 发送单调时钟（毫秒） | 响应时刻 | 响应码 |
| --- | --- | --- |
| 11108121 | 11108237 | 0 |
| 11108320 | 11108402 | 0 |
| 11108502 | 11108602 | 0 |
| 11108686 | 11108785 | 0 |
| 11108884 | 11108985 | 0 |
| 11109069 | 11109153 | 0 |
| 11109251 | 11109361 | 0 |
| 11109444 | 11109511 | 0 |

相邻发送间隔为 182～199 毫秒，明显短于此前的约 1 秒限制。同期 getter 原始剩余冷却仍为约 1050 毫秒，因此成功并非来自清除倒计时。修正下述旧 hook 加载问题后，用户手动确认“可以快速切换，但是还是会显示倒计时”。

这证明在当前设备、账号和大世界场景中，新参数组合可以实现快速切人；不代表战斗、联机、角色死亡、特殊切人和极端网络条件均已验证，也不能据此恢复其他状态限制的绕过。

### 提示 hook 崩溃及旧 APK 加载

测试中出现两次 SIGILL：北京时间 22:54:47（PID 25455）和 23:00:17（PID 26597）。PC 均位于 `Miscs.ShowChangeAvatarFailMessage + 0x14`（RVA `0xbb4a740`），调用方分别为键盘入口 `0xce220b0` 和重试协程 `0xf32a520`。

原函数的懒初始化路径会跳回靠近入口的位置。现场 `0xbb4a740` 的指令已被 inline hook 的地址字面量覆盖，执行该位置造成非法指令。最终代码不再安装这个诊断 hook，原生失败提示函数完整保留，响应追踪仍在 `BEGNBEFMFCI`。

移除安装代码后，第二次测试仍加载到旧版本：崩溃记录中的跳转目标落入已删除的旧 APK 映射。覆盖安装成功不能证明后续游戏进程已经使用新 native 库。再次启动后，对 PID 28155 直接读取入口 32 字节并与原库比较，确认完全一致；随后测试重复按当前角色键及快速交替切换，进程保持存活，发送响应均成功。用户再次手动确认可以快速切人。

后续安装应等待模块更新完成，在新进程核对入口字节或明确的版本日志；不要在旧进程上反复重试。最终安装日志标记为 `Switch CT v3 installed`。

### 倒计时与范围测试

第二阶段让 `switchCooldownTrace` 保留原 getter 调用，仅在实验已启用、模块启用、窗口有焦点、对象类型匹配且顶层为 `InLevelMainPage` 时返回 `0.f`，其他情况下原样返回。原请求合法性检查仍执行，原生错误提示仍执行。

ARM64 单元测试覆盖：请求参数由 false 提升为 true、原参数为 true 的保留、原函数 false 结果的传递、其他参数及隐藏 MethodInfo、节流字段恢复；模块关闭、实验关闭、失焦、非主页面、对象类型失配、安装状态失败时的保留；getter 在同样范围内返回零、范围外恢复原值。测试通过。

第二阶段最终 APK：`dist/vovotool-0.1.30-arm64.apk`，SHA-256 `8c81d05bc0d69dcdfbb43bce9d7073ab5e953ce262360e2d9f4793d0087a2493`。构建和 v2/v3 签名校验通过，已安装；PID 30510 记录了明确的 `Switch CT v3 installed` 日志，且失败提示函数入口 32 字节与原库一致。

用户最终确认：能快速切人、不显示头像倒计时，重复按当前角色键也不再退出。当前设备保持实验开启；源码默认仍关闭，界面入口仍按此前要求移除。未提交 commit。

随后按用户要求将此功能接入沃沃头设置：反角色虚化下方增加「取消切换角色冷却（实验）」开关及状态显示，使用已有 `no_avatar_switch_cd` 配置键，默认关闭、保留已有选择。沿用约 1 秒的配置轮询热更新，同时控制外层请求参数、节流覆盖及剩余冷却显示；关闭后走原检查并恢复原 getter 返回值，模块主开关关闭时同样停止覆盖。界面明确提示极快连按仍可能被游戏拒绝，不把某次 `101` 当成适配加载失败。

设置版 APK 的 SHA-256 为 `aee9a41ffe25f7074e9454f4ac53446913631c40e306bc80b021f5ee74c246a4`，构建与 v2/v3 签名校验通过，已覆盖安装。通过 UI 层级确认新开关、说明和启用状态可见；实际点击关闭后，原生诊断 `avatarSwitchState[1]` 变为 0，页面更新为“切人冷却保持原样”；重新点击开启后原生值恢复为 1。验证全程游戏 PID 保持 30510，未重启游戏，最终保留开启并将原神带回前台。

仍需保留频率边界：最终手动测试中，发送时刻 `11851304` 和 `11851404` 仅相隔 100 毫秒，前者于 `11851420` 返回 `0`，后者于 `11851505` 返回 `101`。两次发送的标志均为 1，且第二次发生在前一次响应之前。该记录不足以区分更短的服务端间隔限制、在途请求限制或其他状态；不能宣称任意频率的切人均被接受。约 190 毫秒间隔的八连成功属于第一阶段；第二阶段补充的是显示归零及用户手动稳定性确认，二者分别记录。
