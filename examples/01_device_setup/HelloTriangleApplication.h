#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "glm/glm.hpp"
#include <array>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
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

// 顶点数据结构，包含位置和颜色属性。
struct Vertex
{
    glm::vec2 pos;
    glm::vec3 color;

    // 返回顶点输入绑定描述符，告诉Vulkan如何解释顶点缓冲区数据。
    static VkVertexInputBindingDescription getBindingDescription()
    {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        return bindingDescription;
    }

    // 返回顶点输入属性描述符数组，告诉Vulkan每个顶点属性的位置、格式和偏移量。
    static std::array<VkVertexInputAttributeDescription, 2>
    getAttributeDescriptions()
    {
        std::array<VkVertexInputAttributeDescription, 2>
            attributeDescriptions{};

        // 位置属性描述符，绑定到顶点缓冲区的第0个绑定点，格式为2个32位浮点数。
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(Vertex, pos);

        // 颜色属性描述符，绑定到顶点缓冲区的第0个绑定点，格式为3个32位浮点数。
        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Vertex, color);

        return attributeDescriptions;
    }
};

struct UniformBufferObject
{
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
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

    // 创建描述符集布局，定义Uniform Buffer的绑定方式。
    void createDescriptorSetLayout();

    // 创建图形管线，包括着色器、固定功能阶段和渲染状态。
    void createGraphicsPipeline();

    // 创建渲染通道，定义渲染目标和子通道。
    void createRenderPass();

    // 创建交换链帧缓冲区，每个图像视图对应一个Framebuffer。
    void createFramebuffers();

    // 创建命令池，用于分配和管理命令缓冲区。
    void createCommandPool();

    // 创建顶点缓冲区，用于存储顶点数据。
    void createVertexBuffer();

    // 创建索引缓冲区，用于存储顶点索引数据。
    void createIndexBuffer();

    // 创建Uniform Buffer，用于存储变换矩阵等数据。
    void createUniformBuffers();

    // 创建描述符池，用于分配描述符集。
    void createDescriptorPool();

    // 创建描述符集，将Uniform Buffer绑定到管线。
    void createDescriptorSets();

    // 在GPU内存中为缓冲区分配合适的内存类型，并创建缓冲区对象。
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                      VkMemoryPropertyFlags properties, VkBuffer &buffer,
                      VkDeviceMemory &bufferMemory);

    // 使用命令缓冲区将数据从源缓冲区复制到目标缓冲区。
    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

    // 创建命令缓冲区，用于记录绘制命令和状态切换。
    void createCommandBuffers();

    // 创建信号量和栅栏，用于同步图像获取、渲染和呈现。
    void createSyncObjects();

    // 更新Uniform Buffer的数据，例如模型、视图和投影矩阵。
    void updateUniformBuffer(uint32_t currentImage);

    // 渲染一帧图像：获取交换链图像、提交绘制命令、呈现到窗口。
    void drawFrame();

private:
    // 检查扩展、验证层和物理设备是否满足要求。
    // 检查系统是否支持GLFW所需的实例扩展。
    [[nodiscard]] bool checkGlfwRequiredExtensionsSupport() const;

    // 检查Khronos验证层是否可用。
    [[nodiscard]] bool checkValidationLayerSupport() const;

    // 判断GPU是否具备所需队列族和设备扩展。
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

    // 创建着色器模块，用于图形管线的顶点和片段着色器。
    VkShaderModule createShaderModule(const std::vector<char> &code);

    // 记录命令缓冲区中的绘制命令。
    void recordCommandBuffer(VkCommandBuffer commandBuffer,
                             uint32_t imageIndex);

    // 清理交换链及其相关资源。
    void cleanupSwapChain();

    // 当窗口大小发生变化时，重新创建交换链和相关资源。
    void recreateSwapChain();

    // 在GPU内存中为缓冲区分配合适的内存类型。
    uint32_t findMemoryType(uint32_t typeFilter,
                            VkMemoryPropertyFlags properties);

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

    // 读取SPIR-V二进制文件到内存缓冲区。
    static std::vector<char> readShaderFile(const std::string &filename);

    // GLFW窗口尺寸变化时设置重建交换链标记。
    static void framebufferResizeCallback(GLFWwindow *window, int width,
                                          int height);

private:
    // 最大同时渲染帧数，通常为2或3。
    const int MAX_FRAMES_IN_FLIGHT = 2;

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

    // 渲染通道，定义渲染目标和子通道。
    VkRenderPass _renderPass = VK_NULL_HANDLE;

    // 描述符集布局，定义Uniform Buffer的绑定方式。
    VkDescriptorSetLayout _descriptorSetLayout = VK_NULL_HANDLE;

    // 描述符池，用于分配描述符集。
    VkDescriptorPool _descriptorPool = VK_NULL_HANDLE;

    // 描述符集，将Uniform Buffer绑定到管线。
    std::vector<VkDescriptorSet> _descriptorSets;

    // 管线布局，描述Shader可访问的Descriptor和Push Constant。
    VkPipelineLayout _pipelineLayout = VK_NULL_HANDLE;

    // 图形管线对象，包含Shader、固定功能阶段和渲染状态。
    VkPipeline _graphicsPipeline = VK_NULL_HANDLE;

    // 交换链帧缓冲区，每个图像视图对应一个Framebuffer。
    std::vector<VkFramebuffer> _swapChainFramebuffers;

    // 命令池，用于分配和管理命令缓冲区。
    VkCommandPool _commandPool = VK_NULL_HANDLE;

    // 命令缓冲区，用于记录绘制命令和状态切换。
    std::vector<VkCommandBuffer> _commandBuffers;

    // 信号量用于同步图像获取和渲染完成。
    std::vector<VkSemaphore> _imageAvailableSemaphores;

    // 每张交换链图像对应的“渲染完成”信号量。
    std::vector<VkSemaphore> _renderFinishedSemaphores;

    // 每个并行帧对应的Fence，防止CPU过早复用帧资源。
    std::vector<VkFence> _inFlightFences;

    // 顶点缓冲区，用于存储顶点数据。
    VkBuffer _vertexBuffer = VK_NULL_HANDLE;

    // 顶点缓冲区的GPU内存句柄。
    VkDeviceMemory _vertexBufferMemory = VK_NULL_HANDLE;

    // 索引缓冲区，用于存储顶点索引数据。
    VkBuffer _indexBuffer = VK_NULL_HANDLE;

    // 索引缓冲区的GPU内存句柄。
    VkDeviceMemory _indexBufferMemory = VK_NULL_HANDLE;

    // Uniform Buffer对象，用于存储每帧的变换矩阵。
    std::vector<VkBuffer> _uniformBuffers;

    // Uniform Buffer的GPU内存句柄，用于存储每帧的变换矩阵。
    std::vector<VkDeviceMemory> _uniformBuffersMemory;

    // Uniform Buffer的CPU可访问内存映射指针，用于更新每帧的变换矩阵。
    std::vector<void *> _uniformBuffersMapped;

    // 窗口尺寸变化标记，在下一帧触发交换链重建。
    bool _framebufferResized = false;

    // 当前正在使用的并行帧索引。
    uint32_t _currentFrame = 0;

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

private:
    const std::vector<Vertex> _vertices = {{{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
                                           {{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
                                           {{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
                                           {{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}}};

    const std::vector<uint16_t> _indices = {0, 1, 2, 2, 3, 0};
};
