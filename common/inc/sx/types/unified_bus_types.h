#pragma once

#include <cstdint>

namespace sx::types {

enum class StreamMode : uint8_t {
    kReliableFifo = 0,  // 可靠模式：基于deque，不丢数据
    kRealTimeLatest = 1, // 实时模式：Overwrite模式，最新数据覆盖旧数据
};

} // namespace sx::types
