cmake_minimum_required(VERSION 3.16)

set(PLUGIN_NAME "portals-stories")
# The launcher's plugin SDK: a tanmatsu-launcher checkout next to this one
# (built once, for its managed_components), or say where.
if(NOT TANMATSU_PLUGIN_SDK)
    set(TANMATSU_PLUGIN_SDK "${CMAKE_CURRENT_SOURCE_DIR}/../tanmatsu-launcher/tools/plugin-sdk")
endif()

# Before project(): it picks the RISC-V toolchain.
include(${TANMATSU_PLUGIN_SDK}/plugin-build.cmake)
project(${PLUGIN_NAME} C)

build_tanmatsu_plugin(${PLUGIN_NAME} "src/main.c")
if(STORIES_URL)
    target_compile_definitions(${PLUGIN_NAME}_obj PRIVATE STORIES_URL="${STORIES_URL}")
endif()
