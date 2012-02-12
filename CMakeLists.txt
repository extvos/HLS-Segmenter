project(httplvs-sd)
cmake_minimum_required(VERSION 2.6)

include(detectFFMpeg.cmake)


add_executable(segmenter segmenter.c options_parsing.c debugs.c avconv_adapted_code.c)

target_link_libraries(segmenter -lavformat -lavcodec -lavutil ${FFMPEG_LIBS})
