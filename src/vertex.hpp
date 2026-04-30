#pragma once

#include "glm/ext/vector_float3.hpp"
#include "vulkan/vulkan_enums.hpp"
#include "vulkan/vulkan_structs.hpp"
#include <array>
#include <cstddef>
#include <functional>
#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>

namespace vke {

struct Vertex {
    glm::vec3 pos{};
    glm::vec3 color{};
    glm::vec2 texCoord{};

    static auto getBindingDescription() -> vk::VertexInputBindingDescription {
        return vk::VertexInputBindingDescription{}
            .setBinding(0)
            .setStride(sizeof(Vertex))
            .setInputRate(vk::VertexInputRate::eVertex);
    }
    static auto getAttributeDescriptions() -> std::array<vk::VertexInputAttributeDescription, 3> {
        return std::array{vk::VertexInputAttributeDescription{}
                              .setLocation(0)
                              .setBinding(0)
                              .setFormat(vk::Format::eR32G32B32Sfloat)
                              .setOffset(offsetof(Vertex, pos)),
                          vk::VertexInputAttributeDescription{}
                              .setLocation(1)
                              .setBinding(0)
                              .setFormat(vk::Format::eR32G32B32Sfloat)
                              .setOffset(offsetof(Vertex, color)),
                          vk::VertexInputAttributeDescription{}
                              .setLocation(2)
                              .setBinding(0)
                              .setFormat(vk::Format::eR32G32Sfloat)
                              .setOffset(offsetof(Vertex, texCoord))};
    }

    bool operator==(const Vertex &other) const {
        return pos == other.pos && color == other.color && texCoord == other.texCoord;
    }
};

// clang-format off
/*
inline constexpr auto vertices = std::array{
    Vertex{{-0.5f, -0.5f, 0.0}, {1.0f, 0.0f, 0.0f}, {0.5f, 0.0f}},
    Vertex{{0.5f, -0.5f, 0.0}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
    Vertex{{0.5f, 0.5f, 0.0}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
    Vertex{{-0.5f, 0.5f, 0.0}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}}, 

    Vertex{{-0.5f, -0.5f, -0.5}, {1.0f, 0.0f, 0.0f}, {0.5f, 0.0f}},
    Vertex{{0.5f, -0.5f, -0.5}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
    Vertex{{0.5f, 0.5f, -0.5}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
    Vertex{{-0.5f, 0.5f, -0.5}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}}
};

inline constexpr auto indices = std::array{
    0u, 1u, 2u, 2u, 3u, 0u,
    4u, 5u, 6u, 6u, 7u, 4u
};
*/
// clang-format on
} // namespace vke

template <> struct std::hash<vke::Vertex> {
    size_t operator()(vke::Vertex const &vertex) const {
        return ((hash<glm::vec3>()(vertex.pos) ^ (hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^
               (hash<glm::vec2>()(vertex.texCoord) << 1);
    }
};
