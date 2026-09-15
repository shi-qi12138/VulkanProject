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
    // 销毁函数同样属于扩展，需要运行时获取函数地址。
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

    _window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
    if (_window == nullptr) {
        throw std::runtime_error("GLFW窗口创建失败");
    }

    // 把应用对象交给GLFW，尺寸回调可通过窗口取回this指针。
    glfwSetWindowUserPointer(_window, this);
    glfwSetFramebufferSizeCallback(_window, framebufferResizeCallback);

    createInstance();
    setupDebugMessenger();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    createSwapChain();
    createImageViews();
    createRenderPass();
    createGraphicsPipeline();
    createFramebuffers();

    // 顶点数据上传需要临时命令缓冲区，因此必须先创建命令池。
    createCommandPool();

    createVertexBuffer();
    createIndexBuffer();

    createCommandBuffers();
    createSyncObjects();
}

void HelloTriangleApplication::mainLoop()
{
    // 窗口关闭前持续处理键盘、鼠标和窗口事件。
    while (!glfwWindowShouldClose(_window)) {
        glfwPollEvents();
        drawFrame();
    }

    vkDeviceWaitIdle(_device);
}

void HelloTriangleApplication::cleanup()
{
    // 销毁每帧和每张交换链图像使用的同步对象。
    for (VkSemaphore semaphore : _renderFinishedSemaphores) {
        vkDestroySemaphore(_device, semaphore, nullptr);
    }

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(_device, _imageAvailableSemaphores[i], nullptr);
        vkDestroyFence(_device, _inFlightFences[i], nullptr);
    }

    // 销毁命令池，释放命令缓冲区和相关资源。
    if (_commandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(_device, _commandPool, nullptr);
        _commandPool = VK_NULL_HANDLE;
    }

    cleanupSwapChain();

    // 销毁顶点缓冲区和索引缓冲区，释放GPU内存。
    if (_vertexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(_device, _vertexBuffer, nullptr);
        _vertexBuffer = VK_NULL_HANDLE;
    }

    if (_vertexBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(_device, _vertexBufferMemory, nullptr);
        _vertexBufferMemory = VK_NULL_HANDLE;
    }

    if (_indexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(_device, _indexBuffer, nullptr);
        _indexBuffer = VK_NULL_HANDLE;
    }

    if (_indexBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(_device, _indexBufferMemory, nullptr);
        _indexBufferMemory = VK_NULL_HANDLE;
    }

    // 销毁图形管线对象，释放GPU资源。
    if (_graphicsPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(_device, _graphicsPipeline, nullptr);
        _graphicsPipeline = VK_NULL_HANDLE;
    }

    // Pipeline Layout依赖Device，必须在销毁Device之前释放。
    if (_pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(_device, _pipelineLayout, nullptr);
        _pipelineLayout = VK_NULL_HANDLE;
    }

    // Render Pass依赖Device，需要在Device之前销毁。
    if (_renderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(_device, _renderPass, nullptr);
        _renderPass = VK_NULL_HANDLE;
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
    // 当前最低要求：同时拥有图形、呈现队列，并支持必需扩展。
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

VkShaderModule
HelloTriangleApplication::createShaderModule(const std::vector<char> &code)
{
    // 创建着色器模块时需要提供SPIR-V字节码的大小和指针。
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t *>(code.data());

    VkShaderModule shaderModule = VK_NULL_HANDLE;
    if (vkCreateShaderModule(_device, &createInfo, nullptr, &shaderModule)
        != VK_SUCCESS) {
        throw std::runtime_error("着色器模块创建失败");
    }
    return shaderModule;
}

void HelloTriangleApplication::recordCommandBuffer(
    VkCommandBuffer commandBuffer, uint32_t imageIndex)
{
    // 开始记录本帧发送给GPU的命令。
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = 0;                  // Optional
    beginInfo.pInheritanceInfo = nullptr; // Optional

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("命令缓冲区开始记录失败");
    }

    // 选择当前交换链图像对应的Framebuffer并设置清屏范围。
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = _renderPass;
    renderPassInfo.framebuffer = _swapChainFramebuffers[imageIndex];

    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = _swapChainExtent;

    // 渲染开始时把颜色附件清除为不透明黑色。
    VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo,
                         VK_SUBPASS_CONTENTS_INLINE);

    // 绑定图形管线，后续绘制命令使用其中的渲染状态。
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      _graphicsPipeline);

    // Viewport和Scissor是动态状态，需要在每次记录时设置。
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(_swapChainExtent.width);
    viewport.height = static_cast<float>(_swapChainExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = _swapChainExtent;
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    // 绑定顶点缓冲区，告诉GPU从哪里读取顶点数据。
    VkBuffer vertexBuffers[] = {_vertexBuffer};
    VkDeviceSize offsets[] = {0};

    // 绑定顶点缓冲区时，指定缓冲区和偏移量，GPU将从该缓冲区读取顶点数据。
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

    // 绑定索引缓冲区，告诉GPU从哪里读取顶点索引数据。
    vkCmdBindIndexBuffer(commandBuffer, _indexBuffer, 0, VK_INDEX_TYPE_UINT16);

    // 使用顶点绘制三角形，指定顶点数量、实例数量、起始顶点和实例偏移。
    // vkCmdDraw(commandBuffer, static_cast<uint32_t>(_vertices.size()), 1, 0,
    // 0);

    // 使用索引绘制三角形，指定索引数量、实例数量、起始索引和顶点偏移。
    vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(_indices.size()), 1,
                     0, 0, 0);

    vkCmdEndRenderPass(commandBuffer);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("命令缓冲区记录结束失败");
    }
}

void HelloTriangleApplication::cleanupSwapChain()
{
    // 销毁交换链帧缓冲区，释放相关资源。
    for (auto framebuffer : _swapChainFramebuffers) {
        vkDestroyFramebuffer(_device, framebuffer, nullptr);
    }
    _swapChainFramebuffers.clear();

    // 销毁交换链图像视图，释放图像资源。
    for (auto imageView : _swapChainImageViews) {
        vkDestroyImageView(_device, imageView, nullptr);
    }
    _swapChainImageViews.clear();
    _swapChainImages.clear();

    if (_swapChain != VK_NULL_HANDLE) {
        // Swapchain拥有其中的VkImage，因此无需单独销毁图像。
        vkDestroySwapchainKHR(_device, _swapChain, nullptr);
        _swapChain = VK_NULL_HANDLE;
    }
}

void HelloTriangleApplication::recreateSwapChain()
{
    // 窗口最小化时尺寸可能为0，等待窗口恢复后再创建交换链。
    int width = 0, height = 0;
    glfwGetFramebufferSize(_window, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(_window, &width, &height);
        glfwWaitEvents();
    }

    // 确保GPU不再使用旧交换链资源。
    vkDeviceWaitIdle(_device);

    cleanupSwapChain();

    // 渲染完成信号量与交换链图像一一对应，需要同步重建。
    for (VkSemaphore semaphore : _renderFinishedSemaphores) {
        vkDestroySemaphore(_device, semaphore, nullptr);
    }
    _renderFinishedSemaphores.clear();

    createSwapChain();
    createImageViews();
    createFramebuffers();

    _renderFinishedSemaphores.resize(_swapChainImages.size());
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    for (VkSemaphore &semaphore : _renderFinishedSemaphores) {
        if (vkCreateSemaphore(_device, &semaphoreInfo, nullptr, &semaphore)
            != VK_SUCCESS) {
            throw std::runtime_error("重建渲染完成信号量失败");
        }
    }
}

uint32_t
HelloTriangleApplication::findMemoryType(uint32_t typeFilter,
                                         VkMemoryPropertyFlags properties)
{
    // 查询物理设备的内存类型和堆信息。
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(_physicalDevice, &memProperties);
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i))
            && (memProperties.memoryTypes[i].propertyFlags & properties)
                   == properties) {
            return i;
        }
    }
    throw std::runtime_error("未找到合适的内存类型");
}

VKAPI_ATTR VkBool32 VKAPI_CALL HelloTriangleApplication::debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT, VkDebugUtilsMessageTypeFlagsEXT,
    const VkDebugUtilsMessengerCallbackDataEXT *callbackData, void *)
{
    std::cerr << "Vulkan验证层：" << callbackData->pMessage << '\n';
    return VK_FALSE;
}

std::vector<char>
HelloTriangleApplication::readShaderFile(const std::string &filename)
{
    // ate从文件尾开始，便于直接取得文件大小；binary避免文本转换。
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("着色器文件打开失败：" + filename);
    }

    // 根据文件大小分配缓冲区，再回到开头读取全部字节。
    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    return buffer;
}

void HelloTriangleApplication::framebufferResizeCallback(GLFWwindow *window,
                                                         int width, int height)
{
    // 从GLFW取回应用对象，只设置标记，不在回调中直接重建资源。
    auto app = reinterpret_cast<HelloTriangleApplication *>(
        glfwGetWindowUserPointer(window));
    app->_framebufferResized = true;
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
    std::cout << "当前使用的物理设备：" << deviceProperties.deviceName << '\n';
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

void HelloTriangleApplication::createGraphicsPipeline()
{
    // 读取编译后的SPIR-V，并创建临时Shader Module。
    auto vertShaderCode = readShaderFile("vertIn.spv");
    auto fragShaderCode = readShaderFile("frag.spv");

    VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
    VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

    // 配置顶点着色器阶段，入口函数名为main。
    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType =
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";

    // 配置片段着色器阶段，负责输出像素颜色。
    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType =
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo,
                                                      fragShaderStageInfo};

    // 当前顶点数据直接写在Shader中，因此不提供顶点输入。
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType =
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    // 获取顶点绑定描述符和属性描述符，用于告诉GPU如何解释顶点数据。
    auto bindingDescription = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();

    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.vertexAttributeDescriptionCount =
        static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    // 每三个顶点组成一个独立三角形。
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType =
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // Viewport和Scissor数量固定为1，具体值在绘制时动态设置。
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    // 光栅化阶段填充三角形，并剔除背面。
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType =
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    // 当前关闭多重采样，每个像素只使用一个样本。
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType =
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // 允许写入RGBA四个通道，暂时关闭颜色混合。
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
        | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType =
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.0f;
    colorBlending.blendConstants[1] = 0.0f;
    colorBlending.blendConstants[2] = 0.0f;
    colorBlending.blendConstants[3] = 0.0f;

    // Viewport和Scissor设为动态状态，无需重建管线即可修改。
    std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT,
                                                 VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount =
        static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    // 当前没有Descriptor Set和Push Constant，先创建空管线布局。
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 0;
    pipelineLayoutInfo.pushConstantRangeCount = 0;

    if (vkCreatePipelineLayout(_device, &pipelineLayoutInfo, nullptr,
                               &_pipelineLayout)
        != VK_SUCCESS) {
        throw std::runtime_error("图形管线布局创建失败");
    }

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = _pipelineLayout;
    pipelineInfo.renderPass = _renderPass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

    if (vkCreateGraphicsPipelines(_device, VK_NULL_HANDLE, 1, &pipelineInfo,
                                  nullptr, &_graphicsPipeline)
        != VK_SUCCESS) {
        throw std::runtime_error("Vulkan图形管线创建失败");
    }

    // 管线布局创建后不再需要Shader Module，可以立即释放。
    vkDestroyShaderModule(_device, fragShaderModule, nullptr);
    vkDestroyShaderModule(_device, vertShaderModule, nullptr);
}

void HelloTriangleApplication::createRenderPass()
{
    // 颜色附件对应交换链图像：开始时清屏，结束后保留内容用于呈现。
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = _swapChainImageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    // 子通道执行期间，图像使用最适合颜色写入的布局。
    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // 当前Render Pass只有一个图形子通道和一个颜色附件。
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    // 等待颜色附件可用后，才允许子通道写入颜色。
    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;

    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;

    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(_device, &renderPassInfo, nullptr, &_renderPass)
        != VK_SUCCESS) {
        throw std::runtime_error("Vulkan渲染通道创建失败");
    }
}

void HelloTriangleApplication::createFramebuffers()
{
    // 每个交换链Image View都需要一个对应的Framebuffer。
    _swapChainFramebuffers.resize(_swapChainImageViews.size());

    for (size_t i = 0; i < _swapChainImageViews.size(); i++) {
        VkImageView attachments[] = {_swapChainImageViews[i]};

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = _renderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = _swapChainExtent.width;
        framebufferInfo.height = _swapChainExtent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(_device, &framebufferInfo, nullptr,
                                &_swapChainFramebuffers[i])
            != VK_SUCCESS) {
            throw std::runtime_error("Vulkan帧缓冲区创建失败");
        }
    }
}

void HelloTriangleApplication::createCommandPool()
{
    // 命令池必须属于实际执行绘制命令的图形队列族。
    QueueFamilyIndices queueFamilies = findQueueFamilies(_physicalDevice);

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = queueFamilies.graphicsFamily.value();

    if (vkCreateCommandPool(_device, &poolInfo, nullptr, &_commandPool)
        != VK_SUCCESS) {
        throw std::runtime_error("Vulkan命令池创建失败");
    }
}

void HelloTriangleApplication::createVertexBuffer()
{
    VkDeviceSize bufferSize = sizeof(_vertices[0]) * _vertices.size();

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                     | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingBuffer, stagingBufferMemory);

    void *data;
    vkMapMemory(_device, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, _vertices.data(), (size_t)bufferSize);
    vkUnmapMemory(_device, stagingBufferMemory);

    createBuffer(bufferSize,
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT
                     | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, _vertexBuffer,
                 _vertexBufferMemory);

    copyBuffer(stagingBuffer, _vertexBuffer, bufferSize);

    vkDestroyBuffer(_device, stagingBuffer, nullptr);
    vkFreeMemory(_device, stagingBufferMemory, nullptr);
}

void HelloTriangleApplication::createIndexBuffer()
{
    VkDeviceSize bufferSize = sizeof(_indices[0]) * _indices.size();

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                     | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingBuffer, stagingBufferMemory);

    void *data;
    vkMapMemory(_device, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, _indices.data(), (size_t)bufferSize);
    vkUnmapMemory(_device, stagingBufferMemory);

    createBuffer(
        bufferSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, _indexBuffer, _indexBufferMemory);

    copyBuffer(stagingBuffer, _indexBuffer, bufferSize);

    vkDestroyBuffer(_device, stagingBuffer, nullptr);
    vkFreeMemory(_device, stagingBufferMemory, nullptr);
}

void HelloTriangleApplication::createBuffer(VkDeviceSize size,
                                            VkBufferUsageFlags usage,
                                            VkMemoryPropertyFlags properties,
                                            VkBuffer &buffer,
                                            VkDeviceMemory &bufferMemory)
{
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(_device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to create buffer!");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(_device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex =
        findMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(_device, &allocInfo, nullptr, &bufferMemory)
        != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate buffer memory!");
    }

    if (vkBindBufferMemory(_device, buffer, bufferMemory, 0) != VK_SUCCESS) {
        throw std::runtime_error("缓冲区与设备内存绑定失败");
    }
}

void HelloTriangleApplication::copyBuffer(VkBuffer srcBuffer,
                                          VkBuffer dstBuffer, VkDeviceSize size)
{
    if (_commandPool == VK_NULL_HANDLE) {
        throw std::runtime_error("复制缓冲区前必须先创建命令池");
    }

    // 从命令池临时分配一个只执行一次的命令缓冲区。
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = _commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    if (vkAllocateCommandBuffers(_device, &allocInfo, &commandBuffer)
        != VK_SUCCESS) {
        throw std::runtime_error("临时复制命令缓冲区分配失败");
    }

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        vkFreeCommandBuffers(_device, _commandPool, 1, &commandBuffer);
        throw std::runtime_error("临时复制命令缓冲区开始记录失败");
    }

    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        vkFreeCommandBuffers(_device, _commandPool, 1, &commandBuffer);
        throw std::runtime_error("临时复制命令缓冲区记录失败");
    }

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    if (vkQueueSubmit(_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE)
        != VK_SUCCESS) {
        vkFreeCommandBuffers(_device, _commandPool, 1, &commandBuffer);
        throw std::runtime_error("缓冲区复制命令提交失败");
    }

    // 等待复制完成后，释放临时命令缓冲区。
    if (vkQueueWaitIdle(_graphicsQueue) != VK_SUCCESS) {
        throw std::runtime_error("等待缓冲区复制完成失败");
    }

    vkFreeCommandBuffers(_device, _commandPool, 1, &commandBuffer);
}

void HelloTriangleApplication::createCommandBuffers()
{
    // 每个并行帧分配一个可重复记录的主命令缓冲区。
    _commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = _commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = (uint32_t)_commandBuffers.size();

    if (vkAllocateCommandBuffers(_device, &allocInfo, _commandBuffers.data())
        != VK_SUCCESS) {
        throw std::runtime_error("Vulkan命令缓冲区分配失败");
    }
}

void HelloTriangleApplication::createSyncObjects()
{
    // 获取图像和Fence按并行帧管理；渲染完成信号量按交换链图像管理。
    _imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    _renderFinishedSemaphores.resize(_swapChainImages.size());
    _inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    // Fence初始为已触发，避免第一帧永久等待。
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(_device, &semaphoreInfo, nullptr,
                              &_imageAvailableSemaphores[i])
                != VK_SUCCESS
            || vkCreateFence(_device, &fenceInfo, nullptr, &_inFlightFences[i])
                   != VK_SUCCESS) {

            throw std::runtime_error("帧同步对象创建失败");
        }
    }

    for (VkSemaphore &semaphore : _renderFinishedSemaphores) {
        if (vkCreateSemaphore(_device, &semaphoreInfo, nullptr, &semaphore)
            != VK_SUCCESS) {
            throw std::runtime_error("渲染完成信号量创建失败");
        }
    }
}

void HelloTriangleApplication::drawFrame()
{
    // 等待当前帧上一次提交完成，确保可以安全复用资源。
    vkWaitForFences(_device, 1, &_inFlightFences[_currentFrame], VK_TRUE,
                    UINT64_MAX);

    uint32_t imageIndex;
    // 获取下一张可渲染的交换链图像。
    VkResult result = vkAcquireNextImageKHR(
        _device, _swapChain, UINT64_MAX,
        _imageAvailableSemaphores[_currentFrame], VK_NULL_HANDLE, &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapChain();
        return;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("获取交换链图像失败");
    }

    // 准备重新提交当前帧，并重新记录它的命令缓冲区。
    vkResetFences(_device, 1, &_inFlightFences[_currentFrame]);

    vkResetCommandBuffer(_commandBuffers[_currentFrame],
                         /*VkCommandBufferResetFlagBits*/ 0);
    recordCommandBuffer(_commandBuffers[_currentFrame], imageIndex);

    // 等待图像可用后执行绘制，完成后触发渲染完成信号量。
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = {_imageAvailableSemaphores[_currentFrame]};
    VkPipelineStageFlags waitStages[] = {
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &_commandBuffers[_currentFrame];

    VkSemaphore signalSemaphores[] = {_renderFinishedSemaphores[imageIndex]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(_graphicsQueue, 1, &submitInfo,
                      _inFlightFences[_currentFrame])
        != VK_SUCCESS) {
        throw std::runtime_error("绘制命令提交失败");
    }

    // 等待绘制结束，再把当前交换链图像提交给窗口系统显示。
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = {_swapChain};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;

    presentInfo.pImageIndices = &imageIndex;

    result = vkQueuePresentKHR(_presentQueue, &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR
        || _framebufferResized) {
        _framebufferResized = false;
        recreateSwapChain();
    } else if (result != VK_SUCCESS) {
        throw std::runtime_error("交换链图像呈现失败");
    }

    // 循环使用有限数量的帧资源。
    _currentFrame = (_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
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
