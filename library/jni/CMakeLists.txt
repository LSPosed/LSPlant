cmake_minimum_required(VERSION 3.4.1)
project(lsplant)

add_definitions(-std=c++20)

set(SOURCES lsplant.cc)

add_subdirectory(external/dex_builder)

add_library(${PROJECT_NAME} SHARED ${SOURCES})
add_library(${PROJECT_NAME}_static STATIC ${SOURCES})

target_include_directories(${PROJECT_NAME} PUBLIC include)
target_include_directories(${PROJECT_NAME}_static PUBLIC include)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(${PROJECT_NAME}_static PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

set(CFLAGS
        -flto
        )

target_compile_options(${PROJECT_NAME} PRIVATE ${CFLAGS})
target_link_options(${PROJECT_NAME} PRIVATE -flto)

target_compile_options(${PROJECT_NAME}_static PRIVATE ${CFLAGS})

target_link_libraries(${PROJECT_NAME} PUBLIC dex_builder log)
target_link_libraries(${PROJECT_NAME}_static PRIVATE dex_builder_static)
target_link_libraries(${PROJECT_NAME}_static PUBLIC log)
