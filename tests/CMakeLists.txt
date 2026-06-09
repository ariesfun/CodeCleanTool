# 探针测试 CMake 配置
# 每个探针是独立可执行文件，通过 add_executable + add_test 注册

# 设置 core_code/src 为公共 include 路径
set(TEST_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/core
)

set(CORE_SRC_DIR ${CMAKE_SOURCE_DIR}/src/core)

# 通用基础设施源文件（多个探针共享）
set(COMMON_SRC_DIR ${CMAKE_SOURCE_DIR}/src/common)

# 测试通用工具头文件路径 (test_common.h)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# ========== 规则引擎探针 ==========
add_executable(test_rule_engine
    rules/test_rule_engine.cpp
    ${CORE_SRC_DIR}/RuleEngine.cpp
)
target_include_directories(test_rule_engine PRIVATE ${TEST_INCLUDE_DIRS})
target_link_libraries(test_rule_engine PRIVATE Qt5::Core)
add_test(NAME RuleEngine COMMAND test_rule_engine)

# ========== GitIgnoreParser 探针 ==========

# test_parser: 规则解析与匹配（纯内存，无文件 I/O）
add_executable(test_gitignore_parser
    gitignore/test_parser.cpp
    ${CORE_SRC_DIR}/GitIgnoreParser.cpp
)
target_include_directories(test_gitignore_parser PRIVATE ${TEST_INCLUDE_DIRS})
target_link_libraries(test_gitignore_parser PRIVATE Qt5::Core)
add_test(NAME GitIgnoreParser COMMAND test_gitignore_parser)

# test_matcher: 文件加载 + 综合匹配（使用临时文件）
add_executable(test_gitignore_matcher
    gitignore/test_matcher.cpp
    ${CORE_SRC_DIR}/GitIgnoreParser.cpp
)
target_include_directories(test_gitignore_matcher PRIVATE ${TEST_INCLUDE_DIRS})
target_link_libraries(test_gitignore_matcher PRIVATE Qt5::Core)
add_test(NAME GitIgnoreMatcher COMMAND test_gitignore_matcher)

# ========== FileCleaner 探针 ==========

# test_delete_file: 单文件删除（正常/只读/不存在/空列表）
add_executable(test_delete_file
    cleaner/test_delete_file.cpp
    ${CORE_SRC_DIR}/FileCleaner.cpp
    ${COMMON_SRC_DIR}/log/src/Logger.cpp
)
target_include_directories(test_delete_file PRIVATE
    ${TEST_INCLUDE_DIRS}
    ${COMMON_SRC_DIR}/log/include
)
target_link_libraries(test_delete_file PRIVATE Qt5::Core)
add_test(NAME FileCleaner_File COMMAND test_delete_file)

# test_delete_dir: 目录删除（空目录/嵌套/多目录/不存在）
add_executable(test_delete_dir
    cleaner/test_delete_dir.cpp
    ${CORE_SRC_DIR}/FileCleaner.cpp
    ${COMMON_SRC_DIR}/log/src/Logger.cpp
)
target_include_directories(test_delete_dir PRIVATE
    ${TEST_INCLUDE_DIRS}
    ${COMMON_SRC_DIR}/log/include
)
target_link_libraries(test_delete_dir PRIVATE Qt5::Core)
add_test(NAME FileCleaner_Dir COMMAND test_delete_dir)

# ========== ConfigManager 探针 ==========

# test_config_manager: 配置读写（默认值/保存/加载/SetValue/GetValue/自定义规则列表）
add_executable(test_config_manager
    config/test_config_manager.cpp
    ${CORE_SRC_DIR}/ConfigManager.cpp
    ${COMMON_SRC_DIR}/config/src/IniConfig.cpp
)
target_include_directories(test_config_manager PRIVATE
    ${TEST_INCLUDE_DIRS}
    ${COMMON_SRC_DIR}/config/include
)
target_link_libraries(test_config_manager PRIVATE Qt5::Core)
add_test(NAME ConfigManager COMMAND test_config_manager)
