# engine_sf2：上游 SF2Sampler 合成引擎 + shim 兼容层 + C API 适配层
#
# upstream/ 为 vendor 纯净拷贝（除 UPSTREAM_PATCHES.md 登记的补丁外禁止修改）；
# shim/ 通过 include 路径阴影提供 Arduino/FS/String 替代实现。

set(ENGINE_SF2_UPSTREAM_SRCS
    "upstream/synth.cpp"
    "upstream/voice.cpp"
    "upstream/SF2Parser.cpp"
    "upstream/adsr.cpp")

idf_component_register(SRCS "engine_sf2.cpp"
                            "shim/shim.cpp"
                            ${ENGINE_SF2_UPSTREAM_SRCS}
                       INCLUDE_DIRS "include"
                       PRIV_INCLUDE_DIRS "shim" "upstream"
                       REQUIRES service_audio
                                engine_midi)

# 上游代码沿用原工程的性能假设（.ino 中的 -O3/-ffast-math pragma），
# 仅作用于上游源文件；同时抑制移植已知的次要警告。
set_source_files_properties(${ENGINE_SF2_UPSTREAM_SRCS}
                            PROPERTIES COMPILE_FLAGS
                            "-O3 -ffast-math -Wno-format -Wno-attributes -Wno-missing-field-initializers -Wno-type-limits -Wno-sign-compare")
