#pragma once

#include "vulkan/vulkan_enums.hpp"
#include "vulkan/vulkan_structs.hpp"
#include <array>
#include <cstddef>
#include <glm/glm.hpp>

namespace vke {

struct Vertex {
    glm::vec2 pos{};
    glm::vec3 color{};

    static auto getBindingDescription() -> vk::VertexInputBindingDescription {
        constexpr auto vertexInputBD = vk::VertexInputBindingDescription{}
                                           .setBinding(0)
                                           .setStride(sizeof(Vertex))
                                           .setInputRate(vk::VertexInputRate::eVertex);
        return vertexInputBD;
    }
    static auto getAttributeDescriptions() -> std::array<vk::VertexInputAttributeDescription, 2> {
        constexpr auto VertexInputADs = std::array{vk::VertexInputAttributeDescription{}
                                                       .setLocation(0)
                                                       .setBinding(0)
                                                       .setFormat(vk::Format::eR32G32Sfloat)
                                                       .setOffset(offsetof(Vertex, pos)),
                                                   vk::VertexInputAttributeDescription{}
                                                       .setLocation(1)
                                                       .setBinding(0)
                                                       .setFormat(vk::Format::eR32G32B32Sfloat)
                                                       .setOffset(offsetof(Vertex, color))};
        return VertexInputADs;
    }
};

// clang-format off
const std::vector<Vertex> vertices = {
    {{0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
    {{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
    {{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}, 
    {{0.0f, -0.5f}, {0.0f, 0.0f, 1.0f}},
    {{0.5f, 0.5f}, {0.0f, 1.0f, .0f}},
    {{0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}},
};
// clang-format on
} // namespace vke
