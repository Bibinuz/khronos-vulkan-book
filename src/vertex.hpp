#pragma once

#include "vulkan/vulkan_enums.hpp"
#include "vulkan/vulkan_structs.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <glm/glm.hpp>

namespace vke {

struct Vertex {
    glm::vec2 pos{};
    glm::vec3 color{};

    static auto getBindingDescription() -> vk::VertexInputBindingDescription {
        return vk::VertexInputBindingDescription{}
            .setBinding(0)
            .setStride(sizeof(Vertex))
            .setInputRate(vk::VertexInputRate::eVertex);
    }
    static auto getAttributeDescriptions() -> std::array<vk::VertexInputAttributeDescription, 2> {
        return std::array{vk::VertexInputAttributeDescription{}
                              .setLocation(0)
                              .setBinding(0)
                              .setFormat(vk::Format::eR32G32Sfloat)
                              .setOffset(offsetof(Vertex, pos)),
                          vk::VertexInputAttributeDescription{}
                              .setLocation(1)
                              .setBinding(0)
                              .setFormat(vk::Format::eR32G32B32Sfloat)
                              .setOffset(offsetof(Vertex, color))};
    }
};

// clang-format off
inline constexpr std::array vertices = {
    Vertex{{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
    Vertex{{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
    Vertex{{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}, 
    Vertex{{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}},
};

inline constexpr std::array<std::uint32_t, 6> indices = {
    0, 1, 2, 2, 3, 0
};
// clang-format on
} // namespace vke
