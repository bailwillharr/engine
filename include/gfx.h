#pragma once

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
    MSAA_OFF,
    MSAA_2X,
    MSAA_4X,
    MSAA_8X,
    MSAA_16X,
};

enum class PresentMode {
    DOUBLE_BUFFERED_NO_VSYNC,
    DOUBLE_BUFFERED_VSYNC,
    TRIPLE_BUFFERED,
};

struct GraphicsSettings {
    GraphicsSettings()
    {
        // sane defaults
        enable_validation = false;
        present_mode = PresentMode::DOUBLE_BUFFERED_VSYNC;
        msaa_level = MSAALevel::MSAA_OFF;
        enable_anisotropy = false; // anisotropic filtering can severely affect performance on intel iGPUs
    }

    bool enable_validation;
    PresentMode present_mode;
    MSAALevel msaa_level;
    bool enable_anisotropy;
};

enum class ImageFormat {
    LINEAR,
    SRGB,
};

enum class ShaderType {
    VERTEX,
    FRAGMENT,
};

enum class BufferType {
    VERTEX,
    INDEX,
    UNIFORM,
};

enum class Primitive {
    POINTS,
    LINES,
    LINE_STRIP,
    TRIANGLES,
    TRIANGLE_STRIP,
};

enum class CullMode { CULL_NONE, CULL_FRONT, CULL_BACK, CULL_FRONT_AND_BACK };

enum class VertexAttribFormat { FLOAT2, FLOAT3, FLOAT4 };

enum class Filter : int {
    LINEAR,
    NEAREST,
};

enum class WrapMode : int {
    REPEAT,
    MIRRORED_REPEAT,
    CLAMP_TO_EDGE,
    CLAMP_TO_BORDER,
};

enum class DescriptorType {
    UNIFORM_BUFFER,
    COMBINED_IMAGE_SAMPLER,
};

namespace ShaderStageFlags {
enum Bits : uint32_t {
    VERTEX = 1 << 0,
    FRAGMENT = 1 << 1,
};
typedef std::underlying_type<Bits>::type Flags;
} // namespace ShaderStageFlags

struct VertexAttribDescription {
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
    bool line_primitives;       // false for triangles, true for lines
    bool depth_attachment_only; // false 99% of the time
    std::vector<const DescriptorSetLayout*> descriptor_set_layouts;
};

struct DescriptorSetLayoutBinding {
    DescriptorType descriptor_type = DescriptorType::UNIFORM_BUFFER;
    ShaderStageFlags::Flags stage_flags = 0;
};

struct SamplerInfo {
    Filter minify = gfx::Filter::LINEAR;
    Filter magnify = gfx::Filter::LINEAR;
    Filter mipmap = gfx::Filter::LINEAR;
    WrapMode wrap_u = gfx::WrapMode::REPEAT;
    WrapMode wrap_v = gfx::WrapMode::REPEAT;
    WrapMode wrap_w = gfx::WrapMode::REPEAT; // only useful for cubemaps AFAIK
    bool anisotropic_filtering = true;        // this can be force disabled by a global setting

    SamplerInfo() = default;

    bool operator==(const SamplerInfo&) const = default;
};

} // namespace gfx
} // namespace engine

// there has to be another way...
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