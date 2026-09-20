cmake_minimum_required(VERSION 3.16)

project(CodeCleanTool VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# MSVC UTF-8 源码支持
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")

# 编译日期版本号（每次构建自动更新为当前日期）
string(TIMESTAMP BUILD_DATE "%Y%m%d")

# Qt 路径：优先取环境变量 QT_SDK_DIR，也可用 -DQT_SDK_DIR= 或 -DCMAKE_PREFIX_PATH= 指定
set(QT_SDK_DIR "$ENV{QT_SDK_DIR}" CACHE PATH "Qt SDK 目录")
if(QT_SDK_DIR)
    list(APPEND CMAKE_PREFIX_PATH ${QT_SDK_DIR})
endif()

find_package(Qt5 5.15 REQUIRED COMPONENTS Widgets)

# ElaWidgetTools — 仅构建静态库，跳过 Example
set(ELAWIDGETTOOLS_BUILD_STATIC_LIB ON CACHE BOOL "静态链接 ElaWidgetTools" FORCE)
add_subdirectory(thirdparty/ElaWidgetTools/ElaWidgetTools)

# 通用库（纯 C++，无 Qt 依赖）
set(COMMON_SOURCES
    src/common/log/src/Logger.cpp
    src/common/config/src/IniConfig.cpp
)

# 项目源码 — 业务服务层（每模块独立目录）
set(CORE_SOURCES
    src/scanner/ScanManager.cpp
    src/rules/RuleEngine.cpp
    src/rules/GitIgnoreParser.cpp
    src/cleaner/FileCleaner.cpp
    src/packager/Packager.cpp
    src/config/ConfigManager.cpp
    src/log/LogManager.cpp
    src/model/LogListModel.cpp
    src/model/ResultModel.cpp
)

set(APP_SOURCES
    src/main.cpp
    src/app/MainWindow.cpp
    src/app/MainPresenter.cpp
    src/widgets/StatsWidget.cpp
)

add_executable(${PROJECT_NAME} WIN32
    ${APP_SOURCES}
    ${CORE_SOURCES}
    ${COMMON_SOURCES}
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/common/log/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src/common/config/include
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    BUILD_DATE="${BUILD_DATE}"
    APP_VERSION="1.0.0"
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt5::Widgets
    ElaWidgetTools
)

# 测试
enable_testing()
add_subdirectory(${CMAKE_SOURCE_DIR}/../tests ${CMAKE_BINARY_DIR}/tests)
