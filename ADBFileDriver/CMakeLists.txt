cmake_minimum_required(VERSION 3.15)
project(ADBFileDriver LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Настройка DLL
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

# Выходной файл
set(DLL_OUTPUT "FileSyncDriver.dll")

# Источники
set(SOURCES
    ADBFileDriver.cpp
)

# Заголовки
set(HEADERS
    ADBFileDriver.h
)

# Создаем DLL
add_library(${DLL_OUTPUT} SHARED
    ${SOURCES}
    ${HEADERS}
)

# Пути включения
target_include_directories(${DLL_OUTPUT} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

# Определения
target_compile_definitions(${DLL_OUTPUT} PRIVATE
    _WINDOWS
    UNICODE
    _UNICODE
)

# Библиотеки для линковки
target_link_libraries(${DLL_OUTPUT} PRIVATE
    ole32.lib
    oleaut32.lib
    shlwapi.lib
)

# Настройка выходной директории
set_target_properties(${DLL_OUTPUT} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/../bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/../bin"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/../bin"
)

# Копирование adb.exe в выходную директорию (опционально)
# add_custom_command(TARGET ${DLL_OUTPUT} POST_BUILD
#     COMMAND ${CMAKE_COMMAND} -E copy_if_different
#         "${CMAKE_SOURCE_DIR}/adb/adb.exe"
#         "$<TARGET_FILE_DIR:${DLL_OUTPUT}>"
# )