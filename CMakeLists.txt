cmake_minimum_required (VERSION 3.2)
project (reaper_waapi_transfer)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(AKSDK_DIR "$ENV{WWISESDK}/include" CACHE PATH "Path Wwise SDK")
set(AKSDK_INCLUDE_DIR "${AKSDK_DIR}/include")

set(EXT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ext")


# check for wwise sdk
if(NOT EXISTS ${AKSDK_INCLUDE_DIR}/AK/WwiseAuthoringAPI)
  message(FATAL_ERROR "Can't find Wwise SDK with WAAPI in ${AKSDK_INCLUDE_DIR}")
endif()

include_directories(${AKSDK_INCLUDE_DIR})
include_directories("${EXT_DIR}/rapidjson" "${EXT_DIR}/rapidxml")

add_definitions(-DUSE_WEBSOCKET -D_CRT_SECURE_NO_WARNINGS)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -W3 -D_DEBUG -DVALIDATE_WAMP")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -W3 -DNDEBUG")

if(MSVC)
# PDB in release
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Zi")
  set(CMAKE_SHARED_LINKER_FLAGS  "${CMAKE_SHARED_LINKER_FLAGS} /DEBUG")
endif()

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)


add_subdirectory(ext/AkAutobahn)
add_subdirectory(ext/imgui)
add_subdirectory(ext/glad)
add_subdirectory(ext/glfw)
add_subdirectory(reaper_waapi_transfer)