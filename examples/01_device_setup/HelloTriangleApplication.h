#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <cstdint>
#include <optional>
#include <vector>

struct QueueFamilyIndices
{
    // 负责执行绘制命令的队列族。
    std::optional<uint32_t> graphicsFamily;

    // 负责将图像呈现到窗口的队列族。
    std::optional<uint32_t> presentFamily;

    // 两种队列族都找到后，设备才满足当前需求。
    [[nodiscard]] bool isComplete() const;
};

// 物理设备对当前窗口Surface的交换链支持信息。
struct SwapChainSupportDetails
{
    // Surface支持的图像数量、尺寸和变换等能力。
    VkSurfaceCapabilitiesKHR capabilities{};

    // Surface支持的像素格式和颜色空间。
    std::vector<VkSurfaceFormatKHR> formats;

    // Surface支持的图像呈现模式。
    std::vector<VkPresentModeKHR> presentModes;
};

class HelloTriangleApplication
{
public:
    // 程序的统一入口：初始化、事件循环、资源清理。
    void run();

private:
    // 程序生命周期。
    // 初始化窗口和Vulkan基础对象。
    void initVulkan();

    // 处理窗口事件，保持程序运行。
    void mainLoop();

    // 按依赖关系的反向顺序释放资源。
    void cleanup();

    // Vulkan基础对象的创建过程。
    // 创建Vulkan实例并启用所需扩展和验证层。
    void createInstance();

    // 创建接收验证层消息的调试对象。
    void setupDebugMessenger();

    // 创建连接Vulkan与窗口的Surface。
    void createSurface();

    // 从系统GPU中选择最合适的物理设备。
    void pickPhysicalDevice();

    // 创建逻辑设备并取得图形、呈现队列。
    void createLogicalDevice();

    // 创建交换链，配置图像格式、尺寸和呈现模式。
    void createSwapChain();

    // 创建交换链图像的视图，用于渲染和呈现。
    void createImageViews();

    // 检查扩展、验证层和物理设备是否满足要求。
    // 检查系统是否支持GLFW所需的实例扩展。
    [[nodiscard]] bool checkGlfwRequiredExtensionsSupport() const;

    // 检查Khronos验证层是否可用。
    [[nodiscard]] bool checkValidationLayerSupport() const;

    [[nodiscard]] bool isDeviceSuitable(VkPhysicalDevice device);

    // 检查GPU是否支持交换链等设备扩展。
    [[nodiscard]] bool
    checkDeviceExtensionSupport(VkPhysicalDevice device) const;

    // 根据GPU类型和能力计算设备评分。
    [[nodiscard]] int rateDeviceSuitability(VkPhysicalDevice device) const;

    // 查找支持图形和窗口呈现的队列族。
    [[nodiscard]] QueueFamilyIndices
    findQueueFamilies(VkPhysicalDevice device) const;

    // 查询GPU对当前Surface的交换链支持情况。
    [[nodiscard]] SwapChainSupportDetails
    querySwapChainSupport(VkPhysicalDevice device) const;

    // 收集创建Vulkan实例所需的扩展名称。
    [[nodiscard]] std::vector<const char *> getRequiredExtensions() const;

    // 从Surface支持的像素格式中选择最合适的一种。
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(
        const std::vector<VkSurfaceFormatKHR> &availableFormats);

    // 从Surface支持的呈现模式中选择最合适的一种。
    VkPresentModeKHR chooseSwapPresentMode(
        const std::vector<VkPresentModeKHR> &availablePresentModes);

    // 从Surface支持的图像尺寸中选择最合适的一种。
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities);

    // 配置并接收验证层输出的调试信息。
    // 填充Debug Messenger的创建参数。
    static void populateDebugMessengerCreateInfo(
        VkDebugUtilsMessengerCreateInfoEXT &createInfo);

    // 验证层发现问题时调用的回调函数。
    static VKAPI_ATTR VkBool32 VKAPI_CALL
    debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                  VkDebugUtilsMessageTypeFlagsEXT messageType,
                  const VkDebugUtilsMessengerCallbackDataEXT *callbackData,
                  void *userData);

private:
    // 窗口初始宽度。
    static constexpr uint32_t WIDTH = 800;

    // 窗口初始高度。
    static constexpr uint32_t HEIGHT = 600;

    // 窗口和Vulkan对象均初始化为空句柄，便于安全清理。
    // GLFW窗口句柄。
    GLFWwindow *_window = nullptr;

    // Vulkan实例，是其他Vulkan对象的入口。
    VkInstance _instance = VK_NULL_HANDLE;

    // 接收验证层消息的调试对象。
    VkDebugUtilsMessengerEXT _debugMessenger = VK_NULL_HANDLE;

    // Vulkan与窗口系统之间的呈现表面。
    VkSurfaceKHR _surface = VK_NULL_HANDLE;

    // 当前选中的物理GPU。
    VkPhysicalDevice _physicalDevice = VK_NULL_HANDLE;

    // 应用程序使用的逻辑设备。
    VkDevice _device = VK_NULL_HANDLE;

    // 用来提交绘制命令的队列。
    VkQueue _graphicsQueue = VK_NULL_HANDLE;

    // 用来提交窗口呈现请求的队列。
    VkQueue _presentQueue = VK_NULL_HANDLE;

    // Vulkan交换链对象，管理图像缓冲区和呈现模式。
    VkSwapchainKHR _swapChain = VK_NULL_HANDLE;

    // 交换链中包含的图像缓冲区，用于渲染和呈现。
    std::vector<VkImage> _swapChainImages;

    // 交换链图像的像素格式和颜色空间。
    VkFormat _swapChainImageFormat = VK_FORMAT_UNDEFINED;

    // 交换链图像的宽度和高度，通常与窗口尺寸一致。
    VkExtent2D _swapChainExtent{};

    // 交换链图像视图的列表，用于将图像绑定到渲染目标。
    std::vector<VkImageView> _swapChainImageViews;

    // 开发阶段使用Khronos官方验证层检查错误用法。
    const std::vector<const char *> _validationLayers = {
        "VK_LAYER_KHRONOS_validation"};

    // 交换链扩展用于把渲染结果呈现到窗口。
    const std::vector<const char *> _deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME};

#ifdef NDEBUG
    // Release模式关闭验证层以减少运行开销。
    static constexpr bool _enableValidationLayers = false;
#else
    // Debug模式启用验证层以检查Vulkan错误用法。
    static constexpr bool _enableValidationLayers = true;
#endif
};
