# 实际渲染尺寸测量

后续结果：0.1.25 通过原生屏幕尺寸切换同步逻辑与输出，已验证 3200×2000 的完整场景链及同尺寸照片，见 [原生分辨率研究](native-resolution-research.md)。下方保留早期路径的对照记录。

2026-11-15，TB710FU / Android 15 / 国服 `7.2.0_48751704_49008628`。

用户对照发现 0.1.18 比实验性的 0.1.19 更清晰，撤回此前“新路径清晰度正常”的判断。此前把 PostProcessLayer 的 RenderTexture 尺寸当成最终场景分辨率，证据不足。现在按实际提交的 Vulkan 绘制链判断。

## 方法

`render_probe.hpp` 沿游戏自己的 Vulkan resolver 包装 API，不修改被观察调用的参数或返回值。图形覆盖仍由原来的 graphics.hpp 独立执行；观察器记录其实际传给驱动的参数。

常驻记录 image、image view、framebuffer、graphics pipeline、采样图像 descriptor 的元数据。收到 root 诊断采样请求后，从一次成功 present 开始记录 command buffer，丢弃两帧预热，保留三帧。只有 `vkQueueSubmit` 成功提交的 primary/secondary command buffer 才计入帧；记录 render area、逐组绘制时的 viewport/scissor、深度测试/写入、索引/实例数量、附件与绑定的图像输入，再记录 present 的 swapchain image。

这能分别回答：

- 资源只是创建过，还是确实参与了该帧提交？
- 几何绘制实际写入多大的目标、使用多大的视口？
- 后处理是否写回较小的目标，并作为后续输出的图像输入？
- 最终输出是否只是把较小的场景图像放大？

不能单看 `Screen.width`、swapchain、某张 RenderTexture 或创建日志。游戏是多分辨率渲染：阴影、半/四分之一尺寸特效与 UI 各有尺寸，不能要求所有附件相同。图像输入来自绑定的 descriptor，并未反射 shader 以证明每个绑定都被实际采样；需要结合写入顺序、句柄衔接、viewport 和 draw 数判断。最终输出链的句柄已逐级衔接。

## 本机实测

保留用户当前配置：目标 2240×1400、120 FPS 上限、65% 关联距离、特效增强关闭。原生路径只透传分辨率，帧率和距离仍保留这些设置。三组均在大世界前台、关闭菜单后采样，每组连续 3 帧；不是同一时刻的画质或帧率基准测试。

| 路径 | 主几何附件、viewport | HDR 全屏处理 / 场景合成 | 最终输出 |
|---|---|---|---|
| 原生尺寸透传 | 1476×922 | 1476×922 | 2429×1518 |
| 0.1.18 的 Vulkan 尺寸改写 | 2240×1400 | 2240×1400 | 2240×1400 |
| 0.1.19 实验 PostProcessLayer 路径 | 2240×1400 | **1476×922** | 2429×1518 |

每帧两个成功 submit、两个 command buffer，missingCommandBuffers=0、dropped=0；观察器的 14 个 API 包装均被游戏解析。旧版每帧 39 个 render pass，新路径 38 个，原生 37 个。

主几何 pass 为带深度写入的多附件绘制：颜色格式 37/43/43/58 和深度 129。第一帧 draw 数分别为原生 103、旧版 214、新路径 162；三组的主几何 viewport、scissor 与附件尺寸均相符。

新路径第一帧关键顺序（pass 序号从 0 开始）：

1. pass 4：2240×1400，162 次带深度写入的几何绘制。
2. pass 22：2240×1400 HDR 目标，绑定前面的几何附件。
3. pass 25：一个三角形的全屏绘制，**写入 1476×922** HDR 目标，绑定 2240×1400 的前序 HDR 颜色目标。
4. pass 36：1476×922 场景颜色合成，绑定 pass 25 的 HDR 图像。
5. pass 37：2429×1518 输出与 UI，绑定 pass 36 的 1476×922 场景颜色。

旧路径对应的后三步均为 2240×1400。因而新路径并非完全没修改几何分辨率，而是在后处理链中丢失了提高的尺寸。后续修复应定位这两个较小的全屏目标及其描述符/逻辑尺寸，保持照片与输出缓冲一致；本轮没有继续修改它们。

1476×922 是本机当前配置、当前场景的测量，不能当作所有设备/画质的固定默认值。测量当时恢复了旧路径供对照；下述 0.1.21 随后在旧路径上修复输出与拍照尺寸兼容。

## 0.1.21：保留旧场景链，恢复原生输出

用户要求直接从旧版本修复拍照。旧路径把实际 swapchain 和输出附件缩成 2240×1400，游戏内部的输出逻辑尺寸仍是 2429×1518。静态分析原生 `CaptureScreenshotAsTexture` / `DoCaptureScreenshot`（`0x4f528e8` / `0x4f52abc`）可见，它经 ScreenManager 的宽高虚函数读取逻辑输出尺寸，并据此创建 CPU 图像和进行读回；原来修改 backing storage 与此不兼容。这里只用该分析解释尺寸关系，未增加这些入口的 hook。

修复只改变旧 Vulkan 路径的目标选择：swapchain 始终透传，且与其原始尺寸完全相同的输出颜色、深度及拍照 attachment 透传；其余场景目标继续按旧逻辑修改 image、framebuffer、render area、viewport 和 scissor。没有启用 0.1.19 的 PostProcessLayer 改尺寸实验，也没有修改保存后的 PNG 来掩盖裁剪。

实机采样 `build/render-probe-photo-fix-world.json`，连续三帧均为：

| 环节 | 实际尺寸 |
|---|---|
| 主几何的颜色/深度附件、viewport、scissor | 2240×1400 |
| HDR 全屏处理、最终场景颜色合成 | 2240×1400 |
| 输出颜色/深度附件、viewport、swapchain | 2429×1518 |
| 输出 pass 绑定的场景颜色输入 | 2240×1400 |

三个采样帧 missingCommandBuffers=0、dropped=0，没有把场景再降回 1476×922。保存的游戏照片 `20261115161409.png` 为 2429×1518；本地检查画面覆盖四边、无填充黑边。诊断记录一次 2429×1518 图像读回、越界计数 0、图形资源失败计数 0。用户确认“拍照正常，清晰度正常”。保留原有 2240×1400、120 FPS 上限、65% 关联距离、特效增强关闭。

这里保留了游戏原有的**输出与照片像素尺寸**。自定义场景尺寸高于输出缓冲时，游戏显示形成高分辨率渲染后缩小的超采样路径；不能据此宣称得到相同尺寸的高分辨率照片，代价也包括更高 GPU 负载。若要求保存超过原生输出像素数的照片，需要另行适配拍照目标与读回，而不能再次只改 swapchain。独立测试覆盖场景目标高于输出时仍保留输出附件，但当前实机照片验证仅覆盖上述 2240×1400 配置。

## 重现及覆盖范围

0.1.20 加入仅 root/模块自身可调用的诊断接口：

- `set_render_path --arg 0|1|2`：原生 / Vulkan 场景尺寸覆盖 / 实验路径，改动后重启游戏。默认 1，不在普通设置界面暴露；0.1.21 起路径 1 保留原生输出尺寸。
- `capture_render`：请求短时采样，不更改图形设置。
- `render_probe`：读取完成的 JSON；普通 diagnostics 不携带大段 JSON。
- `graphics_config`：只读图形设置，不返回账号或认证数据。

在仓库运行 `py tools/render_probe.py build/render-probe-old-world.json`，脚本请求采样、校验请求编号并保存数据。原始三组记录为 `build/render-probe-{native,old,experimental}-world.json`，属于本地忽略的诊断产物；摘要及 SHA-256 另存 `build/render-resolution-evidence.json`。测试后恢复旧路径及原有用户配置。

观察器只在五帧采样窗口逐次收集 draw，不持续进行高成本捕获。当前游戏使用 core render pass / draw / submit；没有声称覆盖其他程序的 dynamic rendering、render-pass-2、submit-2、动态深度测试、push descriptor 或 shader 实际读写。采样缺失或超限会报告计数；只有菜单的一帧不能用来判断大世界。没有读取图像像素、账号、令牌或修改分区。

独立 ARM64 测试 `tests/render_probe_test.cpp` 覆盖：提交后的 primary/secondary 绘制、附件大但 viewport 小、后续放大绑定、未提交命令与未使用纹理排除，以及缺失命令计数。`tests/graphics_test.cpp` 继续验证三种尺寸模式与已有照片范围检查。

API 语义参考 [Vulkan command buffers](https://docs.vulkan.org/spec/latest/chapters/cmdbuffers.html)、[render passes](https://docs.vulkan.org/spec/latest/chapters/renderpass.html) 和 [vkQueueSubmit](https://docs.vulkan.org/refpages/latest/refpages/source/vkQueueSubmit.html)。实测结论来自本机原始记录。
