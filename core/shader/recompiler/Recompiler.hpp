#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

namespace ShaderRecompiler {

enum class ShaderStage {
    Compute,
    Vertex,
    TessellationControl,
    TessellationEvaluation,
    Geometry,
    Fragment,
    Local,
    Mesh
};

struct ShaderBinary {
    ShaderStage stage;
    std::uint64_t codeAddress;
    std::span<const std::uint32_t> code;
    std::uint64_t headerAddress;
    std::span<const std::byte> header;
};

struct ShaderCompileRequest {
    ShaderBinary shader;
    int targetVulkanVersion = 1;
    int targetSpirvVersion = 0x10500;  // SPIR-V 1.5
    std::uint32_t subgroupSize = 32;
    bool useCache = true;
};

struct ShaderCompileResult {
    std::vector<std::uint32_t> spirv;
    std::string debugInfo;
    bool cacheHit = false;
};

// Maxwell (Tegra X1) shader entry points. Input is NVN/Maxwell bytecode,
// output is SPIR-V for Vulkan.
ShaderCompileResult CompileShader(const ShaderCompileRequest& request);
std::shared_ptr<const void> GetResourcePlan(const ShaderCompileRequest& request);

} // namespace ShaderRecompiler
