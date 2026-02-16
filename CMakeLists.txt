cmake_minimum_required(VERSION 3.20)
project(typesetting VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Library sources
file(GLOB_RECURSE SOURCES src/*.cpp)
file(GLOB_RECURSE HEADERS include/typesetting/*.h)

add_library(typesetting STATIC ${SOURCES} ${HEADERS})

target_include_directories(typesetting
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Platform-specific linking
if(APPLE)
    find_library(CORE_TEXT CoreText)
    find_library(CORE_FOUNDATION CoreFoundation)
    find_library(CORE_GRAPHICS CoreGraphics)
    target_link_libraries(typesetting PRIVATE ${CORE_TEXT} ${CORE_FOUNDATION} ${CORE_GRAPHICS})
endif()

# Tests
option(TYPESETTING_BUILD_TESTS "Build tests" ON)
if(TYPESETTING_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
