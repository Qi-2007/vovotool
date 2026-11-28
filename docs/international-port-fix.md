> **当前状态更新（2026-11-28）**：本文档前半部分是 11-27 的历史排查快照，其中"PC 键鼠未启用""singletonRoot 未攻克""probeLayout 全部失败"等结论**已过时**。国际服 PC 键鼠现已打通并真机验证：键盘 Enter/Esc、鼠标转视角、WASD 均可用；鼠标独占改为依据原生 UI 页面栈深度判定，抽卡/设置/角色面板页面光标正常释放与重新独占。详见本文末尾「2026-11-28 补充」章节。实测环境：ARM64 Android + LSPosed + 原神国际服 7.2.0。

## 2026-11-27 WorkBuddy 接手修复记录

### 结论

前一位 AI 的交接文档判断「整张表不可信、要推倒重来」**过度悲观**。独立验证结果：

- 大部分核心地址前一位 AI 定位得**正确**：`setDevice=0x1173d454`、`mouseUpdate=0xd855f5c`、`keyboardUpdate=0xde4d068`、`uidTextSetter=0x10f32c44` —— 逐指令比对相似度 **1.0**（36 条指令全一致）。
- 真正错的只有**三处崩溃点**。

### 三处错误地址

| 函数 | 旧地址（错） | 新地址（对） | 旧地址相似度 |
|---|---|---|---|
| `tick` | `0x43b2a3c` | `0x11743bf0` | 0.278 |
| `switchInputMode` | `0x44058c8` | `0x11438e8c` | 0.333 |
| `textPopulateMesh` | `0x52d2844` | `0x10f335f8` | 0.444 |

**根因**：三处旧地址都落在 `.text` 段（通用函数 prologue 到处都是，16 字节签名假命中）；真正函数在 200MB 的 `il2cpp` 段。

### 验证方法（可复用）

1. **同节区锚点投票**：`tools/probe_anchor.py <CN.so> <OS.so> <RVA>`
   - 用 CN 函数前 512 字节的 8 字节种子，在 OS **同节区**内找命中，按候选偏移投票
   - 三处正确地址票数 56/52/50，第二名仅 6/5/15 —— 信号极强
2. **逐指令校验**：`tools/verify_pair.py <CN.so> <OS.so> <CN_RVA> <OS_RVA> <条数>`
   - 归一化后立即数与地址，逐条对比
   - 三处新候选前 36 条指令**完全一致**
   - `switchInputMode` 参数签名 `mov w19,w2 / mov w20,w1` 对齐 → 确认 `(this,int,int,void*)`，不是前一份误判的指针参数

### 已落地的改动

1. `app/src/main/cpp/profile_os.hpp`
   - 三处地址修正 + `pins[]` 里的 16 字节指纹同步修正
2. `app/src/main/cpp/module.cpp`
   - 探测失败不再无条件设 `probed=true`（原 bug：探测时机太早失败后永不重试）
   - 改为 2 秒节流重试 + 详细失败日志（输出各字段探测结果）
   - 日志格式：`Input chain probe incomplete: mgr=%d inp=%d ...`

### 真机验证结果（2026-11-27 15:07 / 15:35）

- 构建：`python3 tools/build_linux.py`（单 APK 双服，v2/v3 签名校验通过）
- 安装：`adb install -r dist/vovotool-0.1.38-arm64-linuxbuild.apk`（设备地址按实际填写）
- 结果：
  - ✅ 三次崩溃**全部消失**，crash buffer 为空，游戏进程稳定
  - ✅ 游戏正常进入画面（用户确认）
  - ✅ 钩子安装日志正常：`Native hooks installed for pinned 7.2 ARM64 library`
  - ⚠️ PC 键鼠模式未启用：`Input chain probe incomplete: mgr=0 inp=0 src=0 cfg=0 kb=0 mou=0 mouseSrc=0 kbdSrc=0 dev=1 mode=1`

### 未解决：PC 模式

`probeLayout` 所有链路字段探测失败，但字段偏移 `dev=1 mode=1` 有效 → 根因是 `singletonRoot=0x157d53a0`（数据地址，草稿推断值）错误。

- 前 AI 在 `Memory_v3/.../运维-国际服定位表重建.md` 明确写「国际服的这个地址目前未知」
- 该地址是 .bss 全局变量（运行时地址），不能从 .so 文件字节直接验证
- 国服 `singletonRoot=0x158095a0` 位于 `.bss` 起始 `0x157577c0` + `0xb1e0`
- 国际服 `.bss` 起始 `0x157235c0`，按同偏移推算 `0x157d54a0`（草稿值 `0x157d53a0` 差 0x100）
- 由于 .bss 布局会漂，不能只靠偏移推算；需动态注入扫描或更多逆向

### 服务器死机事件（15:30）

- 症状：TCP 22 可连但 sshd 不发 banner，代理 18765 不通，风扇持续热
- 强制重启后从 `journalctl -b -1` 抓到：`wb-agent.service: killed by OOM killer`，内存峰值 **4.3G**（总 7.7G）
- 上次启动日志停在 14:46（死机前用户态已卡死，journald 停止写入）
- 建议给 wb-agent 加 systemd `MemoryMax=512M`，超限自动重启而非拖垮系统
- 已确认**不是**定位脚本残留导致（重启后无残留进程）

### 下一步建议

1. 给 wb-agent 加 `MemoryMax=512M`（防止 14 天周期 OOM 死机）
2. 若要攻克 PC 模式：动态注入扫描 .bss 找有效单例根（用 `probeLayout` 的类名校验），工作量大，需多次真机测试
3. 崩溃修复已完成，可以先到此为止

## 画质功能修复（16:30，已真机验证生效）

### 根因：三处国服硬编码数据地址未适配国际服

国际服与国服的 `.data`/`.bss` 段起始地址差固定为 `0x344200`
（CN `.data`=0x155cc5c0 / `.bss`=0x157577c0，OS `.data`=0x155983c0 / `.bss`=0x157235c0）。
`graphics.hpp` 里三处数据地址仍是国服硬编码，导致画质功能全面失效：

| 用途 | 国服旧值（硬编码） | 国际服新值（+0x344200） | 验证 |
|---|---|---|---|
| 帧率槽 `graphicsFpsSlot` | 0x156a74d8 | **0x156732d8** | 运行时内存写入后读出 90（配置值） |
| Vulkan resolver `vulkanResolverSlot` | 0x15db8460 | **0x15d84260** | 运行时读出有效函数指针 0x790d…（旧址全 0 → graphicsError=-3） |
| screen 对象 `engineOutputScreen` | 0x15dbfea8 | **0x15d8bea8** | 段偏移换算 + 文件字节一致性验证 |

已改为 `profile::` 常量（profile.hpp 国服值 / profile_os.hpp 国际服值），不再硬编码。

### 第二处根因：photoReady 硬依赖

`installGraphics()` 第一行 `if(!photoReady){graphicsError=-9;return;}`——国际服没装 photo 钩子，
photoReady 永远 false，画质钩子装不上。`chooseRenderSize()` 里场景尺寸改写也要求 photoReady。
VOVO_OS_BUILD 下已去掉 photoReady 依赖（场景尺寸只依赖 Vulkan sizing，与截图无关）。

### 第三处：4 个画质函数地址 + 指纹

前 AI 表里的 `graphicsVulkanInitialize` / `graphicsRebuildScene` / `graphicsPrepareScene`
是 `.text` 段假命中。用同节区锚点投票 C 工具重新定位：

- VulkanInitialize: 0x43b7214 → **0x55cdf8c**
- RebuildScene: 0x4e26438 → **0x96aa128**
- PrepareScene: 0x43b2a3c → **0x96aca18**（48 票 vs 4）

同步修正 graphicsPins 指纹。ScreenHeight 保留 0x44d8914（2 条指令 getter，CN/OS 语义不同但锚点 24 票，分辨率主要靠 graphicsSetResolution）。

### 真机验证（16:29，修复版）

- `graphicsState=[0, 1, 1, 1, 0, 32767, 3200, 1440, 90, 1, 9, 5, 8117, ...]`
  - [0]=0 无错误，[8]=90 帧率，[6/7]=3200x1440
- 帧率：运行时内存 `base+0x156732d8` 实测 **90**（用户设的值），GPU 满载实际约 70 帧
- 场景分辨率：日志 `Render attachment 1920x864 -> 3200x1440`、`2057x926 -> 3200x1440`（改写生效）
- 用户确认：画面变细腻；远处神像无变化属模型/纹理细节，与渲染分辨率无关
- 无崩溃，模块正常注入

### 遗留

- PC 键鼠模式：`singletonRoot` 草稿地址错误，仍未攻克（需动态注入扫描 .bss）
- `engineOutputScreen` 的 table+0xe0/e8 校验在运行时未命中（动态扫 screen 指针 0 结果），
  但 `chooseRenderSize` 已去掉 photoReady 依赖，场景尺寸改写不依赖它，`engineOutputSize()`
  只用于 path=3 的 native resolution 日志补设，不影响实际画质
- 每次 `adb install -r` 会改变 APK 路径，LSPosed 的 `modules` 表记录会滞后，但实测按包名动态加载仍生效

### 复用工具（本轮新增）

- `tools/anchor_vote.c`（临时工具）：memmem 同节区锚点投票，毫秒级，替代 Python 全段扫描（避免 OOM 死机）
- `memread.c`、`memscan.c`、`find_screen.c`（临时工具）：process_vm_readv 运行时内存读取/搜索

## 2026-11-28 补充：PC 键鼠鼠标独占判定（UI 页面栈深）

### 问题

国际服 PC 键鼠打通后，鼠标 capture（独占/释放）判定依赖游戏 cursor 字段（`manager+0x300`，视角模式=-1）。
实测该字段不可靠：

- 抽卡 / 设置页面：cursor 始终 -1，模块误判为世界场景，capture 保持，光标出不来、无法点按钮
- 角色面板：cursor 变 55（释放），但关闭后面板后 cursor 残留 55 不归位，capture 不重建，光标卡住、视角跟着光标动

### 定位过程

1. 测试 APK（`maptest`）抓 Android MotionEvent：未 capture 时鼠标是 `0x2002` 绝对坐标，
   capture 后是 `0x20004` 相对位移（走 `onCapturedPointerEvent`）。确认 capture 是 PC 键鼠的前提。
2. Zygisk 纯 native 路线验证走不通：evdev 能读、PC 模式能开，但游戏只在收到 Java 层 capture 后的
   相对事件才调 mouseUpdate，纯 native 拿不到 → 回到 LSPosed 路线。
3. 备用机内存 dump：国服 UI 栈在 `manager+0x200` 是 `Stack`1`，国际服该位置变成 `List`1，
   导致 `topUiContext()` 失效。扫描找到国际服 UI 栈在 **`manager+0x2A8`**（同为 `Stack`1）。

### 关键发现：UI 栈深就是可靠信号

对 `manager+0x2A8` 的 Stack 取 size（栈顶元素在 `items+0x20+(size-1)*8`）：

| 状态 | 栈深 | 栈顶页面 |
|---|---|---|
| 世界场景 | 1 | `BFIFKIIDFKN`（世界主页） |
| 抽卡 | 2 | `GFLCJHBGMCN` |
| 设置 | 3 | `IEANHLFIEEO` |
| 角色面板 | 2 | `LDALOPOAFFN` |
| 关闭角色面板后 | 回到 1 | `BFIFKIIDFKN` |

混淆类名都是 11 字符、仅用 A-P 字母（base16 风格编码），跨版本会变，**不适合做判定**；
栈深是游戏逻辑结构，跨版本稳定。

### 修复

`tickHook` 里用 `uiPageDepth()`（两服偏移 0x2A8 / 0x200 都试，谁命中用谁）取栈深，
把上报的 cursor 改成逻辑值：

- 栈深==1 → -1（保持独占、转视角）
- 栈深>1 → 55（释放光标）
- 栈不可读 → 退回游戏原值

一处改动同时覆盖：mouseHook 相对位移注入、UDP 代理开关、Java 层 captureLoop/onMotion 判定。

### 真机验证（备用机，国际服 7.2.0）

- 世界场景：鼠标转视角正常
- 抽卡 / 设置页面：光标正常出来，可点按钮（修复前完全不行）
- 角色面板：打开释放光标，关闭后自动重新独占、光标消失、视角恢复（老 bug 修复）
- 无异常日志

### 新遗留

按住 ALT（PC 原神会临时释放光标）与回车打开聊天输入框：实测 UI 栈深、cursor、focus 三字段全程不变，
游戏视角会停但不显示光标。这两种场景走游戏内部另一套输入状态机制，需后续差异扫描定位字段。


## 2026-11-28 补充（光标、加载页、地图、虚化）

上节「新遗留」已在当天全部解决，真机（主力机 192.168.9.20，国际服 7.2.0）逐项验证通过：

- **ALT 临时释放光标**：Java 层 onKey 拦截 ALT down/up，主动释放/恢复 pointer capture，不消费事件。
- **回车聊天框**：回车释放光标、ESC 或鼠标/触摸点击界面外恢复。用显式状态机，不依赖游戏混乱的 View 焦点。
- **加载/登录页不误独占**：新增 inWorld()，除栈深外还校验栈元素类名（大世界 HUD），只有真正在大世界时才让 cursor=-1。加载页保持光标可用，进世界立即独占。
- **地图拖动**：国际服不能复用国服 native map_mouse hook（input+0x40 等偏移不同，实测 SIGSEGV 崩溃，已回退）。改为 Java 层把 UI 页面的鼠标绝对事件（source=0x2002）改写成 SOURCE_TOUCHSCREEN 触摸事件，游戏即可识别拖动。
- **反角色虚化**（本次补验时发现的遗漏）：原 install 分支只装 graphics，未调 installAvatarFade，国际服该功能一直是摆设（读内存 avatarFadeStatus=0 确认）。补齐后发现三处需适配：
  1. installAvatarFade 的 pin 循环遍历整个国服数组（7 个），国际服只覆盖前 5 个，第 6 个国服残留 pin 校验失败。改成按 avatarFadePinsCount 遍历（该 bug 影响所有 installXXX，其余暂未启用故未暴露）。
  2. 国际服虚化组件类名与国服相同：LOFPEGCAEFG（owner 为 PECHLEKMHDF），hook 收到的 self 是组件而非 owner。
  3. 反汇编国际服重算函数 0xcf0e058：min(+0x16c, +0x174[, +0x184]) 写入 +0x170，近距离虚化由 +0x174 驱动（国服是 +0x184）。hook 里临时把 +0x174 置 1，原函数生成不透明材质后恢复。
  国际服 updateAvatarFade 直接 return（国服绑定链偏移不适用），改为在 hook 内按类名+因子直接处理。真机怼近角色验证：保持不透明，功能生效。
- **特效增强**：确认生效，日志有 480x216 -> 2400x1080、1920x864 -> 2400x1080 等 attachment 改写。

### 诊断方法备忘
- 验证 native 状态不必切 app（切 app 会导致游戏停上报）：机器有 KernelSU root，可从 JNI 导出函数反汇编出全局变量 so 偏移，/proc/<pid>/maps 找 vovotool base.apk 映射基址，用 dd if=/proc/<pid>/mem 直接读。
- 定位字段/类名走「反汇编游戏 so + 诊断日志 dump 对象字段」组合，比盲猜偏移可靠。
- 国际服类名是 11 字符 A-P 混淆名，跨版本会变；字段偏移随版本变化，均以当次 dump 为准。
