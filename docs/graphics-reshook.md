# reshook 图形功能移植

适配沃沃头 0.1.17、国服 `7.2.0_48751704_49008628` ARM64。模块设置保存在 `com.qi.vovotool` 的私有 SharedPreferences，原神每次启动读取；Hook 和渲染对象随进程退出而销毁，下一次由 LSPosed 重新安装。没有修改游戏文件、资源、系统或 boot 分区。

## 参考程序和功能范围

用户提供 `Genshin Impact_reshook_1.0.apk`，包名 `com.study.GIhook`，SHA-256：

`e7a13db98bd2777ebd45dc146e5079c59d488992d9eef49bf0e331742856748b`

静态检查它的 `HookEntry`、`ConfigBridge`、设置界面和 `libgihook.so`，确认四项功能：

- 自定义 Vulkan 渲染尺寸，配套处理 framebuffer、render area、viewport、scissor。
- 帧率上限：0（跟随游戏）、30、45、60、90、120、144、165。
- 特效增强：识别主画面附近的半尺寸、四分之一尺寸渲染目标并提高尺寸。半尺寸不超过原来的两倍和目标尺寸；四分之一尺寸提高到目标尺寸。
- 10～100% 的植被及关联质量距离比例。

本工程重新实现功能，不打包参考 APK 的 dex、`.so`、界面资源或签名代码，也不安装、启用该模块。范围限当前国服版本；未宣称支持参考包作用域中的国际服和 B 服。

## 当前版本原生入口

| 入口 | RVA | 处理 |
|---|---:|---|
| 原生帧率 setter | `0x4a807c0` | `adrp; str w0; ret`，写入 `base+0x156a74d8` |
| 原生质量 float 分发 | `0xfa71090` | `bool(string*, float)`，仅调整精确匹配的质量名称 |
| 游戏 Vulkan 初始化 | `0x55ce2b8` | 原调用前替换游戏自己的 resolver 槽 |
| resolver 装载调用点 | `0x55ce2f0` | 校验槽地址所对应的装载代码 |
| 游戏 resolver 槽 | `0x15db8460` | 仅替换本进程可写函数指针 |

入口和调用点加入独立 `graphicsPins`。图形校验失败不阻止原有键鼠、UID、随身界面功能加载。帧率 setter 只有 12 字节，不能使用现有 16 字节 inline hook 覆盖；在游戏 Tick 镜像它唯一的整数字段写入，并读取回报。这里显示的是帧率上限，未把该字段当成实际渲染 FPS。

初次诊断曾尝试系统 `vkGetInstanceProcAddr` inline hook，发生启动 SIGILL；已移除。最终实现只挂接上述游戏初始化入口，保存游戏原 resolver，返回本模块的 Vulkan 包装函数，不再修改系统 Vulkan loader 的代码。

## 分辨率和特效

保存配置后要求重启。第一次图形调用冻结整个进程的配置，之后只对比新设置并显示待重启，避免新旧尺寸混用。启动时无法读取配置则保持原值，后续读取到的新配置也等待下次启动。

渲染宽高须为 4 的倍数，最低 1280×720，不超过屏幕原生尺寸，长宽比偏差不超过 10%。界面提供 100%、85%、70% 预填按钮。图形覆盖独立于 PC 输入开关，默认关闭。

主要使用游戏解析的 `vkCreateSwapchainKHR`、`vkGetSwapchainImagesKHR`、`vkCreateImage`、`vkCreateImageView`、`vkCreateFramebuffer`、`vkCmdBeginRenderPass`、`vkCmdSetViewport`、`vkCmdSetScissor`。同时处理销毁、结束 render pass 和重置 command buffer，清理句柄记录。输入的 const Vulkan 结构均复制后调整，不原地修改调用者内存。

只选择符合主画面比例/尺寸的 attachment；不改普通纹理、立方体、数组纹理、小图标和方形阴影图。以实际场景原始尺寸和 swapchain 原始尺寸识别特效目标，避免把游戏自己的渲染比例误当成屏幕尺寸。framebuffer 的所有 attachment 都能匹配相同尺寸时才改写；viewport/scissor 依据当前 command buffer 的已知 framebuffer 缩放。swapchain 尺寸被驱动拒绝时回退原请求，并停止后续尺寸覆盖、显示错误。

当前验证覆盖本机游戏使用的 Vulkan render-pass 路径；OpenGL、其他 Vulkan render-pass/dynamic-rendering 入口、HDR、多 GPU、多窗口任意比例和每一种游戏特效未逐一验证。资源创建错误通过计数和错误状态报告。没有承诺任意组合分辨率下的所有渲染资源都兼容。

## 距离比例

与参考程序相同，匹配以下名称；未知参数原样传给游戏：

| 名称 | 缩放后的最低值 |
|---|---:|
| `terrainDistance` | 50 |
| `shaderLODDistance` | 0 |
| `lightViewDistanceRatio` | 0.1 |
| `terrainBlendDistance` | 10 |
| `meshDepthBlendDistance` | 1 |
| `textureStreamingBudget` | 16 |
| `viewDistanceRatio` | 0 |
| `shadowDistance` | 0 |
| `characterGroupHairEffectMaximumDistance` | 0 |

100% 完整透传。其他比例为 `max(最低值, 原值×比例)`，非有限值和负数透传。因此这是植被及关联质量距离控制，也影响视距、阴影和部分纹理预算，并非仅草地密度。保留该说明在用户界面。

## 诊断和验证

`graphicsState` 有 21 项：错误、代码校验通过、配置已冻结、本次启用、新设置待重启、已解析函数位图、实际 swapchain 宽、高、帧率上限、改写 swapchain 数、image 数、framebuffer 数、render area 次数、viewport 次数、scissor 次数、成功调整质量参数次数、资源失败数、本次距离比例、本次特效开关、本次分辨率开关、特效 image 数。

错误 -1 为代码指纹失败，-2 为质量入口安装失败，-3 为游戏 Vulkan 初始化/解析入口不可用，-4 为自定义 swapchain 被拒绝，-5/-6 为 image/framebuffer 创建失败，-7 为同名 Vulkan 函数出现不同 dispatch 地址。

独立 ARM64 测试 `tests/graphics_test.cpp` 直接包含生产头文件，覆盖关闭透传、主画面/非主画面选择、半/四分之一特效、矩形缩放及越界裁剪、距离最低值、配置冻结、attachment 一致性、退出 render pass 和 command buffer 重置。测试不加载游戏、不联网。

实机测试使用 Lenovo TB710FU / Android 15 / 现有 LSPosed。已验证保存 90 FPS 上限后在新进程自动读取；2240×1400 和 50% 距离在新进程生效，记录到 swapchain、image、framebuffer、render area、viewport、scissor 改写以及原生质量 setter 成功。截图检查大世界和地图完整；M 打开地图、Esc 返回正常。特效增强记录到实际特效 attachment 尺寸增加，最终构建的验证数据保存在本地忽略的 `build/verified-graphics-0.1.17.json`。这不等于实际帧率达到 90，也没有验证所有场景特效的视觉质量。

设置界面另已实测 70% 预填得到 2240×1400、保存后出现待重启提示，以及恢复游戏原值按钮。最终保留图形覆盖关闭，其他输入/快捷键配置保留。
