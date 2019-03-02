cmake_minimum_required(VERSION 3.2)

SET(REAPER_WAAPI_TRANSFER_SOURCES
  "config.h"
  "reaper_plugin.h"
  "reaper_plugin_functions.h"
  "Reaper_WAAPI_Transfer.cpp"
  "Reaper_WAAPI_Transfer.h"
  "reaper_waapi_transfer.rc"
  "RecallWindowHandler.cpp"
  "RecallWindowHandler.h"
  "RenderQueueReader.cpp"
  "RenderQueueReader.h"
  "resource.h"
  "SearchWindowHandler.cpp"
  "SearchWindowHandler.h"
  "TransferSearch.cpp"
  "TransferSearch.h"
  "TransferWindowHandler.cpp"
  "TransferWindowHandler.h"
  "types.h"
  "WAAPIHelpers.cpp"
  "WAAPIHelpers.h"
  "WAAPIRecall.cpp"
  "WAAPIRecall.h"
  "WAAPITransfer.cpp"
  "WAAPITransfer.h"
  "WwiseSettingsReader.cpp"
  "WwiseSettingsReader.h"
)

SET(SOURCES_RAPIDXML
  "rapidxml/rapidxml.hpp"
  "rapidxml/rapidxml_iterators.hpp"
  "rapidxml/rapidxml_print.hpp"
  "rapidxml/rapidxml_utils.hpp"
)

SET(SOURCES_ICONS 
  "resources/icons/actor_mixer_icon.ico"
  "resources/icons/blend_container_icon.ico"
  "resources/icons/folder_icon.ico"
  "resources/icons/music_segment.ico"
  "resources/icons/random_container_icon.ico"
  "resources/icons/sequence_container_icon.ico"
  "resources/icons/switch_container_icon.ico"
  "resources/icons/work_unit_icon.ico"
)

add_library(reaper_waapi_transfer SHARED ${REAPER_WAAPI_TRANSFER_SOURCES} ${SOURCES_ICONS} ${SOURCES_RAPIDXML})

target_link_libraries(reaper_waapi_transfer AkAutobahn)

if(WIN32)
  target_link_libraries(reaper_waapi_transfer Comctl32.lib)
endif()

file(COPY ${SOURCES_ICONS} DESTINATION resources/icons)

set(REAPER64_PATH "" CACHE PATH "Path to reaper x64 base dir")
set(REAPER32_PATH "" CACHE PATH "Path to reaper x86 base dir")

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(REAPER_ARCH_PATH ${REAPER64_PATH})
else()
  set(REAPER_ARCH_PATH ${REAPER32_PATH})
endif()

set(REAPER_PLUGIN_DIR ${REAPER_ARCH_PATH}/plugins)

if(MSVC)
  if(CMAKE_CL_64)
    set_target_properties(reaper_waapi_transfer PROPERTIES OUTPUT_NAME "reaper_waapi_transfer_x64")
  else()
    set_target_properties(reaper_waapi_transfer PROPERTIES OUTPUT_NAME "reaper_waapi_transfer_x86")
  endif() 

  if(NOT ${REAPER_ARCH_PATH} STREQUAL "")
    set_target_properties(reaper_waapi_transfer PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${REAPER_ARCH_PATH}")
    set_target_properties(reaper_waapi_transfer PROPERTIES VS_DEBUGGER_COMMAND "${REAPER_ARCH_PATH}/reaper.exe")
  endif()

endif()

if(NOT ${REAPER_ARCH_PATH} STREQUAL "")
  add_custom_command(TARGET reaper_waapi_transfer POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:reaper_waapi_transfer> ${REAPER_PLUGIN_DIR}/${OUTPUT_NAME}
      COMMENT "Copying plugin DLL to ${REAPER_PLUGIN_DIR}")
endif()