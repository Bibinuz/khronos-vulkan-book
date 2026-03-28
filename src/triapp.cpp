#include "triapp.hpp"

namespace vke {

void TriApp::run() {
    initWindow();
    initVulkan();
    mainLoop();
    cleanup();
}

void TriApp::initWindow() {
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    m_window = glfwCreateWindow(WIDTH, HEIGHT, "vulkan_engine", nullptr, nullptr);
}

void TriApp::initVulkan() { createInstance(); }

void TriApp::createInstance() {}

void TriApp::mainLoop() {
    while (!glfwWindowShouldClose(m_window)) {
        glfwPollEvents();
    }
}

void TriApp::cleanup() {
    glfwDestroyWindow(m_window);
    glfwTerminate();
}

} // namespace vke
