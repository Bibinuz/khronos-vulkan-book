#include "vulkan/vk_platform.h"
#include "vulkan/vulkan.hpp"
#include "vulkan/vulkan_enums.hpp"
#include "vulkan/vulkan_structs.hpp"
#include <GLFW/glfw3.h>
#include <print>
#include <vector>
#include <vulkan/vulkan_raii.hpp>

#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif

constexpr auto WIDTH  = 1920;
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
    void createCommandPool();
    void createCommandBuffer();
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

    vk::raii::Instance m_instance                     = nullptr;
    vk::raii::DebugUtilsMessengerEXT m_debugMessenger = nullptr;
    vk::raii::SurfaceKHR m_surface                    = nullptr;
    vk::raii::PhysicalDevice m_physicalDevice         = nullptr;
    vk::raii::Device m_device                         = nullptr;
    vk::raii::Queue m_graphicsQueue                   = nullptr;
    vk::raii::SwapchainKHR m_swapChain                = nullptr;
    vk::raii::PipelineLayout m_pipelineLayout         = nullptr;
    vk::raii::Pipeline m_graphicsPipeline             = nullptr;
    vk::raii::CommandPool m_commandPool               = nullptr;
    vk::raii::CommandBuffer m_commandBuffer           = nullptr;
    GLFWwindow *m_window{};
    std::uint32_t m_queueIndex{};
    vk::raii::Context m_context{};
    vk::PhysicalDeviceFeatures m_deviceFeatures{};
    vk::Extent2D m_swapChainExtent{};
    vk::SurfaceFormatKHR m_swapChainSurfaceFormat{};
    std::vector<vk::Image> m_swapChainImages{};
    std::vector<vk::raii::ImageView> m_swapChainImageViews{};
};
} // namespace vke
