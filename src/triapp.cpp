#include "triapp.hpp"
#include "GLFW/glfw3.h"
#include "vulkan/vulkan.hpp"
#include "vulkan/vulkan_core.h"
#include "vulkan/vulkan_enums.hpp"
#include "vulkan/vulkan_handles.hpp"
#include "vulkan/vulkan_raii.hpp"
#include "vulkan/vulkan_structs.hpp"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <print>
#include <stdexcept>
#include <string>
#include <vector>

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

void TriApp::initVulkan() {
    createInstance();
    setUpDebugMessenger();
    pickPhysicalDevice();
    createLogicalDevice();
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
    auto layerProperties = m_context.enumerateInstanceLayerProperties();
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
    auto glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    std::vector extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
    if (enableValidationLayers) {
        extensions.push_back(vk::EXTDebugUtilsExtensionName);
    }
    return extensions;
}

auto TriApp::isDeviceSuitable(vk::raii::PhysicalDevice const &physicalDevice) -> bool {
    bool supportsVk13 = physicalDevice.getProperties().apiVersion >= vk::ApiVersion13;
    auto queueFamilies = physicalDevice.getQueueFamilyProperties();
    bool supportsGraphics = std::ranges::any_of(queueFamilies, [](auto const &qfp) {
        return !!(qfp.queueFlags & vk::QueueFlagBits::eGraphics);
    });
    auto availableDeviceExtensions = physicalDevice.enumerateDeviceExtensionProperties();
    bool supportsAllRequiredExtensions = std::ranges::all_of(
        requiredDeviceExtension, [&availableDeviceExtensions](auto const &requiredDeviceExtension) {
            return std::ranges::any_of(
                availableDeviceExtensions,
                [requiredDeviceExtension](auto const &availableDeviceExtension) {
                    return strcmp(availableDeviceExtension.extensionName,
                                  requiredDeviceExtension) == 0;
                });
        });
    auto features =
        physicalDevice
            .template getFeatures2<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan13Features,
                                   vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();
    bool supportsReqFeat =
        features.template get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering &&
        features.template get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>()
            .extendedDynamicState;
    return supportsVk13 && supportsReqFeat && supportsGraphics && supportsAllRequiredExtensions;
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
    std::uint32_t queueIndex = 0;
    for (std::uint32_t qfpIndex = 0; qfpIndex < queueFP.size(); ++qfpIndex) {
        if ((queueFP[qfpIndex].queueFlags & vk::QueueFlagBits::eGraphics) &&
            m_physicalDevice.getSurfaceSupportKHR(qfpIndex, *m_surface)) {
            queueIndex = qfpIndex;
            break;
        }
    }
    vk::StructureChain<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan13Features,
                       vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>
        featureChain{
            vk::PhysicalDeviceFeatures2{},
            vk::PhysicalDeviceVulkan13Features{}.setDynamicRendering(vk::True),
            vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT{}.setExtendedDynamicState(vk::True)};
    auto const priority = 0.5f;
    auto deviceQueueCI = vk::DeviceQueueCreateInfo{};
    deviceQueueCI.setQueueFamilyIndex(queueIndex).setQueueCount(1).setPQueuePriorities(&priority);
    auto deviceCI = vk::DeviceCreateInfo{};
    deviceCI.setPNext(&featureChain.get<vk::PhysicalDeviceFeatures2>())
        .setQueueCreateInfoCount(1)
        .setPQueueCreateInfos(&deviceQueueCI)
        .setEnabledExtensionCount(static_cast<std::uint32_t>(requiredDeviceExtension.size()))
        .setPpEnabledExtensionNames(requiredDeviceExtension.data());
    m_device = vk::raii::Device(m_physicalDevice, deviceCI);
    m_graphicsQueue = vk::raii::Queue(m_device, queueIndex, 0);
}

void TriApp::createSurface() {
    auto _s = VkSurfaceKHR{};
    if (glfwCreateWindowSurface(*m_instance, m_window, nullptr, &_s)) {
        throw std::runtime_error("Failed to create window surface!");
    }
    m_surface = vk::raii::SurfaceKHR(m_instance, _s);
}

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
