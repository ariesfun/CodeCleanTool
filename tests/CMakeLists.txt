# 探针测试 CMake 配置
# 每个探针是独立可执行文件，通过 add_executable + add_test 注册

# 设置 core_code/src 为公共 include 路径
set(TEST_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/core
)

set(CORE_SRC_DIR ${CMAKE_SOURCE_DIR}/src/core)

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
