cmake_minimum_required(VERSION 3.28)
project(lsplant)

find_program(CCACHE ccache)

set(CMAKE_CXX_STANDARD 23)

if (CCACHE)
    set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE})
    set(CMAKE_C_COMPILER_LAUNCHER ${CCACHE})
endif ()

if (LSPLANT_STANDALONE)
    find_package(cxx REQUIRED CONFIG)
    link_libraries(cxx::cxx)
endif()

set(SOURCES lsplant.cc)
file(GLOB_RECURSE MODULE_SOURCES CONFIGURE_DEPENDS "*.cxx")
file(GLOB_RECURSE MODULE_INTERFACES CONFIGURE_DEPENDS "*.ixx")
list(FILTER MODULE_SOURCES EXCLUDE REGEX "${PROJECT_SOURCE_DIR}.external.+")
list(FILTER MODULE_INTERFACES EXCLUDE REGEX "${PROJECT_SOURCE_DIR}.external.+")

set(DEX_BUILDER_BUILD_SHARED OFF CACHE INTERNAL "" FORCE)
add_subdirectory(external/dex_builder)

option(LSPLANT_BUILD_SHARED "If ON, lsplant will also build shared library" ON)

if (LSPLANT_BUILD_SHARED)
    message(STATUS "Building lsplant as a shared library")
    add_library(${PROJECT_NAME} SHARED ${SOURCES})
    set_target_properties(${PROJECT_NAME} PROPERTIES CXX_SCAN_FOR_MODULES ON)
    target_sources(${PROJECT_NAME} PRIVATE FILE_SET ms TYPE CXX_MODULES FILES ${MODULE_SOURCES})
    target_sources(${PROJECT_NAME} PUBLIC FILE_SET CXX_MODULES FILES ${MODULE_INTERFACES})
    target_include_directories(${PROJECT_NAME} PUBLIC include)
    target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_options(${PROJECT_NAME} PRIVATE -flto)
    target_link_options(${PROJECT_NAME} PRIVATE -flto)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory ${DEBUG_SYMBOLS_PATH}/${ANDROID_ABI}
            COMMAND ${CMAKE_OBJCOPY} --only-keep-debug $<TARGET_FILE:${PROJECT_NAME}>
            ${DEBUG_SYMBOLS_PATH}/${ANDROID_ABI}/${PROJECT_NAME}
            COMMAND ${CMAKE_STRIP} --strip-all $<TARGET_FILE:${PROJECT_NAME}>)

    target_link_libraries(${PROJECT_NAME} PRIVATE dex_builder_static)
    target_link_libraries(${PROJECT_NAME} PUBLIC log)
endif()

add_library(${PROJECT_NAME}_static STATIC ${SOURCES})
set_target_properties(${PROJECT_NAME}_static PROPERTIES CXX_SCAN_FOR_MODULES ON)
target_sources(${PROJECT_NAME}_static PRIVATE FILE_SET ms TYPE CXX_MODULES FILES ${MODULE_SOURCES})
target_sources(${PROJECT_NAME}_static PUBLIC FILE_SET CXX_MODULES FILES ${MODULE_INTERFACES})
target_include_directories(${PROJECT_NAME}_static PUBLIC include)
target_include_directories(${PROJECT_NAME}_static PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

if (NOT DEFINED DEBUG_SYMBOLS_PATH)
    set(DEBUG_SYMBOLS_PATH ${CMAKE_BINARY_DIR}/symbols)
endif()

target_link_libraries(${PROJECT_NAME}_static PRIVATE dex_builder_static)
target_link_libraries(${PROJECT_NAME}_static PUBLIC log)
