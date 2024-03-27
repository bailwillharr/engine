#ifndef ENGINE_INCLUDE_GFX_H_
#define ENGINE_INCLUDE_GFX_H_

#include <cstdint>
#include <functional>
#include <string>
#include <type_traits>
#include <vector>

// Enums and structs for the graphics abstraction

namespace engine {
namespace gfx {

// handles (incomplete types)
struct Pipeline;
struct UniformBuffer;
struct Buffer;
struct DrawBuffer;
struct DescriptorSetLayout;
struct DescriptorSet;
struct Image;
struct Sampler;

enum class MSAALevel {
    kOff,
    k2X,
    k4X,
    k8X,
    k16X,
};

struct GraphicsSettings {
    GraphicsSettings()
    {
        // sane defaults
        enable_validation = true;
        vsync = true;
        // not all GPUs/drivers support immediate present with V-sync enabled
        wait_for_present = true;
        msaa_level = MSAALevel::kOff;
        enable_anisotropy = false; // anisotropic filtering can severely affect performance on intel iGPUs
    }

    bool enable_validation;
    bool vsync;
    // idle CPU after render until the frame has been presented
    // (no affect with V-sync disabled)
    bool wait_for_present;
    MSAALevel msaa_level;
    bool enable_anisotropy;
};

enum class ImageFormat {
    kLinear,
    kSRGB,
};

enum class ShaderType {
    kVertex,
    kFragment,
};

enum class BufferType {
    kVertex,
    kIndex,
    kUniform,
};

enum class Primitive {
    kPoints,
    kLines,
    kLineStrip,
    kTriangles,
    kTriangleStrip,
};

enum class CullMode { kCullNone, kCullFront, kCullBack, kCullFrontAndBack };

enum class VertexAttribFormat { kFloat2, kFloat3, kFloat4 };

enum class Filter : int {
    kLinear,
    kNearest,
};

enum class WrapMode : int {
    kRepeat,
    kMirroredRepeat,
    kClampToEdge,
    kClampToBorder,
};

enum class DescriptorType {
    kUniformBuffer,
    kCombinedImageSampler,
};

namespace ShaderStageFlags {
enum Bits : uint32_t {
    kVertex = 1 << 0,
    kFragment = 1 << 1,
};
typedef std::underlying_type<Bits>::type Flags;
} // namespace ShaderStageFlags

struct VertexAttribDescription {
    VertexAttribDescription(uint32_t location, VertexAttribFormat format, uint32_t offset) : location(location), format(format), offset(offset) {}
    uint32_t location; // the index to use in the shader
    VertexAttribFormat format;
    uint32_t offset;
};

struct VertexFormat {
    uint32_t stride;
    std::vector<VertexAttribDescription> attribute_descriptions;
};

struct PipelineInfo {
    std::string vert_shader_path;
    std::string frag_shader_path;
    VertexFormat vertex_format;
    CullMode face_cull_mode;
    bool alpha_blending;
    bool write_z;
    bool line_primitives; // false for triangles, true for lines
    bool depth_attachment_only; // false 99% of the time
    std::vector<const DescriptorSetLayout*> descriptor_set_layouts;
};

struct DescriptorSetLayoutBinding {
    DescriptorType descriptor_type = DescriptorType::kUniformBuffer;
    ShaderStageFlags::Flags stage_flags = 0;
};

struct SamplerInfo {
    Filter minify = gfx::Filter::kLinear;
    Filter magnify = gfx::Filter::kLinear;
    Filter mipmap = gfx::Filter::kLinear;
    WrapMode wrap_u = gfx::WrapMode::kRepeat;
    WrapMode wrap_v = gfx::WrapMode::kRepeat;
    WrapMode wrap_w = gfx::WrapMode::kRepeat; // only useful for cubemaps AFAIK
    bool anisotropic_filtering = true;        // this can be force disabled by a global setting

    bool operator==(const SamplerInfo&) const = default;
};

} // namespace gfx
} // namespace engine

namespace std {
template <>
struct hash<engine::gfx::SamplerInfo> {
    std::size_t operator()(const engine::gfx::SamplerInfo& k) const
    {
        using std::hash;

        size_t h1 = hash<int>()(static_cast<int>(k.minify));
        size_t h2 = hash<int>()(static_cast<int>(k.magnify));
        size_t h3 = hash<int>()(static_cast<int>(k.mipmap));
        size_t h4 = hash<int>()(static_cast<int>(k.wrap_u));
        size_t h5 = hash<int>()(static_cast<int>(k.wrap_v));
        size_t h6 = hash<int>()(static_cast<int>(k.wrap_w));
        size_t h7 = hash<bool>()(k.anisotropic_filtering);

        return ((h1 & 0xFF) << 48) | ((h2 & 0xFF) << 40) | ((h3 & 0xFF) << 32) | ((h4 & 0xFF) << 24) | ((h5 & 0xFF) << 16) | ((h6 & 0xFF) << 8) |
               ((h7 & 0xFF) << 0);
    }
};

} // namespace std

#endif