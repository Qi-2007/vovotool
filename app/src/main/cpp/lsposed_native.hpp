#pragma once
#include <cstdint>
using HookFunction=int(*)(void*,void*,void**);
using UnhookFunction=int(*)(void*);
using ModuleLoaded=void(*)(const char*,void*);
struct NativeAPIEntries { uint32_t version; HookFunction hook_func; UnhookFunction unhook_func; };
