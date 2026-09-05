cmake_minimum_required(VERSION 3.16)
project(engine_sequencer_host_test C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 -Wall -Wextra")

# 宿主机自测：欧几里得验收向量 / M5P 往返+CRC / swing 触发点
# 引擎依赖 engine_midi 生产者入口，此目录内用 engine_midi.h stub shadow 真实头
add_executable(test_engine_seq test_main.c ../engine_sequencer.c)
target_include_directories(test_engine_seq PRIVATE . ../include)
