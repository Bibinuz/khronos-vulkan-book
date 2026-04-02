#include "vulkan/vk_platform.h"
#include "vulkan/vulkan.hpp"
#include "vulkan/vulkan_enums.hpp"
#include "vulkan/vulkan_structs.hpp"
#include <GLFW/glfw3.h>
#include <cstdio>
#include <iostream>
#include <print>
#include <vector>
#include <vulkan/vulkan_raii.hpp>

#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif

constexpr auto WIDTH = 1920;
constexpr auto HEIGHT = 1920;

namespace vke {

const std::vector<char const *> validationLayers = {"VK_LAYER_KHRONOS_validation"};

class TriApp {
  public:
    void run();

  private:
    void initWindow();
    void initVulkan();
    void createInstance();
    void mainLoop();
    void cleanup();
    void createSurface();
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createSwapChain();
    void createImageViews();
    void createGraphicsPipeline();

    // Helper functions
    auto getRequiredInstanceExtensions() -> std::vector<const char *>;
    auto isDeviceSuitable(vk::raii::PhysicalDevice const &physicalDevice) -> bool;
    auto chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR> &availableFormats)
        -> vk::SurfaceFormatKHR;
    auto chooseSwapPresentMode(const std::vector<vk::PresentModeKHR> &availableModes)
        -> vk::PresentModeKHR;
    auto chooseSwapExtent(vk::SurfaceCapabilitiesKHR const &capabilities) //
        -> vk::Extent2D;
    auto chooseSwapMinImageCount(vk::SurfaceCapabilitiesKHR const &capabilities) //
        -> std::uint32_t;
    static auto readFile(const std::string &filename) -> std::vector<char>;
    [[nodiscard]] auto createShaderModule(const std::vector<char> &conde)
        -> const vk::raii::ShaderModule;
    // Debug functions
    void setUpDebugMessenger();
    static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(
        vk::DebugUtilsMessageSeverityFlagBitsEXT severity, vk::DebugUtilsMessageTypeFlagsEXT type,
        const vk::DebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData) {
        std::println(stderr, "Validation layer: type '{}' msg: '{}'", to_string(type),
                     pCallbackData->pMessage);

        // Just to diable the unused parameter error
        std::println("Severity: '{}' \n UserData: '{}'", to_string(severity), pUserData);
        return vk::False;
    }
    std::vector<const char *> requiredDeviceExtension = {vk::KHRSwapchainExtensionName};

    GLFWwindow *m_window{};
    vk::raii::Context m_context{};
    vk::raii::Instance m_instance = nullptr;
    vk::raii::DebugUtilsMessengerEXT m_debugMessenger = nullptr;
    vk::raii::SurfaceKHR m_surface = nullptr;
    vk::raii::PhysicalDevice m_physicalDevice = nullptr;
    vk::raii::Device m_device = nullptr;
    vk::PhysicalDeviceFeatures m_deviceFeatures{};
    vk::raii::Queue m_graphicsQueue = nullptr;
    vk::Extent2D m_swapChainExtent{};
    vk::SurfaceFormatKHR m_swapChainSurfaceFormat{};
    vk::raii::SwapchainKHR m_swapChain = nullptr;
    std::vector<vk::Image> m_swapChainImages{};
    std::vector<vk::raii::ImageView> m_swapChainImageViews{};
    vk::raii::PipelineLayout m_pipelineLayount = nullptr;
};
} // namespace vke
