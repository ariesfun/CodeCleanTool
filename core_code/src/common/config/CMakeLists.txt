cmake_minimum_required(VERSION 3.14)

# ─────────────────────────────────────────────────────────────────────────────
# INI 配置模块：编译为静态库
# ─────────────────────────────────────────────────────────────────────────────
set(CONFIG_TARGET_NAME AlgorithmConfig)

add_library(${CONFIG_TARGET_NAME} STATIC
    src/IniConfig.cpp
    include/IniConfig.h
)

target_include_directories(${CONFIG_TARGET_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

if(NOT WIN32)
    target_link_libraries(${CONFIG_TARGET_NAME} PUBLIC pthread)
endif()

apply_platform_options(${CONFIG_TARGET_NAME})
set_target_output(${CONFIG_TARGET_NAME} static)
