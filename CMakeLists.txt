cmake_minimum_required(VERSION 3.10.0 FATAL_ERROR)
set(CMAKE_MODULAR_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR/cmake}")

project(pandora)

include_directories(external)
include_directories(external/glm)
include_directories(external/gli)
include_directories(external/imgui)
include_directories(external/tinygltf)
include_directories(external/ktx/include)
include_directories(external/ktx/other_include)
include_directories(base)

OPTION(USE_RELATIVE_ASSET_PATH "Load assets (shaders, models, textures) from a fixed path relative to the binary" OFF)
set(RESOURCE_INSTALL_DIR "" CACHE PATH "Path to install resources to (leave empty for running uninstalled)")

find_package(vulkan)
IF (NOT Vulkan_FOUND)
    find_library(Vulkan_LIBRARY NAMES vulkan-1 vulkan PATHS ${CMAKE_SOURCE_DIR}/libs/vulkan)
    IF (Vulkan_LIBRARY)
        set(Vulkan_FOUND ON)
        MESSAGE("Using bundled Vulkan library version")
    ENDIF()
ENDIF()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DVK_USE_PLATFORM_WIN32_KHR")

IF (NOT Vulkan_FOUND)
	message(FATAL_ERROR "Could not find Vulkan library!")
ELSE()
	message(STATUS ${Vulkan_LIBRARY})
ENDIF()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DNOMINMAX -D_USE_MATH_DEFINES")

add_definitions(-D_CRT_SECURE_NO_WARNINGS)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB SOURCE *.cpp )

if(RESOURCE_INSTALL_DIR)
    add_definitions(-DVK_EXAMPLE_ASSETS_DIR=\"{RESOURCE_INSTALL_DIR}/\")
    add_definitions(-DVK_EXAMPLE_SHADERS_DIR=\"{RESOURCE_INSTALL_DIR}/shaders/\")
    install(DIRECTORY assets/ DESTINATION ${RESOURCE_INSTALL_DIR}/)
    install(DIRECTORY shaders/ DESTINATION ${RESOURCE_INSTALL_DIR}/shaders)
else()
    if(NOT USE_RELATIVE_ASSET_PATH)
        add_definitions(-DVK_EXAMPLE_ASSETS_DIR=\"${CMAKE_SOURCE_DIR}/assets/\")
        add_definitions(-DVK_EXAMPLE_SHADERS_DIR=\"${CMAKE_SOURCE_DIR}/shaders/\")
    endif()
endif()

# Asset and shader path selection
IF(MSVC)
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc")
    add_link_options("/ENTRY:mainCRTStartup")
ENDIF(MSVC)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/")

add_subdirectory(base)
add_subdirectory(techniques)