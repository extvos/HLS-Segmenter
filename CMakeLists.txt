project(httplvs-sd)
cmake_minimum_required(VERSION 2.6)

include(detectFFMpeg.cmake)

include_directories(../)
add_subdirectory(../sdrclib sdrclib_build)

add_executable(segmenter segmenter.c options_parsing.c helpers.c )
target_link_libraries(segmenter sdrclib -lm -lavformat -lavcodec -lavutil ${FFMPEG_LIBS})
