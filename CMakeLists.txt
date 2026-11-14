
cmake_minimum_required(VERSION 3.21)
project(X5ControlCenter LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

set(SDK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/CameraSDK-2.1.8-20260828_170741-win64")
set(MEDIA_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/MediaSDK")

foreach(required_file
    "${SDK_ROOT}/lib/CameraSDK.lib"
    "${MEDIA_ROOT}/lib/MediaSDK.lib"
    "${SDK_ROOT}/bin/CameraSDK.dll")
    if(NOT EXISTS "${required_file}")
        message(FATAL_ERROR "Missing SDK dependency: ${required_file}")
    endif()
endforeach()

add_executable(X5ControlCenter WIN32 main.cpp)

target_include_directories(X5ControlCenter PRIVATE
    "${SDK_ROOT}/include"
    "${MEDIA_ROOT}/include"
)

target_link_libraries(X5ControlCenter PRIVATE
    Qt6::Widgets
    "${SDK_ROOT}/lib/CameraSDK.lib"
    "${MEDIA_ROOT}/lib/MediaSDK.lib"
)

add_custom_command(TARGET X5ControlCenter POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SDK_ROOT}/bin/CameraSDK.dll"
        "$<TARGET_FILE_DIR:X5ControlCenter>/CameraSDK.dll"

    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${SDK_ROOT}/bin/jsons"
        "$<TARGET_FILE_DIR:X5ControlCenter>/jsons"

    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${MEDIA_ROOT}/bin"
        "$<TARGET_FILE_DIR:X5ControlCenter>"
)
