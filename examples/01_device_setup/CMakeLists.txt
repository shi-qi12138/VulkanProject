# 示例一：窗口、实例、Surface、设备和交换链。
add_executable(Vulkan01DeviceSetup
    HelloTriangleApplication.cpp
    HelloTriangleApplication.h
    main.cpp
)

target_link_libraries(Vulkan01DeviceSetup PRIVATE VulkanFramework)

set_target_properties(Vulkan01DeviceSetup PROPERTIES
    OUTPUT_NAME "01_device_setup")
