# 探针测试 CMake 配置
# 每个探针是独立可执行文件，通过 add_executable + add_test 注册

# core_code/src 为公共 include 路径，模块子目录按需添加
set(TEST_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/src
)

# 各模块源文件目录（按模块独立组织）
set(SCANNER_DIR    ${CMAKE_SOURCE_DIR}/src/scanner)
set(RULES_DIR      ${CMAKE_SOURCE_DIR}/src/rules)
set(CLEANER_DIR    ${CMAKE_SOURCE_DIR}/src/cleaner)
set(PACKAGER_DIR   ${CMAKE_SOURCE_DIR}/src/packager)
set(MODEL_DIR      ${CMAKE_SOURCE_DIR}/src/model)
set(CONFIG_DIR     ${CMAKE_SOURCE_DIR}/src/config)
set(LOG_DIR        ${CMAKE_SOURCE_DIR}/src/log)

set(COMMON_SRC_DIR ${CMAKE_SOURCE_DIR}/src/common)

# 测试通用工具头文件路径 (test_common.h)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# ========== 规则引擎探针 ==========
add_executable(test_rule_engine
    rules/test_rule_engine.cpp
    ${RULES_DIR}/RuleEngine.cpp
)
target_include_directories(test_rule_engine PRIVATE ${TEST_INCLUDE_DIRS})
target_link_libraries(test_rule_engine PRIVATE Qt5::Core)
add_test(NAME RuleEngine COMMAND test_rule_engine)

# ========== GitIgnoreParser 探针 ==========

# test_parser: 规则解析与匹配（纯内存，无文件 I/O）
add_executable(test_gitignore_parser
    gitignore/test_parser.cpp
    ${RULES_DIR}/GitIgnoreParser.cpp
)
target_include_directories(test_gitignore_parser PRIVATE ${TEST_INCLUDE_DIRS})
target_link_libraries(test_gitignore_parser PRIVATE Qt5::Core)
add_test(NAME GitIgnoreParser COMMAND test_gitignore_parser)

# test_matcher: 文件加载 + 综合匹配（使用临时文件）
add_executable(test_gitignore_matcher
    gitignore/test_matcher.cpp
    ${RULES_DIR}/GitIgnoreParser.cpp
)
target_include_directories(test_gitignore_matcher PRIVATE ${TEST_INCLUDE_DIRS})
target_link_libraries(test_gitignore_matcher PRIVATE Qt5::Core)
add_test(NAME GitIgnoreMatcher COMMAND test_gitignore_matcher)

# ========== FileCleaner 探针 ==========

# test_delete_file: 单文件删除（正常/只读/不存在/空列表）
add_executable(test_delete_file
    cleaner/test_delete_file.cpp
    ${CLEANER_DIR}/FileCleaner.cpp
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
    ${CLEANER_DIR}/FileCleaner.cpp
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
    ${CONFIG_DIR}/ConfigManager.cpp
    ${COMMON_SRC_DIR}/config/src/IniConfig.cpp
)
target_include_directories(test_config_manager PRIVATE
    ${TEST_INCLUDE_DIRS}
    ${COMMON_SRC_DIR}/config/include
)
target_link_libraries(test_config_manager PRIVATE Qt5::Core)
add_test(NAME ConfigManager COMMAND test_config_manager)

# ========== ScanManager 探针 ==========

# test_scan_dir: 目录扫描（空目录/清理规则命中/保留过滤/不存在目录/取消）
add_executable(test_scan_dir
    scanner/test_scan_dir.cpp
    ${SCANNER_DIR}/ScanManager.cpp
    ${RULES_DIR}/RuleEngine.cpp
    ${RULES_DIR}/GitIgnoreParser.cpp
    ${MODEL_DIR}/ResultModel.cpp
    ${COMMON_SRC_DIR}/log/src/Logger.cpp
)
target_include_directories(test_scan_dir PRIVATE
    ${TEST_INCLUDE_DIRS}
    ${COMMON_SRC_DIR}/log/include
)
target_link_libraries(test_scan_dir PRIVATE Qt5::Core Qt5::Gui)
add_test(NAME ScanManager COMMAND test_scan_dir)

# ========== Packager 探针 ==========

# test_7z_pack: 打包测试（Find7zPath 检测/前置条件/PackError/实际打包/CancelPack）
add_executable(test_7z_pack
    packager/test_7z_pack.cpp
    ${PACKAGER_DIR}/Packager.cpp
    ${COMMON_SRC_DIR}/log/src/Logger.cpp
)
target_include_directories(test_7z_pack PRIVATE
    ${TEST_INCLUDE_DIRS}
    ${COMMON_SRC_DIR}/log/include
)
target_link_libraries(test_7z_pack PRIVATE Qt5::Core)
add_test(NAME Packager COMMAND test_7z_pack)
