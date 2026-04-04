#include "triapp.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <print>
#include <stdexcept>
#include <string>
#include <vector>

#include "GLFW/glfw3.h"
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

void TriApp::initVulkan() {
    createInstance();
    std::println("Instance Created");
    setUpDebugMessenger();
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
    createGraphicsPipeline();
    std::println("Graphics pipeline layout created");
    createCommandPool();
    std::println("Command pool created");
    createCommandBuffers();
    std::println("Command buffer created");
    createSyncObjects();
    std::println("Sync objects created");
}

void TriApp::drawFrame() {
    constexpr auto maxUINT64 = std::numeric_limits<std::uint64_t>::max();
    auto fenceResult =
        m_device.waitForFences(*m_inFlightFences[m_frameIndex], vk::True,
                               maxUINT64); // Max uint to effectively disables the timeout
    if (fenceResult != vk::Result::eSuccess) {
        throw std::runtime_error("failed to wait for fence!");
    }
    auto [result, imageIndex] =
        m_swapChain.acquireNextImage(maxUINT64, *m_presentCompleteSemaphores[m_frameIndex],
                                     nullptr); // Also max uint to diable timeout
    if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR ||
        m_frameBufferResized) {
        recreateSwapChain();
        return;
    } else if (result != vk::Result::eSuccess) {
        assert(result == vk::Result::eTimeout || result == vk::Result::eNotReady);
        throw std::runtime_error("Failed to acquire swap chain image!");
    }

    m_device.resetFences(*m_inFlightFences[m_frameIndex]);
    m_commandBuffers[m_frameIndex].reset();
    recordCommandBuffer(imageIndex);
    m_queue.waitIdle();
    auto waitDestinationStageMask =
        vk::PipelineStageFlags{vk::PipelineStageFlagBits::eColorAttachmentOutput};
    auto const submitInfo = vk::SubmitInfo{}
                                .setWaitSemaphoreCount(1)
                                .setPWaitSemaphores(&*m_presentCompleteSemaphores[m_frameIndex])
                                .setPWaitDstStageMask(&waitDestinationStageMask)
                                .setCommandBufferCount(1)
                                .setPCommandBuffers(&*m_commandBuffers[m_frameIndex])
                                .setSignalSemaphoreCount(1)
                                .setPSignalSemaphores(&*m_renderFinishedSemaphores[m_frameIndex]);
    m_queue.submit(submitInfo, *m_inFlightFences[m_frameIndex]);
    auto const presentInfoKHR = vk::PresentInfoKHR{}
                                    .setWaitSemaphoreCount(1)
                                    .setPWaitSemaphores(&*m_renderFinishedSemaphores[m_frameIndex])
                                    .setSwapchainCount(1)
                                    .setPSwapchains(&*m_swapChain)
                                    .setPImageIndices(&imageIndex);
    result                    = m_queue.presentKHR(presentInfoKHR);
    switch (result) {
    case vk::Result::eSuccess:
        break;
    case vk::Result::eSuboptimalKHR:
    case vk::Result::eErrorOutOfDateKHR:
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
    auto appInfo = vk::ApplicationInfo{};
    appInfo.setPApplicationName("Hello triangle")
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
    auto layerProperties     = m_context.enumerateInstanceLayerProperties();
    auto unsuppoertedLayerIt = std::ranges::find_if(requiredLayers, [&layerProperties](
                                                                        auto const &requiredLayer) {
        return std::ranges::none_of(layerProperties, [requiredLayer](auto const &layerProperty) {
            return strcmp(layerProperty.layerName, requiredLayer) == 0;
        });
    });
    if (unsuppoertedLayerIt != requiredLayers.end()) {
        throw std::runtime_error("Required layer not supported: " +
                                 std::string(*unsuppoertedLayerIt));
    }
    // Get all supported extensions
    auto const extensionPropierties = m_context.enumerateInstanceExtensionProperties();
    // Print avialable extensions (can be removed)
    std::println("Available extensions:");
    for (const auto &ext : extensionPropierties) {
        std::println("\t '{}'", std::string(ext.extensionName));
    }
    // Get requires extensions
    auto requiredExtensions = getRequiredInstanceExtensions();
    // Check if required extensions are supported by Vulkan implementation
    auto unsupportedPropertyIt = std::ranges::find_if(
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
    auto createInfo = vk::InstanceCreateInfo{};
    createInfo.setPApplicationInfo(&appInfo)
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
    vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
    vk::DebugUtilsMessageTypeFlagsEXT messageTypeFlags(
        vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
        vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
        vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);
    auto debugUtilsMessengerCreateInfoEXT = vk::DebugUtilsMessengerCreateInfoEXT{};
    debugUtilsMessengerCreateInfoEXT.setMessageSeverity(severityFlags)
        .setMessageType(messageTypeFlags)
        .setPfnUserCallback(&debugCallback);
    m_debugMessenger = m_instance.createDebugUtilsMessengerEXT(debugUtilsMessengerCreateInfoEXT);
}

auto TriApp::getRequiredInstanceExtensions() -> std::vector<const char *> {
    auto glfwExtensionCount = std::uint32_t{0};
    auto glfwExtensions     = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    std::vector extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
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
    auto const devIter = std::ranges::find_if(physicalDevices, [&](auto const &physicalDevice) {
        return isDeviceSuitable(physicalDevice);
    });
    if (devIter == physicalDevices.end()) {
        throw std::runtime_error("failed to find a suitable GPU!");
    }
    m_physicalDevice = *devIter;
}

void TriApp::createLogicalDevice() {
    auto queueFP = m_physicalDevice.getQueueFamilyProperties();
    for (std::uint32_t qfpIndex = 0; qfpIndex < queueFP.size(); ++qfpIndex) {
        if ((queueFP[qfpIndex].queueFlags & vk::QueueFlagBits::eGraphics) &&
            m_physicalDevice.getSurfaceSupportKHR(qfpIndex, *m_surface)) {
            m_queueIndex = qfpIndex;
            break;
        }
    }
    // Any new feature i want to enable must go here
    vk::StructureChain featureChain{
        vk::PhysicalDeviceFeatures2{},
        vk::PhysicalDeviceVulkan11Features{}.setShaderDrawParameters(vk::True),
        vk::PhysicalDeviceVulkan13Features{}.setDynamicRendering(vk::True).setSynchronization2(
            vk::True),
        vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT{}.setExtendedDynamicState(vk::True)};
    // create the logical device
    auto const priority = 0.5f;
    auto deviceQueueCI  = vk::DeviceQueueCreateInfo{};
    deviceQueueCI.setQueueFamilyIndex(m_queueIndex).setQueueCount(1).setPQueuePriorities(&priority);
    auto deviceCI = vk::DeviceCreateInfo{};
    deviceCI.setPNext(&featureChain.get<vk::PhysicalDeviceFeatures2>())
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
auto TriApp::chooseSwapExtent(vk::SurfaceCapabilitiesKHR const &capabilities) //
    -> vk::Extent2D {
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
    auto surfCap             = m_physicalDevice.getSurfaceCapabilitiesKHR(*m_surface);
    m_swapChainExtent        = chooseSwapExtent(surfCap);
    auto minImageCount       = chooseSwapMinImageCount(surfCap);
    auto availableFormats    = m_physicalDevice.getSurfaceFormatsKHR(*m_surface);
    m_swapChainSurfaceFormat = chooseSwapSurfaceFormat(availableFormats);
    auto presentModes        = m_physicalDevice.getSurfacePresentModesKHR(*m_surface);
    // std::uint32_t imageCount = surfCap.minImageCount + 1;
    auto swapChainCI = vk::SwapchainCreateInfoKHR{};
    swapChainCI.setSurface(*m_surface)
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
    m_swapChain       = vk::raii::SwapchainKHR(m_device, swapChainCI);
    m_swapChainImages = m_swapChain.getImages();
}

void TriApp::createImageViews() {
    assert(m_swapChainImageViews.empty());
    auto eI         = vk::ComponentSwizzle::eIdentity;
    auto components = vk::ComponentMapping{};
    components.setR(eI).setG(eI).setB(eI).setA(eI);
    auto subResRange = vk::ImageSubresourceRange{};
    subResRange.setAspectMask(vk::ImageAspectFlagBits::eColor).setLevelCount(1).setLayerCount(1);
    auto imageViewCI = vk::ImageViewCreateInfo{};
    imageViewCI.setViewType(vk::ImageViewType::e2D)
        .setFormat(m_swapChainSurfaceFormat.format)
        .setSubresourceRange(subResRange)
        .setComponents(components);
    for (auto &image : m_swapChainImages) {
        imageViewCI.image = image;
        m_swapChainImageViews.emplace_back(m_device, imageViewCI);
    }
}

void TriApp::createGraphicsPipeline() {
    auto shaderCode          = readFile("./shaders/slang.spv");
    auto shaderModule        = createShaderModule(shaderCode);
    auto vertShaderStageInfo = vk::PipelineShaderStageCreateInfo{};
    vertShaderStageInfo.setStage(vk::ShaderStageFlagBits::eVertex)
        .setModule(shaderModule)
        .setPName("vertMain");
    auto fragShaderStageInfo = vk::PipelineShaderStageCreateInfo{};
    fragShaderStageInfo.setStage(vk::ShaderStageFlagBits::eFragment)
        .setModule(shaderModule)
        .setPName("fragMain");
    auto shaderStages =
        std::vector<vk::PipelineShaderStageCreateInfo>{vertShaderStageInfo, fragShaderStageInfo};
    auto vertexInputCI = vk::PipelineVertexInputStateCreateInfo{};
    auto inputAssembly = vk::PipelineInputAssemblyStateCreateInfo{};
    inputAssembly.setTopology(vk::PrimitiveTopology::eTriangleList);
    auto viewPortStateCI = vk::PipelineViewportStateCreateInfo{};
    viewPortStateCI.setViewportCount(1).setScissorCount(1);
    auto rasterizerCI = vk::PipelineRasterizationStateCreateInfo{};
    rasterizerCI.setDepthClampEnable(vk::False)
        .setRasterizerDiscardEnable(vk::False)
        .setPolygonMode(vk::PolygonMode::eFill)
        .setCullMode(vk::CullModeFlagBits::eBack)
        .setFrontFace(vk::FrontFace::eClockwise)
        .setDepthBiasEnable(vk::False)
        .setLineWidth(1.0f);
    auto multisampling = vk::PipelineMultisampleStateCreateInfo{};
    multisampling.setRasterizationSamples(vk::SampleCountFlagBits::e1)
        .setSampleShadingEnable(vk::False);
    auto colorBlendAttachment = vk::PipelineColorBlendAttachmentState{};
    colorBlendAttachment.setBlendEnable(vk::False).setColorWriteMask(
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
        vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);
    auto colorBlendingCI = vk::PipelineColorBlendStateCreateInfo{};
    colorBlendingCI.setLogicOpEnable(vk::False)
        .setLogicOp(vk::LogicOp::eCopy)
        .setAttachmentCount(1)
        .setPAttachments(&colorBlendAttachment);
    auto dynamicStates =
        std::vector<vk::DynamicState>{vk::DynamicState::eViewport, vk::DynamicState::eScissor};
    auto dynamicStateCI = vk::PipelineDynamicStateCreateInfo{};
    dynamicStateCI.setDynamicStateCount(static_cast<std::uint32_t>(dynamicStates.size()))
        .setPDynamicStates(dynamicStates.data());
    auto pipelineLayoutCI = vk::PipelineLayoutCreateInfo{};
    pipelineLayoutCI.setSetLayoutCount(0).setPushConstantRangeCount(0);
    m_pipelineLayout         = vk::raii::PipelineLayout(m_device, pipelineLayoutCI);
    auto pipelineRenderingCI = vk::PipelineRenderingCreateInfo{};
    pipelineRenderingCI.setColorAttachmentCount(1).setPColorAttachmentFormats(
        &m_swapChainSurfaceFormat.format);
    auto pipelineCI = vk::GraphicsPipelineCreateInfo{};
    pipelineCI.setPNext(&pipelineRenderingCI)
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
    m_graphicsPipeline = vk::raii::Pipeline(m_device, nullptr, pipelineCI);
}

void TriApp::createCommandPool() {
    auto poolCI = vk::CommandPoolCreateInfo{};
    poolCI.setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer)
        .setQueueFamilyIndex(m_queueIndex);
    m_commandPool = vk::raii::CommandPool(m_device, poolCI);
}

void TriApp::createCommandBuffers() {
    auto commandBufferAI = vk::CommandBufferAllocateInfo{};
    commandBufferAI.setCommandPool(m_commandPool)
        .setLevel(vk::CommandBufferLevel::ePrimary)
        .setCommandBufferCount(MAX_FRAMES_IN_FLIGHT);
    m_commandBuffers = vk::raii::CommandBuffers(m_device, commandBufferAI);
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

auto TriApp::createShaderModule(const std::vector<char> &code) -> const vk::raii::ShaderModule {
    auto shaderModuleCI = vk::ShaderModuleCreateInfo{};
    shaderModuleCI.setCodeSize(code.size() * sizeof(char))
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
    auto clearColor     = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);
    auto attachmentInfo = vk::RenderingAttachmentInfo{};
    attachmentInfo.setImageView(m_swapChainImageViews[imageIndex])
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eStore)
        .setClearValue(clearColor);
    auto renderingInfo = vk::RenderingInfo{};
    renderingInfo.setRenderArea(vk::Rect2D{}.setOffset({0, 0}).setExtent(m_swapChainExtent))
        .setLayerCount(1)
        .setColorAttachmentCount(1)
        .setPColorAttachments(&attachmentInfo);
    commandBuffer.beginRendering(renderingInfo);
    commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *m_graphicsPipeline);
    commandBuffer.setViewport(0, vk::Viewport{}
                                     .setX(0.0f)
                                     .setY(0.0f)
                                     .setWidth(static_cast<float>(m_swapChainExtent.width))
                                     .setHeight(static_cast<float>(m_swapChainExtent.height))
                                     .setMinDepth(0.0f)
                                     .setMaxDepth(1.0f));
    commandBuffer.setScissor(0, vk::Rect2D{}.setOffset({0, 0}).setExtent(m_swapChainExtent));
    // Num of vertices and instances to draw
    commandBuffer.draw(3, 1, 0, 0);
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
    auto barrier = vk::ImageMemoryBarrier2{};
    barrier.setSrcStageMask(src_stage_mask)
        .setSrcAccessMask(src_access_mask)
        .setDstStageMask(dst_stage_mask)
        .setDstAccessMask(dst_acces_mask)
        .setOldLayout(old_layout)
        .setNewLayout(new_layout)
        .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
        .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
        .setImage(m_swapChainImages[imageIndex])
        .setSubresourceRange(vk::ImageSubresourceRange{}
                                 .setAspectMask(vk::ImageAspectFlagBits::eColor)
                                 .setBaseMipLevel(0)
                                 .setLevelCount(1)
                                 .setBaseArrayLayer(0)
                                 .setLayerCount(1));
    auto dependencyInfo = vk::DependencyInfo{};
    dependencyInfo.setDependencyFlags({}).setImageMemoryBarrierCount(1).setPImageMemoryBarriers(
        &barrier);
    m_commandBuffers[m_frameIndex].pipelineBarrier2(dependencyInfo);
}

void TriApp::recreateSwapChain() {
    m_device.waitIdle();
    cleanupSwapChain();
    createSwapChain();
    createImageViews();
}

} // namespace vke
