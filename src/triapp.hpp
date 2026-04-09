#pragma once

#include "vulkan/vk_platform.h"
#include "vulkan/vulkan.hpp"
#include "vulkan/vulkan_enums.hpp"
#include "vulkan/vulkan_structs.hpp"
#include <GLFW/glfw3.h>
#include <print>
#include <vector>
#include <vulkan/vulkan_raii.hpp>

namespace vke {

#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif

constexpr auto WIDTH                             = 1920;
constexpr auto HEIGHT                            = 1920;
constexpr std::uint32_t MAX_FRAMES_IN_FLIGHT     = 2;
const std::vector<char const *> validationLayers = {"VK_LAYER_KHRONOS_validation"};

class TriApp {
  public:
    void run();

  private:
    // Main functions
    void initWindow();
    void initVulkan();
    void mainLoop();
    void drawFrame();
    // Create functions
    void createInstance();
    void createSurface();
    void createLogicalDevice();
    void createSwapChain();
    void createImageViews();
    void createGraphicsPipeline();
    void createCommandPool();
    void createCommandBuffers();
    void createSyncObjects();
    void recreateSwapChain();
    void createVertexBuffer();
    void createIndexBuffer();

    // Helper functions
    void pickPhysicalDevice();
    [[nodiscard]] auto getRequiredInstanceExtensions() -> std::vector<const char *>;
    [[nodiscard]] auto isDeviceSuitable(vk::raii::PhysicalDevice const &physicalDevice) -> bool;
    [[nodiscard]] auto
    chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR> &availableFormats)
        -> vk::SurfaceFormatKHR;
    [[nodiscard]] auto chooseSwapPresentMode(const std::vector<vk::PresentModeKHR> &availableModes)
        -> vk::PresentModeKHR;
    [[nodiscard]] auto chooseSwapExtent(vk::SurfaceCapabilitiesKHR const &capabilities) //
        -> vk::Extent2D;
    [[nodiscard]] auto chooseSwapMinImageCount(vk::SurfaceCapabilitiesKHR const &capabilities) //
        -> std::uint32_t;
    [[nodiscard]] static auto readFile(const std::string &filename) -> std::vector<char>;
    [[nodiscard]] auto createShaderModule(std::span<const char> conde)
        -> const vk::raii::ShaderModule;
    void recordCommandBuffer(std::uint32_t imageIndex);
    void transition_image_layout(std::uint32_t imageIndex, vk::ImageLayout old_layout,
                                 vk::ImageLayout new_layout, vk::AccessFlags2 src_access_mask,
                                 vk::AccessFlags2 dst_acces_mask,
                                 vk::PipelineStageFlags2 src_stage_mask,
                                 vk::PipelineStageFlags2 dst_stage_mask);
    static void framebufferResizeCallback(GLFWwindow *window, int width, int height);
    [[nodiscard]] auto findMemoryType(std::uint32_t typeFilter, vk::MemoryPropertyFlags properties)
        -> std::uint32_t;
    void createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage,
                      vk::MemoryPropertyFlags properties, vk::raii::Buffer &buffer,
                      vk::raii::DeviceMemory &bufferMemory);
    void copyBuffer(vk::raii::Buffer &srcBuffer, vk::raii::Buffer &dstBuffer, vk::DeviceSize size);
    // Cleanup functions
    void cleanup();
    void cleanupSwapChain();

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
    vk::raii::Queue m_queue                           = nullptr;
    vk::raii::SwapchainKHR m_swapChain                = nullptr;
    vk::raii::PipelineLayout m_pipelineLayout         = nullptr;
    vk::raii::Pipeline m_graphicsPipeline             = nullptr;
    vk::raii::Buffer m_vertexBuffer                   = nullptr;
    vk::raii::DeviceMemory m_vertexBufferMemory       = nullptr;
    vk::raii::Buffer m_indexBuffer                    = nullptr;
    vk::raii::DeviceMemory m_indexBufferMemory        = nullptr;
    vk::raii::CommandPool m_commandPool               = nullptr;
    std::vector<vk::raii::CommandBuffer> m_commandBuffers{};
    std::vector<vk::raii::Semaphore> m_presentCompleteSemaphores{};
    std::vector<vk::raii::Semaphore> m_renderFinishedSemaphores{};
    std::vector<vk::raii::Fence> m_inFlightFences{};
    std::vector<vk::Image> m_swapChainImages{};
    std::vector<vk::raii::ImageView> m_swapChainImageViews{};
    GLFWwindow *m_window{};
    std::uint32_t m_queueIndex{};
    std::uint32_t m_frameIndex{};
    vk::raii::Context m_context{};
    vk::PhysicalDeviceFeatures m_deviceFeatures{};
    vk::Extent2D m_swapChainExtent{};
    vk::SurfaceFormatKHR m_swapChainSurfaceFormat{};
    bool m_frameBufferResized = false;
};
} // namespace vke
