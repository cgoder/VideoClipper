cmake_minimum_required(VERSION 3.4.1)

add_library( native-lib
             SHARED
             ${CMAKE_SOURCE_DIR}/src/main/cpp/native-lib.cpp )

find_library( log-lib
              log )

add_library( ffmpeg
             SHARED
             IMPORTED )
set_target_properties( ffmpeg
                       PROPERTIES IMPORTED_LOCATION
                       ${CMAKE_SOURCE_DIR}/src/main/jniLibs/${ANDROID_ABI}/libffmpeg.so )

include_directories(${CMAKE_SOURCE_DIR}/src/libs/ffmpeg/include)

target_link_libraries( native-lib
                       ffmpeg
                       ${log-lib} )