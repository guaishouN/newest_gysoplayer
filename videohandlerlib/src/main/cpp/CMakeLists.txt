cmake_minimum_required(VERSION 3.22.1)
project("videohandlerlib")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(FFMPEG_DIR ${CMAKE_SOURCE_DIR}/ffmpeg)
set(FFMPEG_LIB_DIR ${FFMPEG_DIR}/libs/${ANDROID_ABI})
# 使用静态库编译 recompile with -fPIC问题
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-Bsymbolic")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -L ${FFMPEG_DIR}/libs/${CMAKE_ANDROID_ARCH_ABI}")
message(WARNING "ffmpeg dir: ${FFMPEG_LIB_DIR}")
file(GLOB FFMPEG_LIBS "${FFMPEG_LIB_DIR}/*.a")
message(WARNING "Found precompiled libraries: ${FFMPEG_LIBS}")

include_directories(${FFMPEG_DIR}/include/)
file(GLOB src_files *.cpp *.h)
message(WARNING "source files : ${src_files}")
add_library(gysotools_service SHARED
        ${src_files})

target_link_libraries(
        gysotools_service
        ${FFMPEG_LIBS}
        z
        android
        log)