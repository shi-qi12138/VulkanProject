# 目前只统一传递依赖；以后有公共.cpp时可改成STATIC库。
add_library(VulkanFramework INTERFACE)

target_link_libraries(VulkanFramework
    INTERFACE Vulkan::Vulkan glfw glm::glm)
target_compile_features(VulkanFramework INTERFACE cxx_std_20)

if(MSVC)
    target_compile_options(VulkanFramework INTERFACE /W4 /permissive-)
else()
    target_compile_options(VulkanFramework INTERFACE -Wall -Wextra -Wpedantic)
endif()
