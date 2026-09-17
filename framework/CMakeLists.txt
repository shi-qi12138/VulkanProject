# 公共静态库：集中编译第三方实现和后续Vulkan封装代码。
add_library(VulkanFramework STATIC
    src/StbImage.cpp
)

target_link_libraries(VulkanFramework
    PUBLIC
        Vulkan::Vulkan
        glfw
        glm::glm
)

# stb由vcpkg以头文件形式提供。
target_include_directories(VulkanFramework
    PUBLIC
        ${STB_INCLUDE_DIRS}
)

target_compile_features(VulkanFramework PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(VulkanFramework PUBLIC /W4 /permissive-)
else()
    target_compile_options(VulkanFramework PUBLIC -Wall -Wextra -Wpedantic)
endif()
