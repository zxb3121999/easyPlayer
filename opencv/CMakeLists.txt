cmake_minimum_required(VERSION 3.4.1)
set(lib_src_DIR ${CMAKE_SOURCE_DIR}/src/main/jniLibs/${ANDROID_ABI})
include_directories(
        ${CMAKE_SOURCE_DIR}/src/main/cpp/include
)
add_library(opecv_lib SHARED IMPORTED)
set_target_properties(opecv_lib PROPERTIES IMPORTED_LOCATION
                             ${lib_src_DIR}/libopencv_java3.so)

# build application's shared lib
add_library(face-lib SHARED
            ${CMAKE_SOURCE_DIR}/src/main/cpp/face.cpp)

# Include libraries needed for VideoPlayer lib
target_link_libraries(face
log
android
opencv_lib)

