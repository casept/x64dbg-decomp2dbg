cmake_minimum_required(VERSION 3.25 FATAL_ERROR)

project(
  decomp2dbg
  VERSION 0.1
  DESCRIPTION
    "x64dbg plugin for integrating with decomp2dbg."
  LANGUAGES CXX
)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(PLUGIN_BITNESS "64")
endif()
if(CMAKE_SIZEOF_VOID_P EQUAL 4)
  set(PLUGIN_BITNESS "32")
endif()

add_subdirectory(fmt EXCLUDE_FROM_ALL)

add_subdirectory(client EXCLUDE_FROM_ALL)

add_library(decomp2dbg SHARED
  src/graph.cpp
  src/modules.cpp
  src/types.cpp
  src/plugin.cpp
  src/pluginmain.cpp
)

set_target_properties(decomp2dbg PROPERTIES OUTPUT_NAME "decomp2dbg" PREFIX "" SUFFIX ".dp${PLUGIN_BITNESS}")

add_library(x64dbgSdk SHARED IMPORTED)
set_target_properties(x64dbgSdk PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/x64dbg/"
  # Needed so GCC doesn't freak out about missing _imp__ symbols
  IMPORTED_IMPLIB "${CMAKE_SOURCE_DIR}/x64dbg/pluginsdk/x${PLUGIN_BITNESS}dbg.lib"
)

# For technical reasons, it doesn't seem possible to add multiple DLL's to one IMPORTED lib
add_library(x64dbgSdkBridge SHARED IMPORTED)
set_target_properties(x64dbgSdkBridge PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/x64dbg/"
  # Needed so GCC doesn't freak out about missing _imp__Dbg symbols
  IMPORTED_IMPLIB "${CMAKE_SOURCE_DIR}/x64dbg/pluginsdk/x${PLUGIN_BITNESS}bridge.lib"
)


target_link_libraries(decomp2dbg PRIVATE x64dbgSdk x64dbgSdkBridge client fmt::fmt)

# Bumping this to C++20 breaks the SDK
target_compile_features(decomp2dbg PUBLIC cxx_std_17)

# Hack to force cmake to add system (libstdc++) header path to
# compile_commands.json. This also adds a lot of junk, but as long as it doesn't
# slow clangd too much I'll deal.
if(CMAKE_EXPORT_COMPILE_COMMANDS)
  set(CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES
      ${CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES})
endif()
