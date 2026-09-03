# Vulkan学习项目

这个仓库使用一个CMake工程管理多个独立的Vulkan示例。

## 目录结构

```text
VulkanProject/
├── examples/                 # 每个学习阶段一个独立可执行工程
│   └── 01_device_setup/      # 当前基础设备与交换链示例
├── framework/                # 后续自己封装的公共Vulkan库
├── docs/                     # 学习笔记和资源依赖图
├── CMakeLists.txt            # 根CMake配置
├── CMakePresets.json         # VS2026配置预设
└── vcpkg.json                # GLFW、GLM依赖清单
```

## 添加新示例

例如创建 `examples/02_graphics_pipeline`：

1. 在新目录中添加源码和一个 `CMakeLists.txt`。
2. 在 `examples/CMakeLists.txt` 中添加：

```cmake
add_subdirectory(02_graphics_pipeline)
```

3. 新示例链接公共配置：

```cmake
add_executable(Vulkan02GraphicsPipeline main.cpp)
target_link_libraries(Vulkan02GraphicsPipeline PRIVATE VulkanFramework)
```

Visual Studio重新配置CMake后，会把它显示为新的独立启动目标。

## Shader 编译工具

双击 `tools/启动Shader编译工具.bat` 可以打开图形化 Shader 编译工具。它会自动查找
Vulkan SDK 中的 `glslc`，支持选择 Shader 文件、指定输出文件夹、选择 Shader 阶段、
修改输出文件名，并在没有填写后缀时自动添加 `.spv`。

## 封装建议

学习示例优先保留完整过程。当两个或更多示例出现相同代码时，再把公共部分移动到 `framework`，逐步形成自己的Vulkan封装。
