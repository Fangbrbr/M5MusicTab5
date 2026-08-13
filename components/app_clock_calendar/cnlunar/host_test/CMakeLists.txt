cmake_minimum_required(VERSION 3.16)
project(cnlunar_host_test C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 -Wall -Wextra")

# 宿主机对拍工具：输出逐日文本，与 tools/ref_dump.py (Python 原版) diff 验证
add_executable(dump dump.c ../src/cnlunar.c ../src/cnlunar_rules.c ../src/cnlunar_rules_tables.c ../src/cnlunar_level.c ../src/cnlunar_data.c)
target_include_directories(dump PRIVATE ../include ../src)
