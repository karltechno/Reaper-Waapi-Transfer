cmake_minimum_required (VERSION 3.0)
project (reaper_waapi_transfer)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

file(GLOB_RECURSE sources src/*.cpp)
file(GLOB_RECURSE sources_AkAutobahn include/AkAutobahn/*.h include/AkAutobahn/*.cpp)

file(GLOB_RECURSE sources_rapidxml include/rapidxml/*.hpp)
file(GLOB_RECURSE sources_rapidjson include/rapidjson/*.h)

file(GLOB sources_include include/*.h resource.h)

file(GLOB sources_icons resources/icons/*.ico)


set(AKSDK_INCLUDE_DIR "" CACHE PATH "Path Wwise SDK includes")

#check for wwise sdk
if(NOT EXISTS ${AKSDK_INCLUDE_DIR}/AK/WwiseAuthoringAPI)
  message(FATAL_ERROR "Can't find Wwise SDK with WAAPI in ${AKSDK_INCLUDE_DIR}")
endif()

#check for AkAutobahn
if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/include/AkAutobahn/autobahn.h)
  message(FATAL_ERROR "Can't find AkAutobahn in include directory.")
endif()

include_directories ("include" "." ${AKSDK_INCLUDE_DIR})

add_definitions(-DUSE_WEBSOCKET -D_CRT_SECURE_NO_WARNINGS)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -W3 -D_DEBUG -DVALIDATE_WAMP")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -W3 -DNDEBUG")

add_library(reaper_waapi_transfer SHARED ${sources} ${sources_rapidxml} ${sources_rapidjson} ${sources_include} ${sources_AkAutobahn} ${sources_icons} reaper_waapi_transfer.rc)


source_group("AkAutobahn" FILES ${sources_AkAutobahn})
source_group("src" FILES ${sources} reaper_waapi_transfer.rc)
source_group("icons" FILES ${sources_icons})
source_group("rapidjson" FILES ${sources_rapidjson})
source_group("rapidxml" FILES ${sources_rapidxml})
source_group("include" FILES ${sources_include})

if(MSVC)
  if(CMAKE_CL_64)
    set_target_properties(reaper_waapi_transfer PROPERTIES OUTPUT_NAME "reaper_waapi_transfer_x64")
  else()
    set_target_properties(reaper_waapi_transfer PROPERTIES OUTPUT_NAME "reaper_waapi_transfer_x86")
  endif()
endif()

file(COPY ${sources_icons} DESTINATION resources/icons)

if(WIN32)
target_link_libraries(reaper_waapi_transfer Comctl32.lib)
endif()