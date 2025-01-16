cmake_minimum_required(VERSION 3.5)

project(lua-ffi C)

include(FindPkgConfig)

# Version settings
set(LUA_FFI_VERSION_MAJOR 1)
set(LUA_FFI_VERSION_MINOR 1)
set(LUA_FFI_VERSION_PATCH 0)

# Define options for selecting Lua versions
option(USE_LUA51 "Force select Lua5.1")
option(USE_LUA52 "Force select Lua5.2")
option(USE_LUA53 "Force select Lua5.3")
option(USE_LUA54 "Force select Lua5.4")

# Helper function to find and include Lua
function(find_and_include_lua version)
    pkg_search_module(LUA lua-${version})
    if (LUA_FOUND)
        include_directories(${LUA_INCLUDE_DIRS})
    else()
        message(FATAL_ERROR "Liblua${version} is required.")
    endif()
endfunction()

if (LUA_INCLUDE_DIR)
    include_directories(${LUA_INCLUDE_DIR})
else()
    # Check which Lua version to use
    if(USE_LUA51)
        find_and_include_lua(5.1)
        set(LUA_VERSION_MAJOR 5)
        set(LUA_VERSION_MINOR 1)
    elseif(USE_LUA52)
        find_and_include_lua(5.2)
        set(LUA_VERSION_MAJOR 5)
        set(LUA_VERSION_MINOR 2)
    elseif(USE_LUA53)
        find_and_include_lua(5.3)
        set(LUA_VERSION_MAJOR 5)
        set(LUA_VERSION_MINOR 3)
    elseif(USE_LUA54)
        find_and_include_lua(5.4)
        set(LUA_VERSION_MAJOR 5)
        set(LUA_VERSION_MINOR 4)
    else()
        find_package(Lua REQUIRED)
        include_directories(${LUA_INCLUDE_DIR})
    endif()
endif()

find_package(FLEX REQUIRED)

pkg_search_module(LIBFFI libffi)
if (NOT LIBFFI_FOUND)
    message(FATAL_ERROR "libffi is required.")
endif()

add_compile_options(-D_GNU_SOURCE -DLUA_USE_LINUX -Os -Wall -Werror --std=gnu99 -fno-strict-aliasing)

# configure a header file to pass some of the CMake settings to the source code
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)

include_directories(${LIBFFI_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

if (NOT LUA_INSTALL_PREFIX)
    if (LUA_VERSION_MAJOR AND LUA_VERSION_MINOR)
        set(LUA_INSTALL_PREFIX lib/lua/${LUA_VERSION_MAJOR}.${LUA_VERSION_MINOR})
    else()
        set(LUA_INSTALL_PREFIX lib/lua)
    endif()
endif()

flex_target(cparser lex.l ${CMAKE_CURRENT_BINARY_DIR}/lex.c
    DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/lex.h)

add_library(lffi MODULE ffi.c ${CMAKE_CURRENT_BINARY_DIR}/lex.c)
target_link_libraries(lffi PRIVATE ${LIBFFI_LIBRARIES})
set_target_properties(lffi PROPERTIES OUTPUT_NAME ffi PREFIX "")

install(
    TARGETS lffi
    DESTINATION ${LUA_INSTALL_PREFIX}
)
