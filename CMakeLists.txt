cmake_minimum_required(VERSION 3.16)
project(joy_sharp CXX)

file(GLOB JOY_SHARP_SRC
    src/*.cpp)

SET(FILE_DIALOG_SOURCES
        vendor/ImGuiFileDialog/ImGuiFileDialog/ImGuiFileDialog.cpp
        )

SET(IMGUI_SOURCES
        vendor/imgui/imgui.cpp
        vendor/imgui/imgui_demo.cpp
        vendor/imgui/imgui_draw.cpp
        vendor/imgui/backends/imgui_impl_dx11.cpp
        vendor/imgui/backends/imgui_impl_win32.cpp
        vendor/imgui/imgui_widgets.cpp
        )

set(SOURCES
        ${JOY_SHARP_SRC}
        )

if (CMAKE_BUILD_TYPE MATCHES Debug)
    add_definitions(-DDEBUG=1)
endif ()
if (CMAKE_BINARY_DIR MATCHES "Trace$")
    add_definitions(-DTRACE=1)
endif()

add_definitions(-DIMGUI_USE_STB_SPRINTF=1)

include_directories(vendor/imgui/)
include_directories(vendor/stb/)
include_directories(vendor/dirent/include)
include_directories($ENV{DXSDK_DIR}/Include)
include_directories(src/)
link_directories("$ENV{DXSDK_DIR}/Lib/x86")

add_library(ImGui STATIC ${IMGUI_SOURCES})
add_library(FileDialog STATIC ${FILE_DIALOG_SOURCES})

target_link_libraries(ImGui D3D11.LIB)

target_link_libraries(FileDialog ImGui)

add_executable(joy_sharp ${SOURCES})
add_dependencies(joy_sharp ImGui FileDialog)

set_property(TARGET joy_sharp PROPERTY CXX_STANDARD 20)

SET(LINKED_LIBRARIES
    ImGui
    FileDialog)

if (CMAKE_BINARY_DIR MATCHES "Trace$")
    add_library(TracyClient STATIC tracy/TracyClient.cpp tracy/TracyD3D12.hpp)
    target_include_directories(TracyClient PUBLIC tracy/)
    target_compile_definitions(TracyClient PUBLIC TRACY_ENABLE=1)
    list(APPEND LINKED_LIBRARIES TracyClient)
endif()

target_link_libraries(joy_sharp PUBLIC ${LINKED_LIBRARIES})

add_custom_command(TARGET joy_sharp POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy
                   ${CMAKE_SOURCE_DIR}/fonts/fa-solid-900.ttf
                   ${CMAKE_CURRENT_BINARY_DIR}/fa-solid-900.ttf)