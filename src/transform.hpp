#pragma once

#include "glm/ext/matrix_float4x4.hpp"
namespace vke {

struct UniformModelObject {
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
};

} // namespace vke
