cmake_minimum_required(VERSION 3.15)
project(ADBFileDriver VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Настройка DLL
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

# Версия компоненты
set(COMPONENT_VERSION "1.0.0.3")

# Имена целей (targets) с версией
set(TARGET_WIN64 "ADBFileDriver_${COMPONENT_VERSION}_x64")
set(TARGET_WIN32 "ADBFileDriver_${COMPONENT_VERSION}_x86")

# Источники
set(SOURCES
    ADBFileDriver.cpp
)

# Заголовки
set(HEADERS
    ADBFileDriver.h
)

# ============================================================
# Win64 сборка
# ============================================================
add_library(${TARGET_WIN64} SHARED
    ${SOURCES}
    ${HEADERS}
)

target_include_directories(${TARGET_WIN64} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_compile_definitions(${TARGET_WIN64} PRIVATE
    _WINDOWS
    UNICODE
    _UNICODE
)

target_link_libraries(${TARGET_WIN64} PRIVATE
    ole32.lib
    oleaut32.lib
    shlwapi.lib
)

set_target_properties(${TARGET_WIN64} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/../../1C/Release/Win64"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/../../1C/Release/Win64"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/../../1C/Release/Win64"
    OUTPUT_NAME "ADBFileDriver_${COMPONENT_VERSION}_x64"
)

# ============================================================
# Win32 сборка
# ============================================================
add_library(${TARGET_WIN32} SHARED
    ${SOURCES}
    ${HEADERS}
)

target_include_directories(${TARGET_WIN32} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_compile_definitions(${TARGET_WIN32} PRIVATE
    _WINDOWS
    UNICODE
    _UNICODE
)

target_link_libraries(${TARGET_WIN32} PRIVATE
    ole32.lib
    oleaut32.lib
    shlwapi.lib
)

set_target_properties(${TARGET_WIN32} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/../../1C/Release/Win32"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/../../1C/Release/Win32"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/../../1C/Release/Win32"
    OUTPUT_NAME "ADBFileDriver_${COMPONENT_VERSION}_x86"
)
