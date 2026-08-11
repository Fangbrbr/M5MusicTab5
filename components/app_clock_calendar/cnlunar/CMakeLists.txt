# cnlunar ESP-IDF 组件
# 用法：将本目录复制到工程的 components/ 下，或在 CMake 中
#   set(EXTRA_COMPONENT_DIRS /path/to/this/dir)
idf_component_register(
    SRCS "src/cnlunar.c" "src/cnlunar_rules.c" "src/cnlunar_rules_tables.c" "src/cnlunar_level.c" "src/cnlunar_data.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "src"
)
