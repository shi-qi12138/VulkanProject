cmake_minimum_required(VERSION 3.20)

# vcpkg工具链必须在project()之前加载。
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(_vcpkg_roots "$ENV{VCPKG_ROOT}" "D:/APP/vcpkg")
    foreach(_vcpkg_root IN LISTS _vcpkg_roots)
        file(TO_CMAKE_PATH "${_vcpkg_root}" _vcpkg_root)
        set(_vcpkg_toolchain
            "${_vcpkg_root}/scripts/buildsystems/vcpkg.cmake")
        if(EXISTS "${_vcpkg_toolchain}")
            set(CMAKE_TOOLCHAIN_FILE "${_vcpkg_toolchain}"
                CACHE FILEPATH "vcpkg工具链文件")
            break()
        endif()
    endforeach()
    if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
        message(FATAL_ERROR
            "找不到vcpkg工具链，请设置VCPKG_ROOT或修改默认路径。")
    endif()
endif()

if(WIN32 AND NOT DEFINED VCPKG_TARGET_TRIPLET)
    set(VCPKG_TARGET_TRIPLET "x64-windows" CACHE STRING "vcpkg目标平台")
endif()

project(VulkanLearning VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 所有子工程共用这些第三方依赖。
find_package(Vulkan REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)

# 公共封装和独立示例分别管理。
add_subdirectory(framework)
add_subdirectory(examples)

# Visual Studio默认启动当前示例。
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTY VS_STARTUP_PROJECT Vulkan01DeviceSetup)
