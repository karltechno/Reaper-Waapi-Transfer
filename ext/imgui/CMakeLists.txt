cmake_minimum_required(VERSION 3.2)
file(GLOB AK_AUTOBAHN_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/AkAutobahn/*.cpp)

set(IMGUI_SOURCES
    imgui.cpp
    imgui_demo.cpp
    imgui_draw.cpp
    imgui_widgets.cpp
    examples/imgui_impl_opengl3.cpp
)

add_library(imgui ${IMGUI_SOURCES})
target_compile_definitions(imgui PRIVATE "IMGUI_IMPL_OPENGL_LOADER_GLAD")
target_include_directories(imgui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(imgui glad)