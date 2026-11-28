#pragma once
// 一份构建双服共存的运行时开关；native_init 读 /proc/self/cmdline 设定。
inline bool runtimeIsOS = false;
