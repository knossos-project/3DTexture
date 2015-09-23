project(3DTexture)
cmake_minimum_required(VERSION 2.8.11)

if(NOT CMAKE_BUILD_TYPE)#prevent accidental debug builds
    set(CMAKE_BUILD_TYPE "RELEASE")
endif()

include(CMake/cotire.cmake)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)

find_package(Boost REQUIRED)
find_package(GLEW REQUIRED)
find_package(OpenGL REQUIRED)

find_package(Qt5 5.1 REQUIRED COMPONENTS Core Gui OpenGL OpenGLExtensions Widgets)

aux_source_directory(. SRC_LIST)

message(${SRC_LIST})

add_executable(${PROJECT_NAME} ${SRC_LIST})
include_directories(
    ${OPENGL_INCLUDE_DIR}
)
target_link_libraries(${PROJECT_NAME}
    ${GLEW_LIBRARIES}
    ${OPENGL_LIBRARIES}
    -lstdc++
    -lm
    Qt5::Core
    Qt5::Gui
    Qt5::OpenGL
    Qt5::OpenGLExtensions
    Qt5::Widgets
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Winvalid-pch")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -H")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ftime-report")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fuse-ld=gold")

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(${PROJECT_NAME} PRIVATE "-fcolor-diagnostics")
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    target_compile_options(${PROJECT_NAME} PRIVATE "-fdiagnostics-color=always")
endif()

set(COTIRE_MINIMUM_NUMBER_OF_TARGET_SOURCES 1)
cotire(${PROJECT_NAME})
