cmake_minimum_required(VERSION 3.2)

SET(AK_AUTOBAHN_SOURCES
    "AkJson.h"
    "AkVariant.h"
    "autobahn.cpp"
    "autobahn.h"
    "civetweb.cpp"
    "civetweb.h"
    "Client.cpp"
    "Client.h"
    "file_ops.inl"
    "FutureUtils.cpp"
    "FutureUtils.h"
    "handle_form.inl"
    "IWebSocketClientHandler.h"
    "JSONHelpers.h"
    "JsonProvider.cpp"
    "JsonProvider.h"
    "Logger.cpp"
    "Logger.h"
    "md5.inl"
    "sha1.inl"
    "WebSocketClient.cpp"
    "WebSocketClient.h"
)

add_library(AkAutobahn ${AK_AUTOBAHN_SOURCES})
target_include_directories(AkAutobahn PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})