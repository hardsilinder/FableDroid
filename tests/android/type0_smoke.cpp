#include <rex/cvar.h>
#include <rex/graphics/command_processor.h>
#include <rex/graphics/graphics_system.h>
#include <rex/logging.h>
#include <rex/memory/ring_buffer.h>
#include <rex/ui/graphics_provider.h>
#include <array>
#include <cstdio>
#include <cstring>
#include <utility>
#include <vector>

namespace g = rex::graphics;
class TestGraphics final : public g::GraphicsSystem {
 public:
  std::string name() const override { return "packet test"; }
 protected:
  void CreateProvider(bool) override {}
  std::unique_ptr<g::CommandProcessor> CreateCommandProcessor() override { return {}; }
};

class PacketProcessor final : public g::CommandProcessor {
 public:
  explicit PacketProcessor(TestGraphics& graphics) : CommandProcessor(&graphics, nullptr) {}
  using CommandProcessor::ExecutePacketType0;
  std::vector<std::pair<uint32_t, uint32_t>> writes;
  uint32_t ranges = 0;
  void IssueSwap(uint32_t, uint32_t, uint32_t) override {}
 protected:
  bool SetupContext() override { return true; }
  void ShutdownContext() override {}
  void WriteRegister(uint32_t index, uint32_t value) override { writes.emplace_back(index, value); }
  void WriteRegistersFromMem(uint32_t index, uint32_t* data, uint32_t count) override {
    ++ranges;
    CommandProcessor::WriteRegistersFromMem(index, data, count);
  }
  g::Shader* LoadShader(g::xenos::ShaderType, uint32_t, const uint32_t*, uint32_t) override {
    return nullptr;
  }
  bool IssueDraw(g::xenos::PrimitiveType, uint32_t, IndexBufferInfo*, bool) override { return true; }
  bool IssueCopy() override { return true; }
};

int main() {
  // Truncated-packet errors are expected and asserted below.
  if (auto* logger = rex::GetLoggerRaw(rex::log::gpu())) logger->set_level(spdlog::level::off);
  TestGraphics graphics;
  PacketProcessor processor(graphics);
  uint32_t rng = 0x2FAB1Eu;
  auto next = [&] { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; };
  for (uint32_t test = 0; test < 2000; ++test) {
    alignas(4) std::array<uint8_t, 1024> storage{};
    const uint32_t count = 1 + next() % 128;
    const uint32_t base = next() & 0x7FFF;
    const bool repeated = next() & 1;
    const uint32_t start = (next() % 256) * 4;
    std::vector<std::pair<uint32_t, uint32_t>> expected;
    for (uint32_t i = 0; i < count; ++i) {
      uint32_t value = next();
      expected.emplace_back(base + (repeated ? 0 : i), value);
      uint32_t swapped = __builtin_bswap32(value);
      std::memcpy(storage.data() + ((start + i * 4) % storage.size()), &swapped, 4);
    }
    const uint32_t packet = ((count - 1) << 16) | (repeated ? 0x8000 : 0) | base;
    for (bool bulk : {false, true}) {
      if (!rex::cvar::SetFlagByName("gpu_bulk_type0", bulk ? "true" : "false")) return 1;
      rex::memory::RingBuffer ring(storage.data(), storage.size());
      ring.set_read_offset(start);
      ring.set_write_offset((start + count * 4) % storage.size());
      processor.writes.clear();
      processor.ranges = 0;
      if (!processor.ExecutePacketType0(&ring, packet) || processor.writes != expected ||
          ring.read_count() != 0) return 2;
      if (bulk && !repeated && count > 1 && processor.ranges == 0) return 3;
      if (repeated && processor.ranges != 0) return 4;
      // A truncated packet must fail without consuming words or writing registers.
      ring.set_read_offset(start);
      ring.set_write_offset((start + (count - 1) * 4) % storage.size());
      processor.writes.clear();
      if (processor.ExecutePacketType0(&ring, packet) || !processor.writes.empty() ||
          ring.read_offset() != start) return 5;
    }
  }
  std::puts("type0: 4000 scalar/bulk cases, endian values, sequential/repeated writes, ring wrap and truncation PASS");
}
