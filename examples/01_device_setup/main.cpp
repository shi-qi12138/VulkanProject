#include "HelloTriangleApplication.h"

#include <cstdlib>
#include <exception>
#include <iostream>

int main()
{
    // main只负责启动应用，并统一输出运行期间的异常。
    HelloTriangleApplication app;

    try {
        app.run();
    } catch (const std::exception& error) {
        std::cerr << "程序运行失败：" << error.what() << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
