#include "HelloTriangleApplication.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>

namespace
{
// Debug Utils属于扩展，需要通过vkGetInstanceProcAddr取得函数地址。
VkResult
createDebugUtilsMessenger(VkInstance instance,
                          const VkDebugUtilsMessengerCreateInfoEXT *createInfo,
                          VkDebugUtilsMessengerEXT *debugMessenger)
{
    const auto function = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));

    return function != nullptr
               ? function(instance, createInfo, nullptr, debugMessenger)
               : VK_ERROR_EXTENSION_NOT_PRESENT;
}

void destroyDebugUtilsMessenger(VkInstance instance,
                                VkDebugUtilsMessengerEXT debugMessenger)
{
    const auto function = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));

    if (function != nullptr) {
        function(instance, debugMessenger, nullptr);
    }
}
} // namespace

bool QueueFamilyIndices::isComplete() const
{
    return graphicsFamily.has_value() && presentFamily.has_value();
}

void HelloTriangleApplication::run()
{
    // 按顺序执行完整的应用生命周期。
    initVulkan();
    mainLoop();
    cleanup();
}

void HelloTriangleApplication::initVulkan()
{
    // GLFW负责创建窗口，并提供当前平台所需的Vulkan扩展。
    if (glfwInit() != GLFW_TRUE) {
        throw std::runtime_error("GLFW初始化失败");
    }

    if (glfwVulkanSupported() != GLFW_TRUE) {
        throw std::runtime_error("当前系统未找到可用的Vulkan环境");
    }

    // Vulkan负责渲染，不需要创建OpenGL上下文。
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    _window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
    if (_window == nullptr) {
        throw std::runtime_error("GLFW窗口创建失败");
    }

    createInstance();
    setupDebugMessenger();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    createSwapChain();
    createImageViews();
}

void HelloTriangleApplication::mainLoop()
{
    // 窗口关闭前持续处理键盘、鼠标和窗口事件。
    while (!glfwWindowShouldClose(_window)) {
        glfwPollEvents();
    }
}

void HelloTriangleApplication::cleanup()
{
    // 销毁交换链图像视图，释放图像资源。
    for (auto imageView : _swapChainImageViews) {
        vkDestroyImageView(_device, imageView, nullptr);
    }

    // 销毁交换链对象，释放交换链图像和相关资源。
    if (_device != VK_NULL_HANDLE && _swapChain != VK_NULL_HANDLE) {

        // 销毁对象：释放Vulkan内部资源
        vkDestroySwapchainKHR(_device, _swapChain, nullptr);

        // 句柄置空：修改程序自己的变量
        _swapChain = VK_NULL_HANDLE;
    }

    // Vulkan资源按照创建顺序的反方向销毁。
    if (_device != VK_NULL_HANDLE) {
        vkDestroyDevice(_device, nullptr);
        _device = VK_NULL_HANDLE;
    }

    // 销毁调试回调对象，释放验证层资源。
    if (_debugMessenger != VK_NULL_HANDLE) {
        destroyDebugUtilsMessenger(_instance, _debugMessenger);
        _debugMessenger = VK_NULL_HANDLE;
    }

    // 销毁Surface对象，释放窗口相关资源。
    if (_surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(_instance, _surface, nullptr);
        _surface = VK_NULL_HANDLE;
    }

    // 销毁Vulkan实例，释放所有Vulkan对象和资源。
    if (_instance != VK_NULL_HANDLE) {
        vkDestroyInstance(_instance, nullptr);
        _instance = VK_NULL_HANDLE;
    }

    // 销毁GLFW窗口，释放窗口相关资源。
    if (_window != nullptr) {
        glfwDestroyWindow(_window);
        _window = nullptr;
    }

    glfwTerminate();
}

void HelloTriangleApplication::createInstance()
{
    // 开启验证层时，必须先确认系统已经安装对应层。
    if (_enableValidationLayers && !checkValidationLayerSupport()) {
        throw std::runtime_error("请求的Vulkan验证层不可用");
    }
    if (!checkGlfwRequiredExtensionsSupport()) {
        throw std::runtime_error("系统不支持GLFW所需的Vulkan实例扩展");
    }

    // 描述应用和使用的Vulkan API版本。
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Hello Triangle";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    const auto extensions = getRequiredExtensions();

    // 设置实例需要启用的扩展和验证层。
    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    if (_enableValidationLayers) {
        createInfo.enabledLayerCount =
            static_cast<uint32_t>(_validationLayers.size());
        createInfo.ppEnabledLayerNames = _validationLayers.data();
        populateDebugMessengerCreateInfo(debugCreateInfo);
        createInfo.pNext = &debugCreateInfo;
    }

    if (vkCreateInstance(&createInfo, nullptr, &_instance) != VK_SUCCESS) {
        throw std::runtime_error("Vulkan实例创建失败");
    }
}

bool HelloTriangleApplication::checkGlfwRequiredExtensionsSupport() const
{
    // Vulkan常用两次查询：第一次获取数量，第二次获取实际数据。
    uint32_t availableCount = 0;
    if (vkEnumerateInstanceExtensionProperties(nullptr, &availableCount,
                                               nullptr)
        != VK_SUCCESS) {
        return false;
    }

    std::vector<VkExtensionProperties> availableExtensions(availableCount);
    if (vkEnumerateInstanceExtensionProperties(nullptr, &availableCount,
                                               availableExtensions.data())
        != VK_SUCCESS) {
        return false;
    }

    std::unordered_set<std::string_view> availableNames;
    for (const auto &extension : availableExtensions) {
        availableNames.emplace(extension.extensionName);
    }

    uint32_t requiredCount = 0;
    const char **requiredExtensions =
        glfwGetRequiredInstanceExtensions(&requiredCount);
    if (requiredExtensions == nullptr) {
        return false;
    }

    for (uint32_t index = 0; index < requiredCount; ++index) {
        if (!availableNames.contains(requiredExtensions[index])) {
            std::cerr << "缺少Vulkan实例扩展：" << requiredExtensions[index]
                      << '\n';
            return false;
        }
    }
    return true;
}

bool HelloTriangleApplication::checkValidationLayerSupport() const
{
    // 枚举系统支持的验证层，并逐个检查所需层是否存在。
    uint32_t layerCount = 0;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (const char *requiredLayer : _validationLayers) {
        bool found = false;
        for (const auto &availableLayer : availableLayers) {
            if (std::strcmp(requiredLayer, availableLayer.layerName) == 0) {
                found = true;
                break;
            }
        }
        if (!found) {
            return false;
        }
    }
    return true;
}

bool HelloTriangleApplication::isDeviceSuitable(VkPhysicalDevice device)
{
    QueueFamilyIndices indices = findQueueFamilies(device);

    bool extensionsSupported = checkDeviceExtensionSupport(device);

    bool swapChainAdequate = false;
    if (extensionsSupported) {
        SwapChainSupportDetails swapChainSupport =
            querySwapChainSupport(device);
        swapChainAdequate = !swapChainSupport.formats.empty()
                            && !swapChainSupport.presentModes.empty();
    }

    return indices.isComplete() && extensionsSupported && swapChainAdequate;
}

std::vector<const char *>
HelloTriangleApplication::getRequiredExtensions() const
{
    // GLFW返回创建当前平台Surface所需的实例扩展。
    uint32_t count = 0;
    const char **glfwExtensions = glfwGetRequiredInstanceExtensions(&count);
    std::vector<const char *> extensions(glfwExtensions,
                                         glfwExtensions + count);

    if (_enableValidationLayers) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    return extensions;
}

VkSurfaceFormatKHR HelloTriangleApplication::chooseSwapSurfaceFormat(
    const std::vector<VkSurfaceFormatKHR> &availableFormats)
{
    // 优先选择SRGB颜色空间和B8G8R8A8格式。
    for (const auto &availableFormat : availableFormats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB
            && availableFormat.colorSpace
                   == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormat;
        }
    }

    // 如果没有理想的格式，则返回第一个可用格式。
    return availableFormats.front();
}

VkPresentModeKHR HelloTriangleApplication::chooseSwapPresentMode(
    const std::vector<VkPresentModeKHR> &availablePresentModes)
{
    // 优先选择Mailbox模式，它允许在屏幕刷新前多次更新图像，减少撕裂。
    for (const auto &availablePresentMode : availablePresentModes) {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return availablePresentMode;
        }
    }

    // 如果没有Mailbox模式，则选择FIFO模式，它是Vulkan规范要求必须支持的模式。
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D HelloTriangleApplication::chooseSwapExtent(
    const VkSurfaceCapabilitiesKHR &capabilities)
{
    // 如果Surface的当前尺寸不是最大值，则直接使用它。
    if (capabilities.currentExtent.width
        != std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    } else {
        // 否则使用窗口的实际像素尺寸，并限制在Surface支持的范围内。
        int width, height;
        glfwGetFramebufferSize(_window, &width, &height);

        VkExtent2D actualExtent = {static_cast<uint32_t>(width),
                                   static_cast<uint32_t>(height)};

        actualExtent.width =
            std::clamp(actualExtent.width, capabilities.minImageExtent.width,
                       capabilities.maxImageExtent.width);
        actualExtent.height =
            std::clamp(actualExtent.height, capabilities.minImageExtent.height,
                       capabilities.maxImageExtent.height);

        return actualExtent;
    }
}

VKAPI_ATTR VkBool32 VKAPI_CALL HelloTriangleApplication::debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT, VkDebugUtilsMessageTypeFlagsEXT,
    const VkDebugUtilsMessengerCallbackDataEXT *callbackData, void *)
{
    std::cerr << "Vulkan验证层：" << callbackData->pMessage << '\n';
    return VK_FALSE;
}

void HelloTriangleApplication::populateDebugMessengerCreateInfo(
    VkDebugUtilsMessengerCreateInfoEXT &createInfo)
{
    createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
        | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
        | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
                             | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                             | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;
}

void HelloTriangleApplication::setupDebugMessenger()
{
    // Release模式关闭验证层，因此不创建调试回调。
    if (!_enableValidationLayers) {
        return;
    }

    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    populateDebugMessengerCreateInfo(createInfo);
    if (createDebugUtilsMessenger(_instance, &createInfo, &_debugMessenger)
        != VK_SUCCESS) {
        throw std::runtime_error("Vulkan调试回调创建失败");
    }
}

void HelloTriangleApplication::createSurface()
{
    // GLFW隐藏了Win32、X11等平台相关的Surface创建细节。
    if (glfwCreateWindowSurface(_instance, _window, nullptr, &_surface)
        != VK_SUCCESS) {
        throw std::runtime_error("Vulkan窗口Surface创建失败");
    }
}

void HelloTriangleApplication::pickPhysicalDevice()
{
    // 枚举所有GPU，并通过评分选择最适合的一块。
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(_instance, &deviceCount, nullptr);
    if (deviceCount == 0) {
        throw std::runtime_error("没有找到支持Vulkan的GPU");
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(_instance, &deviceCount, devices.data());

    std::multimap<int, VkPhysicalDevice> candidates;
    for (VkPhysicalDevice device : devices) {
        if (!isDeviceSuitable(device)) {
            continue;
        }

        candidates.emplace(rateDeviceSuitability(device), device);
    }

    if (candidates.empty() || candidates.rbegin()->first <= 0) {
        throw std::runtime_error("没有找到满足队列和交换链要求的GPU");
    }
    _physicalDevice = candidates.rbegin()->second;

    // 输出最终选中的物理设备名称，便于确认程序正在使用哪块GPU。
    VkPhysicalDeviceProperties deviceProperties{};
    vkGetPhysicalDeviceProperties(_physicalDevice, &deviceProperties);
    std::cout << "当前使用的物理设备：" << deviceProperties.deviceName
              << '\n';
}

int HelloTriangleApplication::rateDeviceSuitability(
    VkPhysicalDevice device) const
{
    // 缺少队列族、交换链扩展或Surface支持时，设备直接判为不可用。
    const QueueFamilyIndices indices = findQueueFamilies(device);
    if (!indices.isComplete() || !checkDeviceExtensionSupport(device)) {
        return 0;
    }

    const SwapChainSupportDetails swapChainSupport =
        querySwapChainSupport(device);
    if (swapChainSupport.formats.empty()
        || swapChainSupport.presentModes.empty()) {
        return 0;
    }

    VkPhysicalDeviceProperties properties{};
    VkPhysicalDeviceFeatures features{};
    vkGetPhysicalDeviceProperties(device, &properties);
    vkGetPhysicalDeviceFeatures(device, &features);

    if (!features.geometryShader) {
        return 0;
    }

    // 独立显卡获得额外分数，再结合最大纹理尺寸进行排序。
    int score = static_cast<int>(properties.limits.maxImageDimension2D);
    if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        score += 1000;
    }
    return score;
}

QueueFamilyIndices
HelloTriangleApplication::findQueueFamilies(VkPhysicalDevice device) const
{
    // 图形队列负责绘制，呈现队列负责把图像显示到Surface。
    QueueFamilyIndices indices;
    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
    std::vector<VkQueueFamilyProperties> families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, families.data());

    for (uint32_t index = 0; index < count; ++index) {
        if ((families[index].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0U) {
            indices.graphicsFamily = index;
        }

        VkBool32 presentSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, index, _surface,
                                             &presentSupport);
        if (presentSupport == VK_TRUE) {
            indices.presentFamily = index;
        }

        if (indices.isComplete()) {
            break;
        }
    }
    return indices;
}

void HelloTriangleApplication::createLogicalDevice()
{
    // 图形和呈现可能使用同一个队列族，set可避免重复创建。
    const QueueFamilyIndices indices = findQueueFamilies(_physicalDevice);
    const std::set<uint32_t> uniqueFamilies = {indices.graphicsFamily.value(),
                                               indices.presentFamily.value()};

    // 每个队列族都需要指定优先级
    float priority = 1.0F;
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;

    // 为每个队列族创建一个VkDeviceQueueCreateInfo结构体，指定队列族索引、队列数量和优先级。
    for (uint32_t family : uniqueFamilies) {
        VkDeviceQueueCreateInfo queueInfo{};
        queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfo.queueFamilyIndex = family;
        queueInfo.queueCount = 1;
        queueInfo.pQueuePriorities = &priority;
        queueCreateInfos.push_back(queueInfo);
    }

    VkPhysicalDeviceFeatures features{};

    // 创建逻辑设备时指定启用的队列族、设备特性和扩展。
    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount =
        static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.pEnabledFeatures = &features;

    // 使用交换链需要先启用 VK_KHR_swapchain 扩展
    createInfo.enabledExtensionCount =
        static_cast<uint32_t>(_deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = _deviceExtensions.data();

    if (vkCreateDevice(_physicalDevice, &createInfo, nullptr, &_device)
        != VK_SUCCESS) {
        throw std::runtime_error("Vulkan逻辑设备创建失败");
    }

    // 队列已随Device创建，这里仅取得它们的句柄。
    vkGetDeviceQueue(_device, indices.graphicsFamily.value(), 0,
                     &_graphicsQueue);
    vkGetDeviceQueue(_device, indices.presentFamily.value(), 0, &_presentQueue);
}

void HelloTriangleApplication::createSwapChain()
{
    // 查询物理设备对当前Surface的交换链支持情况。
    SwapChainSupportDetails swapChainSupport =
        querySwapChainSupport(_physicalDevice);

    // 选择最合适的Surface格式、呈现模式和图像尺寸。
    VkSurfaceFormatKHR surfaceFormat =
        chooseSwapSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode =
        chooseSwapPresentMode(swapChainSupport.presentModes);
    VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

    // 使用Surface要求的最小图像数量，并确保不超过允许的最大值。
    uint32_t imageCount = swapChainSupport.capabilities.minImageCount;
    if (swapChainSupport.capabilities.maxImageCount > 0
        && imageCount > swapChainSupport.capabilities.maxImageCount) {

        // 如果超过最大值，则使用最大值。
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    // 配置交换链创建参数，包括Surface、图像数量、格式、尺寸、使用方式等。
    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = _surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    // 交换链图像将作为颜色附件，用来接收渲染结果。
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    // 交换链图像的使用方式取决于图形队列和呈现队列是否属于同一个队列族。
    QueueFamilyIndices indices = findQueueFamilies(_physicalDevice);
    uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(),
                                     indices.presentFamily.value()};

    // 如果图形队列和呈现队列属于不同的队列族，则需要设置共享模式。
    if (indices.graphicsFamily != indices.presentFamily) {
        // 不同队列族共享图像，省去手动转移图像所有权。
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        // 同一队列族使用独占模式，通常拥有更好的性能。
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        // EXCLUSIVE模式下，这两个字段会被忽略。
        createInfo.queueFamilyIndexCount = 0;     // Optional
        createInfo.pQueueFamilyIndices = nullptr; // Optional
    }

    // 保留Surface当前变换，并让窗口系统忽略图像Alpha通道。
    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    // 设置呈现模式；被遮挡的像素允许不参与最终合成。
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    // 创建交换链对象，并检查返回值是否成功。
    if (vkCreateSwapchainKHR(_device, &createInfo, nullptr, &_swapChain)
        != VK_SUCCESS) {
        throw std::runtime_error("Vulkan交换链创建失败");
    }

    // 获取交换链图像的数量，并分配存储空间。
    vkGetSwapchainImagesKHR(_device, _swapChain, &imageCount, nullptr);
    _swapChainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(_device, _swapChain, &imageCount,
                            _swapChainImages.data());

    // 保存交换链图像格式和尺寸，供后续创建图像视图和渲染目标使用。
    _swapChainImageFormat = surfaceFormat.format;
    _swapChainExtent = extent;
}

void HelloTriangleApplication::createImageViews()
{
    // 为交换链中的每个图像创建一个图像视图，便于渲染和呈现。
    _swapChainImageViews.resize(_swapChainImages.size());
    for (size_t i = 0; i < _swapChainImages.size(); ++i) {
        VkImageViewCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = _swapChainImages[i];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = _swapChainImageFormat;

        // 设置图像视图的颜色通道映射，通常使用默认的身份映射。
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

        // 交换链图像只使用一个颜色层级和一个数组层。
        createInfo.subresourceRange.aspectMask =
            VK_IMAGE_ASPECT_COLOR_BIT; // 颜色图像
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        // 创建图像视图对象，并检查返回值是否成功。
        if (vkCreateImageView(_device, &createInfo, nullptr,
                              &_swapChainImageViews[i])
            != VK_SUCCESS) {
            throw std::runtime_error("交换链图像视图创建失败");
        }
    }
}

bool HelloTriangleApplication::checkDeviceExtensionSupport(
    VkPhysicalDevice device) const
{
    // 从必需扩展集合中删除已支持项，集合为空表示全部满足。
    uint32_t count = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(count);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count,
                                         availableExtensions.data());

    std::set<std::string> required(_deviceExtensions.begin(),
                                   _deviceExtensions.end());
    for (const auto &extension : availableExtensions) {
        required.erase(extension.extensionName);
    }
    return required.empty();
}

SwapChainSupportDetails
HelloTriangleApplication::querySwapChainSupport(VkPhysicalDevice device) const
{
    // 查询Surface能力、可用图像格式和呈现模式。
    SwapChainSupportDetails details;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, _surface,
                                              &details.capabilities);

    // 查询支持的表面格式
    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, _surface, &formatCount,
                                         nullptr);
    if (formatCount > 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, _surface, &formatCount,
                                             details.formats.data());
    }

    // Surface支持的图像呈现模式。
    uint32_t presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, _surface,
                                              &presentModeCount, nullptr);
    if (presentModeCount > 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(
            device, _surface, &presentModeCount, details.presentModes.data());
    }

    return details;
}
