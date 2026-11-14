# 安卓扫码登录实验

0.1.15 适配当前国服 ARM64 客户端的账号 SDK。已在 Lenovo TB710FU / Android 15 实机完成手机扫码授权、原 SDK 验证及返回游戏启动页。

## PC 静态依据

对本机 7.1.0 的 `YuanShen.exe` 及 `YuanShen_Data/Plugins/AccountPlatNative.dll` 只读分析。扫码页面及请求逻辑位于后者；以下地址均为该 DLL 的 RVA，不是主程序地址。

| 入口 | RVA | 可确认内容 |
| --- | --- | --- |
| 获取二维码 | `0x349190`、`0x34b0c0` | 构造 `app_id`、`device`，经 SDK 配置解析 `fetchQRCode` URL |
| 查询二维码 | `0x35b140` | 构造 `app_id`、`device`、`ticket`，解析 `queryQRCode` URL |
| 公共请求头构造 | `0x399740` | SDK 版本、渠道、客户端类型、语言、设备标识及指纹、游戏业务、生命周期、JSON 内容类型 |
| 授权结果解析 | `0x331770` | 从 `payload.raw` 读取 `is_v2_token`，区分新旧凭据 |

这些入口通过虚函数读取 SDK 配置；没有把未经解析的配置字段当成固定常量。公共头构造处没有直接设置 `User-Agent`，这不能证明整个 PC 网络栈都没有 UA。PC 文件版本较旧，不能声称其所有运行时配置与当前服务端一致。

## 当前实现

在实际使用的 `com.miHoYo.sdk.platform.module.login.LoginManager` 捕获登录会话，并在 `SdkActivity` 的界面提供“扫码登录”。APK 内另一套 `Porte` 登录 SDK 未用于当前登录弹窗，不能仅通过 hook 它增加入口。

请求官方 `https://hk4e-sdk.mihoyo.com/hk4e_cn/combo/panda/qrcode/` 下的 `fetch`、`query`。请求体分别为：

```json
{"app_id":"4","device":"<SDK 当前设备标识>"}
{"app_id":"4","device":"<同一设备标识>","ticket":"<本次二维码票据>"}
```

通过当前安卓 SDK 的 `ComboNetClient.Companion.getHttpHeader()` 读取公共头，按固定白名单转发元数据，复用 SDK 自己初始化的设备标识及指纹；不自行生成指纹，不复制其他设备的信息。请求体 `device` 与 `x-rpc-device_id` 一致。获取和查询共用本次快照。SDK 版本和客户端类型来自本机，未硬编码成旧 PC SDK 的值。当前保留 Android HTTP 默认 UA。

返回链接须为 HTTPS、`user.mihoyo.com`，且 `app_id=4`、`biz_key=hk4e_cn`，然后用游戏 APK 自带 ZXing 在本地绘制。轮询约 1.5 秒一次，刷新或关闭弹窗使旧任务失效。

确认授权后，仅在内存中解析 `Account` 返回的账号、令牌及 `mid`，调用原 `LoginManager.autoLogin(token, uid, uid)`。第三个参数是非空显示标签，空值会使该 SDK 退回缓存账号检查。

当前 SDK 的 Passport 分支原来调用 `loginCheck()`，不会使用传入的扫码凭据。模块仅在本次扫码调用栈内，将这一步接到原 SDK 的令牌验证：新版凭据按 `is_v2_token` 标记构造临时 Passport Account，带上 `mid` 及 SDK `TOKEN_TYPE_STOKEN`（值 1），调用原 `LoginManager.loginVerify()`；旧版凭据由原 `Porte.loginByGameToken()` 兑换。若没有 Passport 模块，使用原 MDK 的 `startVerify(Account, boolean)`。其余账号登录不改动。

以上分支都保留 `NewAutoLoginPresenter$passportStartVerify$1` 原续接回调，后续服务端校验、账号状态、实名处理及游戏回调交给原 SDK。模块不伪造成功回调；账号、票据、二维码链接和令牌不写入日志或诊断。

扫码成功后，等待捕获到的原游戏 `LoginCallback.onSuccess` 实际返回，再在主线程调用原 `closeLoginUI()`，并通过 SDK 界面管理器关闭仍在栈内的 `Login.AccountListLogin`。前者只覆盖手机号、账号密码页面，因此历史账号选择页需要单独关闭。该收尾只处理正在验证的扫码会话；失败、取消和实名确认期间不执行成功收尾。同一已完成回调不会因界面恢复而重新添加扫码入口。

## 排错及验证边界

修正了初始反射调用 ZXing `encode` 时漏传 `BarcodeFormat.QR_CODE` 参数的问题，设备已经能够显示二维码。

另一个独立错误是初始使用了 `app_id=1`。官方接口实际返回 `app_name=崩坏3`、`biz_key=bh3_cn`；`app_id=4` 返回 `app_name=原神`、`biz_key=hk4e_cn`。用户已确认修正后手机显示原神。

用户确认：旧请求在扫码前保持正常，扫码后查询返回 `-3503`，公共错误说明为“请求失败，当前设备或网络环境存在风险”。这不应被误判成获取二维码失败或仅凭错误码认定为网络、UA 问题。对照 PC 请求补入当前 SDK 公共元数据及一致的设备标识后，用户确认成功扫码，设备进入 SDK 验证阶段。

随后排查了两个验证接入问题：`autoLogin` 的缓存账号分支、Passport 的 `loginCheck()` 忽略扫码凭据。首次改接旧令牌兑换后出现“升级应用版本或删除账号后重新登录”的服务端提示；由 PC 解析函数进一步发现 `is_v2_token` 分支，补入对应的原 SDK 验证。

2026-11-15 实机确认本次授权 `is_v2_token=true` 且包含 `mid`；原 SDK 的 `loginResult` 已返回，诊断为 `qrState=[1,1,2,5,0]`。用户确认登录成功，但当时需手动关闭遗留的账号弹窗；截图只证明关闭后已返回带“点击进入”的游戏启动页，不能作为自动关闭的证据。随后补入上述 SDK 成功收尾，用户确认弹窗自动关闭正常。

另一次复测发现：最初把新版令牌标为 `TOKEN_TYPE_GAME_TOKEN`（16）虽然能首次验证，但 SDK 缓存登录会把 16 分发到旧令牌兑换入口，导致二次登录出现相同的“升级应用”提示。PC 的 `is_v2_token=true` 分支把对应类型设为 1，安卓 SDK 的正常缓存验证也只把 `TOKEN_TYPE_STOKEN`（1）分发到 `loginVerify()`。现已改正；由原 SDK 验证并保存更新记录，不直接修改账号数据库或存储令牌。用户重新扫码后确认测试成功，模块状态为 `qrState=[1,0,1,5,0]`。旧实验版留下的记录需重新扫码一次更新。

上述请求的设备标识和指纹均来自当前游戏 SDK；二次登录错误来自令牌类型分发，不应再归因于随机设备信息。旧令牌及未成年人实名等额外分支尚未逐一实测。

最终复测：用户明确确认“二次登录正常”，无需再次扫码即可使用更新后的缓存账号。自动关闭与缓存登录均已完成实机验证。

同一平板的独立 HTTP 测试中，默认 Android UA 和空 UA 在未扫码状态均可得到 `Init`；该结果只覆盖未扫码查询，不能证明扫码后的行为。

`qrState=[hook安装状态,活动会话,生成次数,阶段,错误码]`：阶段 1 为生成，2 为等待扫码，3 为已扫码，4 为 SDK 验证，5 为 SDK 返回登录结果；负值为失败、过期或桥接异常。错误界面区分请求、解析、绘图及查询阶段，网络接口错误仅显示短公共说明和数字错误码。

稳定版回退安装包为 `dist/vovotool-0.1.14-arm64.apk`。扫码开关不影响已验证的原生键鼠、UID、合成台及日委功能；更新后需要重新启动游戏加载新模块。
