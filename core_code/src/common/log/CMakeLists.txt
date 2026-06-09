cmake_minimum_required(VERSION 3.14)

# ─────────────────────────────────────────────────────────────────────────────
# 公共日志模块：编译为共享库，供 Algorithm 和 AlgorithmFuncTest 链接
# ─────────────────────────────────────────────────────────────────────────────
set(LOG_TARGET_NAME AlgorithmLog)

add_library(${LOG_TARGET_NAME} SHARED
    src/Logger.cpp
    include/Logger.h
)

target_include_directories(${LOG_TARGET_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

if (MSVC)
    target_compile_options(${LOG_TARGET_NAME} PRIVATE # 作用于当前目录及其所有子目录（全局影响）, 会在此处立即生效
        /wd4251      # 屏蔽 STL DLL 导出警告
    )
endif()

# Windows 下导出符号；Linux/Kylin 下可不需要，但保留无妨
target_compile_definitions(${LOG_TARGET_NAME}
    PRIVATE ALGORITHMLOG_BUILD_DLL
    INTERFACE ALGORITHMLOG_USE_DLL
)

# 麒麟/Linux 下链接 pthread
if(NOT WIN32)
    target_link_libraries(${LOG_TARGET_NAME} PUBLIC pthread)
endif()

# 应用统一平台编译选项（警告级别、编码等）
apply_platform_options(${LOG_TARGET_NAME})

set_target_output(${LOG_TARGET_NAME} shared)
