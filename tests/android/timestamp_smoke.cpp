#include <rex/graphics/vulkan/timestamp.h>
#include <cstdio>

using rex::graphics::vulkan::TimestampDelta;
static_assert(TimestampDelta(10, 15, 64) == 5);
static_assert(TimestampDelta(UINT64_MAX, 2, 64) == 3);
static_assert(TimestampDelta(0, uint64_t(1) << 63, 64) == (uint64_t(1) << 63));
static_assert(TimestampDelta(0xFFFFFFFF0ULL, 0x20ULL, 36) == 0x30);
static_assert(TimestampDelta(0x123400000001ULL, 0x567800000003ULL, 32) == 2);
static_assert(TimestampDelta(10, 15, 0) == 0);
static_assert(TimestampDelta(10, 15, 65) == 0);

int main() {
  // Check wrap and high unused bits at every supported counter width.
  for (uint32_t bits = 1; bits <= 64; ++bits) {
    const uint64_t mask = bits == 64 ? UINT64_MAX : (uint64_t(1) << bits) - 1;
    const auto before = mask - 1;
    if (TimestampDelta(before, 1, bits) != (3 & mask)) return 1;
  }
  std::puts("timestamp delta: wrap, unsigned deltas, unused bits, unsupported queues PASS");
}
