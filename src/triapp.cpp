#include "triapp.hpp"

#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <glm/glm.hpp>
#include <limits>
#include <print>
#include <stdexcept>
#include <string>
#include <vector>

#include "GLFW/glfw3.h"
#include "glm/ext/matrix_clip_space.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "glm/ext/vector_float3.hpp"
#include "glm/trigonometric.hpp"
#include "transform.hpp"
#include "vertex.hpp"
#include "vulkan/vulkan.hpp"
#include "vulkan/vulkan_core.h"
#include "vulkan/vulkan_enums.hpp"
#include "vulkan/vulkan_handles.hpp"
#include "vulkan/vulkan_raii.hpp"
#include "vulkan/vulkan_structs.hpp"

namespace vke {

void TriApp::mainLoop() {
    while (!glfwWindowShouldClose(m_window)) {
        glfwPollEvents();
        drawFrame();
    }
    m_device.waitIdle();
}

void TriApp::run() {
    initWindow();
    initVulkan();
    mainLoop();
    cleanup();
}

void TriApp::initWindow() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    m_window = glfwCreateWindow(WIDTH, HEIGHT, "vulkan_engine", nullptr, nullptr);
    glfwSetWindowUserPointer(m_window, this);
    glfwSetFramebufferSizeCallback(m_window, framebufferResizeCallback);
}

void TriApp::framebufferResizeCallback(GLFWwindow *window, [[maybe_unused]] int width,
                                       [[maybe_unused]] int height) {
    auto app                  = reinterpret_cast<TriApp *>(glfwGetWindowUserPointer(window));
    app->m_frameBufferResized = true;
}

// clang-format off
void TriApp::initVulkan() {
    createInstance();
        std::println("Instance Created");
    setUpDebugMessenger();
        std::println("Setted up debug messages");
    createSurface();
        std::println("Surface Created");
    pickPhysicalDevice();
        std::println("Picked physical device");
    createLogicalDevice();
        std::println("Logical device Created");
    createSwapChain();
        std::println("Swapchain Created");
    createImageViews();
        std::println("Image views created");
    createUniformBuffers();
        std::println("Uniform buffers created");
    createDescriptiorSetLayout();
        std::println("Descriptor set layout created");
    createDescriptorPool();
        std::println("Descriptor pool created");
    createDescriptorSets();
        std::println("Descriptor sets created");
    createGraphicsPipeline();
        std::println("Graphics pipeline layout created");
    createCommandPool();
        std::println("Command pool created");
    createVertexBuffer();
        std::println("Vertex buffer created");
    createIndexBuffer();
        std::println("Index buffer created");
    createCommandBuffers();
        std::println("Command buffer created");
    createSyncObjects();
        std::println("Sync objects created");
}
// clang-format on

void TriApp::drawFrame() {
    constexpr auto maxUINT64 = std::numeric_limits<std::uint64_t>::max();
    const auto fenceResult =
        m_device.waitForFences(*m_inFlightFences[m_frameIndex], vk::True,
                               maxUINT64); // Max uint to effectively disables the timeout
    if (fenceResult != vk::Result::eSuccess) {
        throw std::runtime_error("failed to wait for fence!");
    }
    auto [result, imageIndex] =
        m_swapChain.acquireNextImage(maxUINT64, *m_presentCompleteSemaphores[m_frameIndex],
                                     nullptr); // Also max uint to diable timeout
    if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR) {
        std::println("Swapchain is in a suboptimal  or out of date state");
        recreateSwapChain();
        return;
    } else if (result != vk::Result::eSuccess) {
        assert(result == vk::Result::eTimeout || result == vk::Result::eNotReady);
        throw std::runtime_error("Failed to acquire swap chain image!");
    }
    updateUniformBuffer(m_frameIndex);
    m_device.resetFences(*m_inFlightFences[m_frameIndex]);
    m_commandBuffers[m_frameIndex].reset();
    recordCommandBuffer(imageIndex);
    m_queue.waitIdle();
    const auto waitDestinationStageMask =
        vk::PipelineStageFlags{vk::PipelineStageFlagBits::eColorAttachmentOutput};
    const auto submitInfo = vk::SubmitInfo{}
                                .setWaitSemaphoreCount(1)
                                .setPWaitSemaphores(&*m_presentCompleteSemaphores[m_frameIndex])
                                .setPWaitDstStageMask(&waitDestinationStageMask)
                                .setCommandBufferCount(1)
                                .setPCommandBuffers(&*m_commandBuffers[m_frameIndex])
                                .setSignalSemaphoreCount(1)
                                .setPSignalSemaphores(&*m_renderFinishedSemaphores[imageIndex]);
    m_queue.submit(submitInfo, *m_inFlightFences[m_frameIndex]);
    const auto presentInfoKHR = vk::PresentInfoKHR{}
                                    .setWaitSemaphoreCount(1)
                                    .setPWaitSemaphores(&*m_renderFinishedSemaphores[imageIndex])
                                    .setSwapchainCount(1)
                                    .setPSwapchains(&*m_swapChain)
                                    .setPImageIndices(&imageIndex);
    result                    = m_queue.presentKHR(presentInfoKHR);
    switch (result) {
    case vk::Result::eSuccess:
        break;
    case vk::Result::eSuboptimalKHR:
    case vk::Result::eErrorOutOfDateKHR:
        m_frameBufferResized = false;
        recreateSwapChain();
        break;
    default:
        throw std::runtime_error(
            "Something unexpected happened on vk::Queue::presentKHR (function drawFrame)");
        break;
    }
    m_frameIndex = (m_frameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
}

void TriApp::createInstance() {
    const auto appInfo = vk::ApplicationInfo{}
                             .setPApplicationName("Hello triangle")
                             .setApplicationVersion(VK_MAKE_VERSION(1, 0, 0))
                             .setPEngineName("No Engine")
                             .setEngineVersion(VK_MAKE_VERSION(1, 0, 0))
                             .setApiVersion(VK_MAKE_VERSION(1, 4, 0));
    // Get required validation layers
    auto requiredLayers = std::vector<char const *>{};
    if (enableValidationLayers) {
        requiredLayers.assign(validationLayers.begin(), validationLayers.end());
    }
    // Check if validation layers are supported by vulkan implementation
    const auto layerProperties = m_context.enumerateInstanceLayerProperties();
    const auto unsuppoertedLayerIt =
        std::ranges::find_if(requiredLayers, [&layerProperties](auto const &requiredLayer) {
            return std::ranges::none_of(
                layerProperties, [requiredLayer](auto const &layerProperty) {
                    return strcmp(layerProperty.layerName, requiredLayer) == 0;
                });
        });
    if (unsuppoertedLayerIt != requiredLayers.end()) {
        throw std::runtime_error("Required layer not supported: " +
                                 std::string(*unsuppoertedLayerIt));
    }
    // Get all supported extensions
    const auto extensionPropierties = m_context.enumerateInstanceExtensionProperties();
    // Print avialable extensions (can be removed)
    std::println("Available extensions:");
    for (const auto &ext : extensionPropierties) {
        std::println("\t '{}'", std::string(ext.extensionName));
    }
    // Get requires extensions
    const auto requiredExtensions = getRequiredInstanceExtensions();
    // Check if required extensions are supported by Vulkan implementation
    const auto unsupportedPropertyIt = std::ranges::find_if(
        requiredExtensions, [&extensionPropierties](auto const &requiredExtension) {
            return std::ranges::none_of(
                extensionPropierties, [requiredExtension](auto const &extensionPropierty) {
                    return strcmp(extensionPropierty.extensionName, requiredExtension) == 0;
                });
        });
    if (unsupportedPropertyIt != requiredExtensions.end()) {
        throw std::runtime_error("Required extension not supported: " +
                                 std::string(*unsupportedPropertyIt));
    }
    // Create instance
    const auto createInfo =
        vk::InstanceCreateInfo{}
            .setPApplicationInfo(&appInfo)
            .setFlags(vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR)
            .setEnabledLayerCount(static_cast<std::uint32_t>(requiredLayers.size()))
            .setPEnabledLayerNames(requiredLayers)
            .setEnabledExtensionCount(static_cast<std::uint32_t>(requiredExtensions.size()))
            .setPEnabledExtensionNames(requiredExtensions);
    m_instance = vk::raii::Instance(m_context, createInfo);
}

void TriApp::setUpDebugMessenger() {
    if (!enableValidationLayers)
        return;
    const vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
    const vk::DebugUtilsMessageTypeFlagsEXT messageTypeFlags(
        vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
        vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
        vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);
    const auto debugUtilsMessengerCreateInfoEXT = vk::DebugUtilsMessengerCreateInfoEXT{}
                                                      .setMessageSeverity(severityFlags)
                                                      .setMessageType(messageTypeFlags)
                                                      .setPfnUserCallback(&debugCallback);
    m_debugMessenger = m_instance.createDebugUtilsMessengerEXT(debugUtilsMessengerCreateInfoEXT);
}

auto TriApp::getRequiredInstanceExtensions() -> std::vector<const char *> {
    auto glfwExtensionCount = std::uint32_t{0};
    auto glfwExtensions     = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    std::vector extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
    extensions.push_back(vk::KHRPortabilityEnumerationExtensionName);
    if (enableValidationLayers) {
        extensions.push_back(vk::EXTDebugUtilsExtensionName);
    }
    return extensions;
}

auto TriApp::isDeviceSuitable(vk::raii::PhysicalDevice const &physicalDevice) -> bool {
    bool supportsVk13     = physicalDevice.getProperties().apiVersion >= vk::ApiVersion13;
    auto queueFamilies    = physicalDevice.getQueueFamilyProperties();
    bool supportsGraphics = std::ranges::any_of(queueFamilies, [](auto const &qfp) {
        return !!(qfp.queueFlags & vk::QueueFlagBits::eGraphics);
    });
    // check if all required extensions are available
    auto availableDeviceExtensions     = physicalDevice.enumerateDeviceExtensionProperties();
    bool supportsAllRequiredExtensions = std::ranges::all_of(
        requiredDeviceExtension, [&availableDeviceExtensions](auto const &requiredDeviceExtension) {
            return std::ranges::any_of(
                availableDeviceExtensions,
                [requiredDeviceExtension](auto const &availableDeviceExtension) {
                    return strcmp(availableDeviceExtension.extensionName,
                                  requiredDeviceExtension) == 0;
                });
        });
    // check if the physical device supports the required features
    auto features = physicalDevice.template getFeatures2<
        vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan11Features,
        vk::PhysicalDeviceVulkan13Features, vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();
    bool supportsReqFeat =
        features.template get<vk::PhysicalDeviceVulkan11Features>().shaderDrawParameters &&
        features.template get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering &&
        features.template get<vk::PhysicalDeviceVulkan13Features>().synchronization2 &&
        features.template get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>()
            .extendedDynamicState;
    return supportsVk13 && supportsReqFeat && supportsGraphics && supportsAllRequiredExtensions;
}

void TriApp::cleanupSwapChain() {
    m_swapChainImageViews.clear();
    m_swapChain = nullptr;
}

void TriApp::cleanup() {
    glfwDestroyWindow(m_window);
    glfwTerminate();
}

void TriApp::pickPhysicalDevice() {
    auto physicalDevices = m_instance.enumeratePhysicalDevices();
    if (physicalDevices.empty()) {
        throw std::runtime_error("failed to find GPUs with vulkan support!");
    }
    const auto devIter = std::ranges::find_if(physicalDevices, [&](auto const &physicalDevice) {
        return isDeviceSuitable(physicalDevice);
    });
    if (devIter == physicalDevices.end()) {
        throw std::runtime_error("failed to find a suitable GPU!");
    }
    m_physicalDevice = *devIter;
}

void TriApp::createLogicalDevice() {
    const auto queueFP = m_physicalDevice.getQueueFamilyProperties();
    for (std::uint32_t qfpIndex = 0; qfpIndex < queueFP.size(); ++qfpIndex) {
        if ((queueFP[qfpIndex].queueFlags & vk::QueueFlagBits::eGraphics) &&
            m_physicalDevice.getSurfaceSupportKHR(qfpIndex, *m_surface)) {
            m_queueIndex = qfpIndex;
            break;
        }
    }
    // Any new feature i want to enable must go here
    const auto featureChain =
        vk::StructureChain{vk::PhysicalDeviceFeatures2{},
                           vk::PhysicalDeviceVulkan11Features{} //
                               .setShaderDrawParameters(vk::True),
                           vk::PhysicalDeviceVulkan13Features{} //
                               .setDynamicRendering(vk::True)   //
                               .setSynchronization2(vk::True),
                           vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT{} //
                               .setExtendedDynamicState(vk::True)};
    // create the logical device
    const auto priority      = 0.5f;
    const auto deviceQueueCI = vk::DeviceQueueCreateInfo{}
                                   .setQueueFamilyIndex(m_queueIndex)
                                   .setQueueCount(1)
                                   .setPQueuePriorities(&priority);
    const auto deviceCI =
        vk::DeviceCreateInfo{}
            .setPNext(&featureChain.get<vk::PhysicalDeviceFeatures2>())
            .setQueueCreateInfoCount(1)
            .setPQueueCreateInfos(&deviceQueueCI)
            .setEnabledExtensionCount(static_cast<std::uint32_t>(requiredDeviceExtension.size()))
            .setPpEnabledExtensionNames(requiredDeviceExtension.data());
    m_device = vk::raii::Device(m_physicalDevice, deviceCI);
    m_queue  = vk::raii::Queue(m_device, m_queueIndex, 0);
}

void TriApp::createSurface() {
    auto _s = VkSurfaceKHR{};
    if (glfwCreateWindowSurface(*m_instance, m_window, nullptr, &_s) != 0) {
        throw std::runtime_error("Failed to create window surface!");
    }
    m_surface = vk::raii::SurfaceKHR(m_instance, _s);
}

auto TriApp::chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR> &availableFormats)
    -> vk::SurfaceFormatKHR {
    const auto formatIt = std::ranges::find_if(availableFormats, [](const auto &format) {
        return format.format == vk::Format::eB8G8R8A8Srgb &&
               format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
    });
    return formatIt != availableFormats.end() ? *formatIt : availableFormats[0];
}

auto TriApp::chooseSwapPresentMode(const std::vector<vk::PresentModeKHR> &availableModes)
    -> vk::PresentModeKHR {
    assert(std::ranges::any_of(availableModes,
                               [](auto mode) { return mode == vk::PresentModeKHR::eFifo; }));
    return std::ranges::any_of(
               availableModes,
               [](const auto value) { return vk::PresentModeKHR::eMailbox == value; })
               ? vk::PresentModeKHR::eMailbox
               : vk::PresentModeKHR::eFifo;
}
auto TriApp::chooseSwapExtent(vk::SurfaceCapabilitiesKHR const &capabilities) -> vk::Extent2D {
    if (capabilities.currentExtent.width != std::numeric_limits<std::uint32_t>::max()) {
        return capabilities.currentExtent;
    }
    std ::int32_t width{}, height{};
    glfwGetFramebufferSize(m_window, &width, &height);
    return {std::clamp<std::uint32_t>(static_cast<std::uint32_t>(width),
                                      capabilities.minImageExtent.width,
                                      capabilities.maxImageExtent.width),
            std::clamp<std::uint32_t>(static_cast<std::uint32_t>(height),
                                      capabilities.minImageExtent.height,
                                      capabilities.maxImageExtent.height)};
}

auto TriApp::chooseSwapMinImageCount(vk::SurfaceCapabilitiesKHR const &capabilities) //
    -> std::uint32_t {
    auto minImageCount = std::max(3u, capabilities.minImageCount);
    if ((0 < capabilities.maxImageCount) && (capabilities.maxImageCount < minImageCount)) {
        minImageCount = capabilities.maxImageCount;
    }
    return minImageCount;
}

void TriApp::createSwapChain() {
    const auto surfCap          = m_physicalDevice.getSurfaceCapabilitiesKHR(*m_surface);
    m_swapChainExtent           = chooseSwapExtent(surfCap);
    const auto minImageCount    = chooseSwapMinImageCount(surfCap);
    const auto availableFormats = m_physicalDevice.getSurfaceFormatsKHR(*m_surface);
    m_swapChainSurfaceFormat    = chooseSwapSurfaceFormat(availableFormats);
    const auto presentModes     = m_physicalDevice.getSurfacePresentModesKHR(*m_surface);
    const auto swapChainCI      = vk::SwapchainCreateInfoKHR{}
                                      .setSurface(*m_surface)
                                      .setMinImageCount(minImageCount)
                                      .setImageFormat(m_swapChainSurfaceFormat.format)
                                      .setImageColorSpace(m_swapChainSurfaceFormat.colorSpace)
                                      .setImageExtent(m_swapChainExtent)
                                      .setImageArrayLayers(1)
                                      .setImageUsage(vk::ImageUsageFlagBits::eColorAttachment)
                                      .setPreTransform(surfCap.currentTransform)
                                      .setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque)
                                      .setPresentMode(chooseSwapPresentMode(presentModes))
                                      .setClipped(vk::True)
                                      .setOldSwapchain(nullptr);
    m_swapChain                 = vk::raii::SwapchainKHR(m_device, swapChainCI);
    m_swapChainImages           = m_swapChain.getImages();
}

void TriApp::createImageViews() {
    assert(m_swapChainImageViews.empty());
    const auto eI          = vk::ComponentSwizzle::eIdentity;
    const auto components  = vk::ComponentMapping{}.setR(eI).setG(eI).setB(eI).setA(eI);
    const auto subResRange = vk::ImageSubresourceRange{}
                                 .setAspectMask(vk::ImageAspectFlagBits::eColor)
                                 .setLevelCount(1)
                                 .setLayerCount(1);
    auto imageViewCI       = vk::ImageViewCreateInfo{}
                                 .setViewType(vk::ImageViewType::e2D)
                                 .setFormat(m_swapChainSurfaceFormat.format)
                                 .setSubresourceRange(subResRange)
                                 .setComponents(components);
    for (auto &image : m_swapChainImages) {
        imageViewCI.image = image;
        m_swapChainImageViews.emplace_back(m_device, imageViewCI);
    }
}

void TriApp::createDescriptiorSetLayout() {
    const auto uboLayoutBinding = vk::DescriptorSetLayoutBinding{}
                                      .setBinding(0)
                                      .setDescriptorType(vk::DescriptorType::eUniformBuffer)
                                      .setDescriptorCount(1)
                                      .setStageFlags(vk::ShaderStageFlagBits::eVertex)
                                      .setPImmutableSamplers(nullptr);
    const auto layoutCI =
        vk::DescriptorSetLayoutCreateInfo{}.setBindingCount(1).setPBindings(&uboLayoutBinding);
    m_descriptorSetLayout = vk::raii::DescriptorSetLayout(m_device, layoutCI);
}

void TriApp::createGraphicsPipeline() {
    const auto shaderCode          = readFile("./shaders/slang.spv");
    const auto shaderModule        = createShaderModule(shaderCode);
    const auto vertShaderStageInfo = vk::PipelineShaderStageCreateInfo{}
                                         .setStage(vk::ShaderStageFlagBits::eVertex)
                                         .setModule(shaderModule)
                                         .setPName("vertMain");
    const auto fragShaderStageInfo = vk::PipelineShaderStageCreateInfo{}
                                         .setStage(vk::ShaderStageFlagBits::eFragment)
                                         .setModule(shaderModule)
                                         .setPName("fragMain");
    const auto shaderStages =
        std::vector<vk::PipelineShaderStageCreateInfo>{vertShaderStageInfo, fragShaderStageInfo};
    const auto bindingDescription    = Vertex::getBindingDescription();
    const auto attributeDescriptions = Vertex::getAttributeDescriptions();
    const auto vertexInputCI = vk::PipelineVertexInputStateCreateInfo{}
                                   .setVertexBindingDescriptionCount(1)
                                   .setPVertexBindingDescriptions(&bindingDescription)
                                   .setVertexAttributeDescriptionCount(attributeDescriptions.size())
                                   .setPVertexAttributeDescriptions(attributeDescriptions.data());
    const auto inputAssembly = vk::PipelineInputAssemblyStateCreateInfo{}.setTopology(
        vk::PrimitiveTopology::eTriangleList);
    const auto viewPortStateCI =
        vk::PipelineViewportStateCreateInfo{}.setViewportCount(1).setScissorCount(1);
    const auto rasterizerCI  = vk::PipelineRasterizationStateCreateInfo{}
                                   .setDepthClampEnable(vk::False)
                                   .setRasterizerDiscardEnable(vk::False)
                                   .setPolygonMode(vk::PolygonMode::eFill)
                                   .setCullMode(vk::CullModeFlagBits::eBack)
                                   .setFrontFace(vk::FrontFace::eCounterClockwise)
                                   .setDepthBiasEnable(vk::False)
                                   .setLineWidth(1.0f);
    const auto multisampling = vk::PipelineMultisampleStateCreateInfo{}
                                   .setRasterizationSamples(vk::SampleCountFlagBits::e1)
                                   .setSampleShadingEnable(vk::False);
    const auto colorBlendAttachment =
        vk::PipelineColorBlendAttachmentState{}.setBlendEnable(vk::False).setColorWriteMask(
            vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
            vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);
    const auto colorBlendingCI = vk::PipelineColorBlendStateCreateInfo{}
                                     .setLogicOpEnable(vk::False)
                                     .setLogicOp(vk::LogicOp::eCopy)
                                     .setAttachmentCount(1)
                                     .setPAttachments(&colorBlendAttachment);
    const auto dynamicStates =
        std::vector<vk::DynamicState>{vk::DynamicState::eViewport, vk::DynamicState::eScissor};
    const auto dynamicStateCI =
        vk::PipelineDynamicStateCreateInfo{}
            .setDynamicStateCount(static_cast<std::uint32_t>(dynamicStates.size()))
            .setPDynamicStates(dynamicStates.data());
    const auto pipelineLayoutCI = vk::PipelineLayoutCreateInfo{}
                                      .setSetLayoutCount(1)
                                      .setPSetLayouts(&*m_descriptorSetLayout)
                                      .setPushConstantRangeCount(0);
    m_pipelineLayout            = vk::raii::PipelineLayout(m_device, pipelineLayoutCI);
    const auto pipelineRenderingCI =
        vk::PipelineRenderingCreateInfo{}.setColorAttachmentCount(1).setPColorAttachmentFormats(
            &m_swapChainSurfaceFormat.format);
    const auto pipelineCI = vk::GraphicsPipelineCreateInfo{}
                                .setPNext(&pipelineRenderingCI)
                                .setStageCount(2)
                                .setPStages(shaderStages.data())
                                .setPVertexInputState(&vertexInputCI)
                                .setPInputAssemblyState(&inputAssembly)
                                .setPViewportState(&viewPortStateCI)
                                .setPRasterizationState(&rasterizerCI)
                                .setPMultisampleState(&multisampling)
                                .setPColorBlendState(&colorBlendingCI)
                                .setPDynamicState(&dynamicStateCI)
                                .setLayout(m_pipelineLayout)
                                .setRenderPass(nullptr);
    m_graphicsPipeline    = vk::raii::Pipeline(m_device, nullptr, pipelineCI);
}
/*
void TriApp::createVertexBuffer() {
    auto sizeBuffer = vk::DeviceSize{sizeof(vertices[0]) * vertices.size()};
    auto properties =
        vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible;
    createBuffer(sizeBuffer, vk::BufferUsageFlagBits::eVertexBuffer, properties, m_vertexBuffer,
                 m_vertexBufferMemory);
    void *data = m_vertexBufferMemory.mapMemory(0, sizeBuffer);
    std::memcpy(data, vertices.data(), (size_t)sizeBuffer);
    m_vertexBufferMemory.unmapMemory();
}
*/

void TriApp::createIndexBuffer() {
    auto bufferSize                = vk::DeviceSize{sizeof(indices[0]) * indices.size()};
    vk::raii::Buffer stagingBuffer = nullptr;
    vk::raii::DeviceMemory stagingBufferMemory = nullptr;
    auto usage = vk::BufferUsageFlags{vk::BufferUsageFlagBits::eTransferSrc};
    auto properties =
        vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible;
    createBuffer(bufferSize, usage, properties, stagingBuffer, stagingBufferMemory);
    void *data = stagingBufferMemory.mapMemory(0, bufferSize);
    memcpy(data, indices.data(), (size_t)bufferSize);
    stagingBufferMemory.unmapMemory();
    usage      = vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst;
    properties = vk::MemoryPropertyFlagBits::eDeviceLocal;
    createBuffer(bufferSize, usage, properties, m_indexBuffer, m_indexBufferMemory);
    copyBuffer(stagingBuffer, m_indexBuffer, bufferSize);
}

void TriApp::createVertexBuffer() {
    const auto bufferSize = vk::DeviceSize{sizeof(vertices[0]) * vertices.size()};
    // Create staging buffer
    auto usage = vk::BufferUsageFlags{vk::BufferUsageFlagBits::eTransferSrc};
    auto properties =
        vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible;
    vk::raii::Buffer stagingBuffer             = nullptr;
    vk::raii::DeviceMemory stagingBufferMemory = nullptr;
    createBuffer(bufferSize, usage, properties, stagingBuffer, stagingBufferMemory);
    void *dataStaging = stagingBufferMemory.mapMemory(0, bufferSize);
    memcpy(dataStaging, vertices.data(), (size_t)bufferSize);
    stagingBufferMemory.unmapMemory();
    // create vertex buffer
    usage      = vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst;
    properties = vk::MemoryPropertyFlagBits::eDeviceLocal;
    createBuffer(bufferSize, usage, properties, m_vertexBuffer, m_vertexBufferMemory);
    // copy from staging buffer to vertex buffer
    copyBuffer(stagingBuffer, m_vertexBuffer, bufferSize);
}

void TriApp::createUniformBuffers() {
    m_uniformBuffers.clear();
    m_uniformBuffersMemory.clear();
    m_uniformBuffersMapped.clear();
    auto usage = vk::BufferUsageFlags{vk::BufferUsageFlagBits::eUniformBuffer};
    auto properties =
        vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible;
    for (std::size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        const auto bufferSize            = vk::DeviceSize{sizeof(UniformModelObject)};
        vk::raii::Buffer buffer          = nullptr;
        vk::raii::DeviceMemory bufferMem = nullptr;
        createBuffer(bufferSize, usage, properties, buffer, bufferMem);
        m_uniformBuffers.emplace_back(std::move(buffer));
        m_uniformBuffersMemory.emplace_back(std::move(bufferMem));
        m_uniformBuffersMapped.emplace_back(m_uniformBuffersMemory[i].mapMemory(0, bufferSize));
    }
}

void TriApp::createDescriptorPool() {
    const auto poolSize = vk::DescriptorPoolSize{}
                              .setType(vk::DescriptorType::eUniformBuffer)
                              .setDescriptorCount(MAX_FRAMES_IN_FLIGHT);
    const auto poolInfo = vk::DescriptorPoolCreateInfo{}
                              .setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet)
                              .setMaxSets(MAX_FRAMES_IN_FLIGHT)
                              .setPoolSizeCount(1)
                              .setPPoolSizes(&poolSize);
    m_descriptorPool    = vk::raii::DescriptorPool(m_device, poolInfo);
}

void TriApp::createDescriptorSets() {
    const auto layouts =
        std::vector<vk::DescriptorSetLayout>(MAX_FRAMES_IN_FLIGHT, *m_descriptorSetLayout);
    const auto descriptorSetAI =
        vk::DescriptorSetAllocateInfo{}
            .setDescriptorPool(m_descriptorPool)
            .setDescriptorSetCount(static_cast<std::uint32_t>(layouts.size()))
            .setPSetLayouts(layouts.data());
    m_descriptorSets.clear();
    m_descriptorSets = m_device.allocateDescriptorSets(descriptorSetAI);
    for (std::size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        const auto bufferInfo =
            vk::DescriptorBufferInfo{}
                .setBuffer(m_uniformBuffers[i])
                .setOffset(0)
                //.setRange(VK_WHOLE_SIZE) if we are overriding the hole buffer this is equivalent
                .setRange(vk::DeviceSize{sizeof(UniformModelObject)});
        const auto descriptorWrite = vk::WriteDescriptorSet{}
                                         .setDstSet(m_descriptorSets[i])
                                         .setDstBinding(0)
                                         .setDstArrayElement(0)
                                         .setDescriptorCount(1)
                                         .setDescriptorType(vk::DescriptorType::eUniformBuffer)
                                         .setPBufferInfo(&bufferInfo);
        m_device.updateDescriptorSets(descriptorWrite, {});
    }
}

void TriApp::createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage,
                          vk::MemoryPropertyFlags properties, vk::raii::Buffer &buffer,
                          vk::raii::DeviceMemory &bufferMemory) {
    const auto bufferCI = vk::BufferCreateInfo{}.setSize(size).setUsage(usage).setSharingMode(
        vk::SharingMode::eExclusive);
    buffer                     = vk::raii::Buffer(m_device, bufferCI);
    const auto memRequirements = buffer.getMemoryRequirements();
    const auto memoryAI =
        vk::MemoryAllocateInfo{}
            .setAllocationSize(memRequirements.size)
            .setMemoryTypeIndex(findMemoryType(memRequirements.memoryTypeBits, properties));
    bufferMemory = vk::raii::DeviceMemory(m_device, memoryAI);
    buffer.bindMemory(*bufferMemory, 0);
}

void TriApp::copyBuffer(vk::raii::Buffer &srcBuffer, vk::raii::Buffer &dstBuffer,
                        vk::DeviceSize size) {
    const auto commandBufferAI   = vk::CommandBufferAllocateInfo{}
                                       .setCommandPool(m_commandPool)
                                       .setLevel(vk::CommandBufferLevel::ePrimary)
                                       .setCommandBufferCount(1);
    const auto commandCopyBuffer = vk::raii::CommandBuffer(
        std::move(m_device.allocateCommandBuffers(commandBufferAI).front()));
    commandCopyBuffer.begin(
        vk::CommandBufferBeginInfo{}.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));
    commandCopyBuffer.copyBuffer(srcBuffer, dstBuffer, vk::BufferCopy(0, 0, size));
    commandCopyBuffer.end();
    m_queue.submit(
        vk::SubmitInfo{}.setCommandBufferCount(1).setPCommandBuffers(&*commandCopyBuffer), nullptr);
    m_queue.waitIdle();
}

void TriApp::updateUniformBuffer(std::uint32_t currentImg) {
    static auto startTime  = std::chrono::high_resolution_clock::now();
    const auto currentTime = std::chrono::high_resolution_clock::now();
    const auto time        = std::chrono::duration<float>(currentTime - startTime).count();
    auto ubo               = UniformModelObject{};
    ubo.model =
        glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f),
                           glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.proj = glm::perspective(glm::radians(120.0f),
                                static_cast<float>(m_swapChainExtent.width) /
                                    static_cast<float>(m_swapChainExtent.height),
                                0.1f, 10.f);
    ubo.proj[1][1] *= -1;
    memcpy(m_uniformBuffersMapped[currentImg], &ubo, sizeof(ubo));
}

auto TriApp::findMemoryType(std::uint32_t typeFilter, vk::MemoryPropertyFlags properties)
    -> std::uint32_t {
    auto memProperties = m_physicalDevice.getMemoryProperties();
    for (std::uint32_t i = 0; i < memProperties.memoryTypeCount; ++i) {
        if ((typeFilter & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    throw std::runtime_error("failed to find suitable memory type");
}

void TriApp::createCommandPool() {
    const auto poolCI = vk::CommandPoolCreateInfo{}
                            .setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer)
                            .setQueueFamilyIndex(m_queueIndex);
    m_commandPool     = vk::raii::CommandPool(m_device, poolCI);
}

void TriApp::createCommandBuffers() {
    const auto commandBufferAI = vk::CommandBufferAllocateInfo{}
                                     .setCommandPool(m_commandPool)
                                     .setLevel(vk::CommandBufferLevel::ePrimary)
                                     .setCommandBufferCount(MAX_FRAMES_IN_FLIGHT);
    m_commandBuffers           = vk::raii::CommandBuffers(m_device, commandBufferAI);
}

void TriApp::createSyncObjects() {
    assert(m_presentCompleteSemaphores.empty() && m_renderFinishedSemaphores.empty() &&
           m_inFlightFences.empty());
    for (std::size_t i = 0; i < m_swapChainImages.size(); ++i) {
        m_renderFinishedSemaphores.emplace_back(m_device, vk::SemaphoreCreateInfo{});
    }
    for (std::size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        m_presentCompleteSemaphores.emplace_back(m_device, vk::SemaphoreCreateInfo{});
        m_inFlightFences.emplace_back(
            m_device, vk::FenceCreateInfo{}.setFlags(vk::FenceCreateFlagBits::eSignaled));
    }
}

auto TriApp::createShaderModule(std::span<const char> code) -> const vk::raii::ShaderModule {
    const auto shaderModuleCI = vk::ShaderModuleCreateInfo{}
                                    .setCodeSize(code.size() * sizeof(char))
                                    .setPCode(reinterpret_cast<const std::uint32_t *>(code.data()));
    vk::raii::ShaderModule shaderModule{m_device, shaderModuleCI};
    return shaderModule;
}

auto TriApp::readFile(const std::string &fileName) -> std::vector<char> {
    std::ifstream file(fileName, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + fileName);
    }
    std::vector<char> buffer(static_cast<std::size_t>(file.tellg()));
    file.seekg(0, std::ios::beg);
    file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    file.close();
    return buffer;
}

void TriApp::recordCommandBuffer(std::uint32_t imageIndex) {
    auto &commandBuffer = m_commandBuffers[m_frameIndex];
    commandBuffer.begin({});
    transition_image_layout(imageIndex, vk::ImageLayout::eUndefined,
                            vk::ImageLayout::eColorAttachmentOptimal, {},
                            vk::AccessFlagBits2::eColorAttachmentWrite,
                            vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                            vk::PipelineStageFlagBits2::eColorAttachmentOutput);
    constexpr auto clearColor = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);
    const auto attachmentInfo = vk::RenderingAttachmentInfo{}
                                    .setImageView(m_swapChainImageViews[imageIndex])
                                    .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
                                    .setLoadOp(vk::AttachmentLoadOp::eClear)
                                    .setStoreOp(vk::AttachmentStoreOp::eStore)
                                    .setClearValue(clearColor);
    const auto renderingInfo =
        vk::RenderingInfo{}
            .setRenderArea(vk::Rect2D{}.setOffset({0, 0}).setExtent(m_swapChainExtent))
            .setLayerCount(1)
            .setColorAttachmentCount(1)
            .setPColorAttachments(&attachmentInfo);
    commandBuffer.beginRendering(renderingInfo);
    commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *m_graphicsPipeline);
    commandBuffer.bindVertexBuffers(0, *m_vertexBuffer, {0});
    commandBuffer.bindIndexBuffer(*m_indexBuffer, 0, vk::IndexType::eUint32);
    commandBuffer.setViewport(0, vk::Viewport{}
                                     .setX(0.0f)
                                     .setY(0.0f)
                                     .setWidth(static_cast<float>(m_swapChainExtent.width))
                                     .setHeight(static_cast<float>(m_swapChainExtent.height))
                                     .setMinDepth(0.0f)
                                     .setMaxDepth(1.0f));
    commandBuffer.setScissor(0, vk::Rect2D{}.setOffset({0, 0}).setExtent(m_swapChainExtent));
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipelineLayout, 0,
                                     *m_descriptorSets[m_frameIndex], nullptr);
    // Num of vertices and instances to draw
    commandBuffer.drawIndexed(indices.size(), 1, 0, 0, 0);
    commandBuffer.endRendering();
    transition_image_layout(imageIndex, vk::ImageLayout::eColorAttachmentOptimal,
                            vk::ImageLayout::ePresentSrcKHR,
                            vk::AccessFlagBits2::eColorAttachmentWrite, {},
                            vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                            vk::PipelineStageFlagBits2::eBottomOfPipe);
    commandBuffer.end();
}

void TriApp::transition_image_layout(std::uint32_t imageIndex, vk::ImageLayout old_layout,
                                     vk::ImageLayout new_layout, vk::AccessFlags2 src_access_mask,
                                     vk::AccessFlags2 dst_acces_mask,
                                     vk::PipelineStageFlags2 src_stage_mask,
                                     vk::PipelineStageFlags2 dst_stage_mask) {
    const auto subResRange    = vk::ImageSubresourceRange{}
                                    .setAspectMask(vk::ImageAspectFlagBits::eColor)
                                    .setBaseMipLevel(0)
                                    .setLevelCount(1)
                                    .setBaseArrayLayer(0)
                                    .setLayerCount(1);
    const auto barrier        = vk::ImageMemoryBarrier2{}
                                    .setSrcStageMask(src_stage_mask)
                                    .setSrcAccessMask(src_access_mask)
                                    .setDstStageMask(dst_stage_mask)
                                    .setDstAccessMask(dst_acces_mask)
                                    .setOldLayout(old_layout)
                                    .setNewLayout(new_layout)
                                    .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                                    .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                                    .setImage(m_swapChainImages[imageIndex])
                                    .setSubresourceRange(subResRange);
    const auto dependencyInfo = vk::DependencyInfo{}
                                    .setDependencyFlags({})
                                    .setImageMemoryBarrierCount(1)
                                    .setPImageMemoryBarriers(&barrier);
    m_commandBuffers[m_frameIndex].pipelineBarrier2(dependencyInfo);
}

void TriApp::recreateSwapChain() {
    m_device.waitIdle();
    cleanupSwapChain();
    createSwapChain();
    createImageViews();
}

} // namespace vke
