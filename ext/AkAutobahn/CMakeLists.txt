cmake_minimum_required(VERSION 3.2)
file(GLOB AK_AUTOBAHN_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/AkAutobahn/*.cpp)

add_library(AkAutobahn ${AK_AUTOBAHN_SOURCES})
target_compile_definitions(AkAutobahn PRIVATE WIN32_LEAN_AND_MEAN)
target_include_directories(AkAutobahn PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/AkAutobahn)