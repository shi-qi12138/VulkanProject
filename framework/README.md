# Framework

这个目录用于存放后续自己封装的公共Vulkan代码。

建议出现重复代码后再逐步提取，例如：

```text
framework/
├── include/vulkan_framework/
│   ├── VulkanContext.h
│   ├── VulkanDevice.h
│   ├── VulkanSwapchain.h
│   └── VulkanBuffer.h
└── src/
    ├── VulkanContext.cpp
    ├── VulkanDevice.cpp
    ├── VulkanSwapchain.cpp
    └── VulkanBuffer.cpp
```
