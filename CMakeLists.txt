cmake_minimum_required(VERSION 3.16)

# ---------------------------------------------------------------------------
# 版本号（全项目唯一可信源，组件经 FW_VERSION 缓存变量注入编译宏）
# 1. CI 可用环境变量 FW_VERSION_OVERRIDE=v1.0.1 显式指定，release 构建不受容器内 git 故障影响
#    （不用 -D 传参：-D 会残留 CMakeCache，污染之后的本地构建）
# 2. HEAD 上有 v* tag → 版本即 tag（约定格式 va.b.c，CI 会校验）
# 3. 无 tag → 7 位 commit hash
# 4. 工作区有未提交修改 → 追加 -dirty（CI 检出必干净，仅本地开发出现）
# 5. git 完全不可用 → "unknown" + 构建期 WARNING（2026-08 CI 容器 dubious ownership 教训）
# ---------------------------------------------------------------------------

set(PROJECT_VERSION_CLEAN "0.0.0")

if(DEFINED ENV{FW_VERSION_OVERRIDE} AND NOT "$ENV{FW_VERSION_OVERRIDE}" STREQUAL "")
    set(FW_VERSION "$ENV{FW_VERSION_OVERRIDE}")
else()
    set(FW_VERSION "")
    execute_process(
        COMMAND git tag --points-at HEAD --list "v*"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE HEAD_TAGS
        RESULT_VARIABLE GIT_RESULT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(GIT_RESULT EQUAL 0 AND HEAD_TAGS)
        # Trap: 同一提交可能挂多个 tag，多行结果会撑破编译宏，只取第一行
        string(REGEX MATCH "^[^\n]+" FW_VERSION "${HEAD_TAGS}")
    else()
        execute_process(
            COMMAND git rev-parse --short=7 HEAD
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            OUTPUT_VARIABLE FW_VERSION
            RESULT_VARIABLE GIT_RESULT
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
    endif()

    if(NOT FW_VERSION)
        set(FW_VERSION "unknown")
        message(WARNING "git 无法获取版本号（非 git 仓库或 git 故障），固件版本将显示 unknown；"
                        "CI 容器内构建请检查 safe.directory 配置")
    else()
        execute_process(
            COMMAND git status --porcelain
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            OUTPUT_VARIABLE GIT_DIRTY
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
        if(GIT_DIRTY)
            set(FW_VERSION "${FW_VERSION}-dirty")
        endif()
    endif()
endif()

# Contract: CMake project(VERSION) 只接受数字版本，app_desc->version 供 IDF/xiaozhi 使用
if(FW_VERSION MATCHES "^v?([0-9]+\\.[0-9]+\\.[0-9]+)")
    set(PROJECT_VERSION_CLEAN "${CMAKE_MATCH_1}")
endif()

message(STATUS "Firmware version: ${FW_VERSION}")

# 经 cache 变量下发，避免组件目录作用域拿不到顶层普通变量
set(FW_VERSION "${FW_VERSION}" CACHE INTERNAL "固件版本字符串，组件注入 FIRMWARE_VERSION 宏用")

set(EXTRA_COMPONENT_DIRS
    "components/engine_midi"
    "components/engine_sf2"
    "components/engine_gui"
    "components/service_audio"
    "components/service_page"
    "components/service_input"
    "components/service_sd"
    "components/service_spiffs"
    "components/service_rtc"
    "components/service_power"
    "components/service_i2c"
    "components/service_usb_host"
    "components/service_usb_device"
    "components/service_wifi"
    "components/service_ble"
    "components/service_nvs"
    "components/service_i18n"
    "components/service_http_client"
    "components/service_xiaozhi"
    "components/app_manager"
    "components/app_zen"
    "components/app_ear_trainer"
    "components/app_circle_of_fifths"
    "components/app_chord_trainer"
    "components/app_xy_pad"
    "components/app_drum_pad"
    "components/app_tiny_piano"
    "components/app_clock_calendar"
    "components/app_ai_agent"
    "components/app_midi_player"
    "components/app_metronome"
    "components/task_gui"
    "components/task_input"
    "components/task_comm"
    "components/task_app"
    "components/task_audio"
    "components/task_ai"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(TAB5_Music_Pad VERSION ${PROJECT_VERSION_CLEAN})

# ---------------------------------------------------------------------------
# SPIFFS 资源镜像：所有字体/图片 bin 文件已内嵌进固件（EMBED_FILES），
# 不再需要 prepare_spiffs.py 脚本收集资源到 SPIFFS 分区。
# ---------------------------------------------------------------------------
find_package(Python REQUIRED)

set(SPIFFS_IMAGE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/spiffs_image)

spiffs_create_partition_image(storage ${SPIFFS_IMAGE_DIR} FLASH_IN_PROJECT)
