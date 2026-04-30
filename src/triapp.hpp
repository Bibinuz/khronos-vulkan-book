#pragma once

#include "vertex.hpp"
#include "vulkan/vk_platform.h"
#include "vulkan/vulkan.hpp"
#include "vulkan/vulkan_enums.hpp"
#include "vulkan/vulkan_handles.hpp"
#include "vulkan/vulkan_structs.hpp"
#include <GLFW/glfw3.h>
#include <cstdint>
#include <print>
#include <string>
#include <utility>
#include <vector>
#include <vulkan/vulkan_raii.hpp>

namespace vke {

#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif

constexpr auto WIDTH                             = 1920u;
constexpr auto HEIGHT                            = 1920u;
constexpr std::uint32_t MAX_FRAMES_IN_FLIGHT     = 2;
const std::vector<char const *> validationLayers = {"VK_LAYER_KHRONOS_validation"};

const std::string MODEL_PATH                     = "models/viking_room.obj";
const std::string TEXTURE_PATH                   = "textures/viking_room.png";

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
    void createDescriptiorSetLayout();
    void createGraphicsPipeline();
    void createCommandPool();
    void createCommandBuffers();
    void createSyncObjects();
    void recreateSwapChain();
    void createVertexBuffer();
    void createIndexBuffer();
    void createUniformBuffers();
    void createDescriptorPool();
    void createDescriptorSets();
    void createTextureImage();
    void createTextureImageView();
    void createTextureSampler();
    void createDepthResources();

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
    auto createShaderModule(std::span<const char> conde) -> const vk::raii::ShaderModule;
    auto findMemoryType(std::uint32_t typeFilter, vk::MemoryPropertyFlags properties)
        -> std::uint32_t;
    void pickPhysicalDevice();
    void recordCommandBuffer(std::uint32_t imageIndex);
    void transition_image_layout(vk::Image image, vk::ImageLayout old_layout,
                                 vk::ImageLayout new_layout, vk::AccessFlags2 src_access_mask,
                                 vk::AccessFlags2 dst_acces_mask,
                                 vk::PipelineStageFlags2 src_stage_mask,
                                 vk::PipelineStageFlags2 dst_stage_mask,
                                 vk::ImageAspectFlags image_aspect_flags);
    void createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage,
                      vk::MemoryPropertyFlags properties, vk::raii::Buffer &buffer,
                      vk::raii::DeviceMemory &bufferMemory);
    auto createStagingBuffer(vk::DeviceSize bufferSize, void *data)
        -> std::pair<vk::raii::Buffer, vk::raii::DeviceMemory>;
    void copyBuffer(vk::raii::Buffer &srcBuffer, vk::raii::Buffer &dstBuffer, vk::DeviceSize size);
    void updateUniformBuffer(std::uint32_t currentImg);
    void createImage(std::uint32_t widht, std::uint32_t height, std::uint32_t mipLevels,
                     vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage,
                     vk::MemoryPropertyFlags properties, vk::raii::Image &image,
                     vk::raii::DeviceMemory &imageMemory);
    static void framebufferResizeCallback(GLFWwindow *window, int width, int height);
    auto beginSingleTimeCommands() -> vk::raii::CommandBuffer;
    void endSingleTimeCommands(vk::raii::CommandBuffer &commandBuffer);
    void transitionImageLayout(const vk::raii::Image &image, vk::ImageLayout oldLayout,
                               vk::ImageLayout newLayout, std::uint32_t mipLevels);
    void copyBufferToImage(const vk::raii::Buffer &buffer, vk::raii::Image &image,
                           std::uint32_t widht, std::uint32_t height);
    auto createImageView(const vk::Image &image, vk::Format format,
                         vk::ImageAspectFlags aspectFlags, std::uint32_t mipLevels)
        -> vk::raii::ImageView;
    auto findSupportedFormat(const std::vector<vk::Format> &candidates, vk::ImageTiling tiling,
                             vk::FormatFeatureFlags features) -> vk::Format;
    void loadModel();
    void generateMipmaps(vk::raii::Image &image, vk::Format imageFormat, std::int32_t texWidth,
                         std::int32_t texHeight, std::uint32_t mipLevels);

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
    std::vector<const char *> requiredDeviceExtension   = {vk::KHRSwapchainExtensionName};

    vk::raii::Instance m_instance                       = nullptr;
    vk::raii::DebugUtilsMessengerEXT m_debugMessenger   = nullptr;
    vk::raii::SurfaceKHR m_surface                      = nullptr;
    vk::raii::PhysicalDevice m_physicalDevice           = nullptr;
    vk::raii::Device m_device                           = nullptr;
    vk::raii::Queue m_queue                             = nullptr;
    vk::raii::SwapchainKHR m_swapChain                  = nullptr;
    vk::raii::DescriptorSetLayout m_descriptorSetLayout = nullptr;
    vk::raii::PipelineLayout m_pipelineLayout           = nullptr;
    vk::raii::Pipeline m_graphicsPipeline               = nullptr;
    vk::raii::Buffer m_vertexBuffer                     = nullptr;
    vk::raii::DeviceMemory m_vertexBufferMemory         = nullptr;
    vk::raii::Buffer m_indexBuffer                      = nullptr;
    vk::raii::DeviceMemory m_indexBufferMemory          = nullptr;
    vk::raii::CommandPool m_commandPool                 = nullptr;
    vk::raii::DescriptorPool m_descriptorPool           = nullptr;
    vk::raii::Image m_textureImage                      = nullptr;
    vk::raii::DeviceMemory m_textureImageMemory         = nullptr;
    vk::raii::ImageView m_textureImageView              = nullptr;
    vk::raii::Sampler m_textureSampler                  = nullptr;
    vk::raii::Image m_depthImage                        = nullptr;
    vk::raii::DeviceMemory m_depthImageMemory           = nullptr;
    vk::raii::ImageView m_depthImageView                = nullptr;
    vk::raii::Context m_context{};
    std::vector<vk::raii::Buffer> m_uniformBuffers{};
    std::vector<vk::raii::DeviceMemory> m_uniformBuffersMemory{};
    std::vector<vk::raii::CommandBuffer> m_commandBuffers{};
    std::vector<vk::raii::Semaphore> m_presentCompleteSemaphores{};
    std::vector<vk::raii::Semaphore> m_renderFinishedSemaphores{};
    std::vector<vk::raii::Fence> m_inFlightFences{};
    std::vector<vk::raii::ImageView> m_swapChainImageViews{};
    std::vector<vk::raii::DescriptorSet> m_descriptorSets{};
    std::vector<vk::Image> m_swapChainImages{};
    std::vector<void *> m_uniformBuffersMapped{};
    GLFWwindow *m_window{};
    std::uint32_t m_queueIndex{};
    std::uint32_t m_frameIndex{};
    vk::PhysicalDeviceFeatures m_deviceFeatures{};
    vk::Extent2D m_swapChainExtent{};
    vk::SurfaceFormatKHR m_swapChainSurfaceFormat{};
    vk::Format m_depthFormat{};
    bool m_frameBufferResized = false;
    std::vector<Vertex> m_vertices;
    std::vector<std::uint32_t> m_indices;
    std::uint32_t m_mipLevels;
};
} // namespace vke
