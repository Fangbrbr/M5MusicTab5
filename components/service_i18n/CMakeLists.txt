# 生成目录必须在组件注册前存在，否则 idf_component_register 的 INCLUDE_DIRS 会报错
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")

idf_component_register(
    SRCS
        "service_i18n.c"
        "${CMAKE_CURRENT_BINARY_DIR}/generated/service_i18n_generated.c"
    INCLUDE_DIRS
        "include"
        "${CMAKE_CURRENT_BINARY_DIR}/generated"
    REQUIRES
        service_nvs
)

find_package(Python REQUIRED)

set(TRANSLATIONS_TSV "${CMAKE_CURRENT_SOURCE_DIR}/translations.tsv")
set(GEN_I18N_PY      "${PROJECT_DIR}/tools/gen_i18n.py")
set(GENERATED_DIR    "${CMAKE_CURRENT_BINARY_DIR}/generated")

set(GENERATED_FILES
    "${GENERATED_DIR}/service_i18n_generated_enum.h"
    "${GENERATED_DIR}/service_i18n_generated.h"
    "${GENERATED_DIR}/service_i18n_generated.c"
)

add_custom_command(
    OUTPUT ${GENERATED_FILES}
    COMMAND ${Python_EXECUTABLE} ${GEN_I18N_PY} ${TRANSLATIONS_TSV} ${GENERATED_DIR}
    DEPENDS ${TRANSLATIONS_TSV} ${GEN_I18N_PY}
    COMMENT "Generating i18n C arrays from translations.tsv"
)

add_custom_target(service_i18n_generate DEPENDS ${GENERATED_FILES})
add_dependencies(${COMPONENT_LIB} service_i18n_generate)
