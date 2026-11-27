#pragma once
// 输入链路的结构体/字段偏移。
//
// 为什么单列一张表：国服和国际服是同一份源码编译出来的，但结构体布局会漂
// （实测 manager 的 0x370 在国际服变成 0x33c，input 的 0x1a4 变成 0x18c），
// 所以这些数字不能散落在代码里。
//
// 国际服的链路项（从单例根一路取到 Keyboard/Mouse 实体）没法完全从二进制静态推出来，
// 改成运行时探测：ready() 里本来就逐个用类名核对，探到的偏移必须先通过类名校验才采用，
// 探不到就当作没就绪、不装配 hook，不会往错地址上写东西。

namespace inoff {

struct Layout {
    int rootManager;            // 单例根 -> 输入管理器
    int rootInput;              // 单例根 -> 输入状态
    int rootSources;            // 单例根 -> 设备持有者
    int rootConfig;             // 单例根 -> ConfigVars
    int configPlatform;         // ConfigVars -> PlatformVars
    int sourcesKeyboard;        // 设备持有者 -> Keyboard
    int sourcesMouse;           // 设备持有者 -> Mouse
    int inputBinding;           // 输入状态 -> 按键绑定
    int mouseSource;            // Mouse -> UnityUnifiedMouseSource
    int keyboardSource;         // Keyboard -> UnityUnifiedKeyboardSource
    int keyboardMap;            // Keyboard -> 键码映射表
    int deviceType;             // 管理器 -> 设备类型
    int inputMode;              // 输入状态 -> 输入模式
    int cursor;                 // 管理器 -> 鼠标模式
    int platformDisableKeyboard;
    int platformDisableMouse;
    int keyboardCd;
    int mouseCd;
};

#include "runtime_switch.hpp"
// 国服默认值（全部硬编码确认过）；国际服在 native_init 里被 probe() 覆盖。
inline Layout active = {0x8bc0, 0x8e20, 0x1030, 0xf48,
                        0x88, 0x58, 0x30, 0x20,
                        0x148, 0x100, 0x108,
                        0x370, 0x1a4, 0x340,
                        0x10, 0x11, 0xcd, 0xcd};
inline bool probed = true;
// 国际服的链路偏移：deviceType/inputMode 已确认，其余交给 probe() 运行时填。
inline void apply_os_layout(){
    active = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x33c, 0x18c, 0, 0, 0, 0, 0};
    probed = false;
}

} // namespace inoff
