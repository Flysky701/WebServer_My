cmake_minimum_required(VERSION 3.10)
project(Webserver_My)

# 基础配置
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)  # 禁用编译器特有扩展，提高代码可移植性

# 构建类型优化（默认 Release）
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    message(STATUS "Build type set to Release (optimized)")
endif()

# 编译器优化选项
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    # 通用优化
    add_compile_options(
        -O3               # 最高优化级别
        -march=native     # 针对本地 CPU 架构优化（SIMD 加速字符串操作）
        -flto             # 链接时优化（跨模块优化）
        -ffast-math       # 激进数学优化（若涉及数值计算）
    )

    # 字符串处理专项优化
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        add_compile_options(
            -foptimize-strlen    # 优化 strlen 和 std::string::size()
            -fno-semantic-interposition  # 减少动态库符号冲突
        )
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        add_compile_options(
            -fvectorize
            -fslp-vectorize
        )
    endif()

    # 链接时优化同步设置
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -flto")
endif()

# 可选 STL 实现优化（Clang 推荐）
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_options(-stdlib=libc++)
    link_libraries(c++abi c++)
endif()


# 文件目录操作
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/public DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

# 包含目录
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/sql
    ${CMAKE_SOURCE_DIR}/include/http
    ${CMAKE_SOURCE_DIR}/include/threadpool
    ${CMAKE_SOURCE_DIR}/include/log
    ${CMAKE_SOURCE_DIR}/include/epollmanager
    ${CMAKE_SOURCE_DIR}/include/connection
    ${CMAKE_SOURCE_DIR}/include/server
    ${CMAKE_SOURCE_DIR}/include/timer
    ${CMAKE_SOURCE_DIR}/include/filehandle
)
# 可执行文件配置
add_executable(server
    src/main.cpp
    # src/test_unit.cpp  # 按需启用
)

#调试用， 使用需禁用优化
# add_compile_options(-fsanitize=address)
# add_link_options(-fsanitize=address)