#include <rex/graphics/pipeline/shader/spirv.h>
#include <rex/graphics/pipeline/shader/spirv_translator.h>
#include <rex/string/buffer.h>
#include <bit>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <vector>

// Translate actual dumped guest programs in both modes without creating a
// Vulkan device. The resulting modules are validated with spirv-val on host.
int main(int argc, char** argv) {
  if (argc != 3) return 1;
  const std::filesystem::path input(argv[1]), output(argv[2]);
  std::filesystem::create_directories(output);
  uint32_t count = 0;
  for (const auto& file : std::filesystem::directory_iterator(input)) {
    const auto name = file.path().filename().string();
    if (name.find(".ucode.bin.") == std::string::npos) continue;
    const auto bytes = std::filesystem::file_size(file.path());
    if (!bytes || bytes % 4) return 2;
    std::vector<uint32_t> ucode(bytes / 4);
    std::ifstream stream(file.path(), std::ios::binary);
    stream.read(reinterpret_cast<char*>(ucode.data()), bytes);
    if (!stream) return 3;
    const auto hash = std::stoull(name.substr(7, 16), nullptr, 16);
    const bool vertex = file.path().extension() == ".vert";
    for (bool texel : {false, true}) {
      rex::graphics::SpirvShader shader(
          vertex ? rex::graphics::xenos::ShaderType::kVertex : rex::graphics::xenos::ShaderType::kPixel,
          hash, ucode.data(), ucode.size(), std::endian::native);
      rex::string::StringBuffer disassembly;
      shader.AnalyzeUcode(disassembly);
      auto features = rex::graphics::SpirvShaderTranslator::Features(true);
      features.max_storage_buffer_range = 128 << 20;
      features.shared_memory_texel_reads = texel;
      rex::graphics::SpirvShaderTranslator translator(features, false, false, false);
      const auto modification = vertex ? translator.GetDefaultVertexShaderModification(127)
                                       : translator.GetDefaultPixelShaderModification(127);
      auto* translation = shader.GetOrCreateTranslation(modification);
      if (!translator.TranslateAnalyzedShader(*translation) || !translation->is_valid()) return 4;
      const auto& binary = translation->translated_binary();
      std::ofstream out(output / (name + (texel ? ".texel.spv" : ".storage.spv")), std::ios::binary);
      out.write(reinterpret_cast<const char*>(binary.data()), binary.size());
      if (!out) return 5;
      std::printf("%s texel=%d memexport=%u bytes=%zu PASS\n", name.c_str(), texel,
                  shader.memexport_eM_written(), binary.size());
      ++count;
    }
  }
  std::printf("translated %u modules\n", count);
  return count ? 0 : 6;
}
