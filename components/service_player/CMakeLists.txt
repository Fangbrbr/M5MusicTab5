# service_player：通用音频播放服务（C++ 桥接 micro-mp3，对外 C API）
#
# 依赖 micro-mp3（流式 MP3 解码器，经 main 依赖树注入）与 service_audio（aux 混音输出）。
idf_component_register(SRCS "service_player.cpp"
                       INCLUDE_DIRS "include"
                       REQUIRES service_audio
                                esphome__micro-mp3)