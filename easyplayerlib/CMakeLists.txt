cmake_minimum_required(VERSION 3.4.1)
set(lib_src_DIR ${CMAKE_SOURCE_DIR}/src/main/jniLibs/${ANDROID_ABI})
include_directories(
        ${CMAKE_SOURCE_DIR}/src/main/cpp/include
)
#add_library(x265_lib SHARED IMPORTED)
#set_target_properties(x265_lib PROPERTIES IMPORTED_LOCATION
#                            ${lib_src_DIR}/libx265.so)

add_library(ffmpeg_lib SHARED IMPORTED)
set_target_properties(ffmpeg_lib PROPERTIES IMPORTED_LOCATION
        ${lib_src_DIR}/libffmpeg.so)
link_libraries("${CMAKE_SOURCE_DIR}/src/main/jniLibs/${ANDROID_ABI}/libx264.a")
#add_library(x264 SHARED IMPORTED)
#set_target_properties(x264 PROPERTIES IMPORTED_LOCATION
#        ${lib_src_DIR}/libx264.so)

add_library(freetype SHARED IMPORTED)
set_target_properties(freetype PROPERTIES IMPORTED_LOCATION
        ${lib_src_DIR}/libfreetype.so)

add_library(crypto SHARED IMPORTED)
set_target_properties(crypto PROPERTIES IMPORTED_LOCATION
        ${lib_src_DIR}/libcrypto.so)

add_library(ssl SHARED IMPORTED)
set_target_properties(ssl PROPERTIES IMPORTED_LOCATION
        ${lib_src_DIR}/libssl.so)


add_library(player-lib SHARED
        ${CMAKE_SOURCE_DIR}/src/main/cpp/easyplayer-lib.cpp
        ${CMAKE_SOURCE_DIR}/src/main/cpp/utils.cpp
        ${CMAKE_SOURCE_DIR}/src/main/cpp/coder.cpp
        ${CMAKE_SOURCE_DIR}/src/main/cpp/easyPlayer.cpp
        ${CMAKE_SOURCE_DIR}/src/main/cpp/recorder.cpp
        ${CMAKE_SOURCE_DIR}/src/main/cpp/transform.cpp
        ${CMAKE_SOURCE_DIR}/src/main/cpp/camera.cpp
        )
# Include libraries needed for VideoPlayer lib
target_link_libraries(player-lib
        log
        android
        OpenSLES
        ffmpeg_lib
        freetype
        crypto
        ssl
        )
#x264_lib
#x264_148_lib

