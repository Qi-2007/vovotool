#pragma once
#include <cstdint>
#pragma pack(push,1)
struct ProxyPacket {
    uint32_t magic=0x47495043;
    uint8_t token[16]{};
    uint32_t sequence=0;
    float gain=0.25f;
    uint32_t enabled=0;
};
#pragma pack(pop)
static_assert(sizeof(ProxyPacket)==32);
