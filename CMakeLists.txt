# Cmake script for ADBFileDriver
# External component for 1C:Enterprise 8.3
# File exchange via USB/ADB

cmake_minimum_required(VERSION 3.10...3.31 FATAL_ERROR)

project(ADBFileDriver VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# ADBFileDriver sources
set(ADBFileDriver_SRC
    ADBFileDriver/ADBFileDriver.cpp
    ADBFileDriver/ADBFileDriver.h
)

# Definition file for exports (Windows)
set(ADBFileDriver_DEF ADBFileDriver.def)

set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} ${CMAKE_SOURCE_DIR})

if (WIN32)
    set(CMAKE_LINK_DEF_FILE_FLAG ADBFileDriver.def)
    add_definitions(-DUNICODE -DWIN32 -D_AFXDLL)
endif()

# ============================================================
# Две версии библиотеки: Win32 (i386) и Win64 (x86_64)
# ============================================================

if (WIN32)
    # ------------------------------------------
    # ADBFileDriver_Win32 — для arch="i386"
    # ------------------------------------------
    add_library(ADBFileDriver_Win32 SHARED ${ADBFileDriver_SRC})

    # Add .def file for symbol exports
    target_sources(ADBFileDriver_Win32 PRIVATE ${ADBFileDriver_DEF})

    # Выходная директория
    set_target_properties(ADBFileDriver_Win32 PROPERTIES
        CLEAN_DIRECT_OUTPUT 1
        OUTPUT_NAME ADBFileDriver_Win32
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/1C/Release/Win32"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/1C/Release/Win32"
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/1C/Release/Win32"
    )

    # Windows-specific compile options
    target_compile_options(ADBFileDriver_Win32 PRIVATE /utf-8 /EHsc)

    # Link Windows libraries
    target_link_libraries(ADBFileDriver_Win32
        ole32.lib
        oleaut32.lib
        shlwapi.lib
        advapi32.lib
    )

    # ------------------------------------------
    # ADBFileDriver_Win64 — для arch="x86_64"
    # ------------------------------------------
    add_library(ADBFileDriver_Win64 SHARED ${ADBFileDriver_SRC})

    # Add .def file for symbol exports
    target_sources(ADBFileDriver_Win64 PRIVATE ${ADBFileDriver_DEF})

    # Выходная директория
    set_target_properties(ADBFileDriver_Win64 PROPERTIES
        CLEAN_DIRECT_OUTPUT 1
        OUTPUT_NAME ADBFileDriver_Win64
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/1C/Release/Win64"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/1C/Release/Win64"
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/1C/Release/Win64"
    )

    # Windows-specific compile options
    target_compile_options(ADBFileDriver_Win64 PRIVATE /utf-8 /EHsc)

    # Link Windows libraries
    target_link_libraries(ADBFileDriver_Win64
        ole32.lib
        oleaut32.lib
        shlwapi.lib
        advapi32.lib
    )
endif()

# Platform-specific settings
if (UNIX)
    set_source_files_properties(${ADBFileDriver_SRC}
        PROPERTIES COMPILE_FLAGS
        "-fPIC"
    )
endif()