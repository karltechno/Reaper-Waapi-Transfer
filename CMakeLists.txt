cmake_minimum_required (VERSION 3.2)
project (reaper_waapi_transfer)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(AKSDK_INCLUDE_DIR "$ENV{WWISESDK}/include" CACHE PATH "Path Wwise SDK includes")

# check for wwise sdk
if(NOT EXISTS ${AKSDK_INCLUDE_DIR}/AK/WwiseAuthoringAPI)
  message(FATAL_ERROR "Can't find Wwise SDK with WAAPI in ${AKSDK_INCLUDE_DIR}")
endif()

include_directories(${AKSDK_INCLUDE_DIR} "AkAutobahn/")

add_definitions(-DUSE_WEBSOCKET -D_CRT_SECURE_NO_WARNINGS)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -W3 -D_DEBUG -DVALIDATE_WAMP")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -W3 -DNDEBUG")

if(MSVC)
# PDB in release
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Zi")
  set(CMAKE_SHARED_LINKER_FLAGS  "${CMAKE_SHARED_LINKER_FLAGS} /DEBUG")
endif()

add_subdirectory(AkAutobahn/AkAutobahn)
add_subdirectory(reaper_waapi_transfer)