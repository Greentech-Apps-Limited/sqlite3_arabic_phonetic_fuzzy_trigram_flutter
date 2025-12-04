# The Flutter tooling requires that developers have a version of Visual Studio
# installed that includes CMake 3.14 or later. You should not increase this
# version, as doing so will cause the plugin to fail to compile for some
# customers of the plugin.
cmake_minimum_required(VERSION 3.14)

# Project-level configuration.
set(PROJECT_NAME "sqlite3_arabic_phonetic_fuzzy_trigram_flutter")
project(${PROJECT_NAME} LANGUAGES CXX C)

# Explicitly opt in to modern CMake behaviors to avoid warnings with recent
# versions of CMake.
cmake_policy(VERSION 3.14...3.25)

# This value is used when generating builds using this plugin, so it must
# not be changed
set(PLUGIN_NAME "sqlite3_arabic_phonetic_fuzzy_trigram_flutter_plugin")

# Build sqlite3 as a separate static library
add_library(sqlite3_lib STATIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include/tokenizer/sqlite3.c"
        )

target_compile_definitions(sqlite3_lib PRIVATE
        SQLITE_ENABLE_FTS3
        SQLITE_ENABLE_FTS3_PARENTHESIS
        SQLITE_ENABLE_FTS3_TOKENIZER
        SQLITE_ENABLE_FTS4
        SQLITE_ENABLE_FTS5
        SQLITE_ENABLE_RTREE
        SQLITE_DQS=0
        SQLITE_DEFAULT_MEMSTATUS=0
        SQLITE_TEMP_STORE=2
        SQLITE_MAX_EXPR_DEPTH=0
        SQLITE_OMIT_AUTHORIZATION
        SQLITE_OMIT_DECLTYPE
        SQLITE_OMIT_DEPRECATED
        SQLITE_OMIT_GET_TABLE
        SQLITE_OMIT_LOAD_EXTENSION
        SQLITE_OMIT_PROGRESS_CALLBACK
        SQLITE_OMIT_SHARED_CACHE
        SQLITE_OMIT_TCL_VARIABLE
        SQLITE_OMIT_TRACE
        SQLITE_USE_ALLOCA
        SQLITE_UNTESTABLE
        SQLITE_HAVE_ISNAN
        SQLITE_HAVE_LOCALTIME_R
        SQLITE_HAVE_LOCALTIME_S
        SQLITE_HAVE_MALLOC_USABLE_SIZE
        SQLITE_HAVE_STRCHRNUL
        )

if(MSVC)
    target_compile_options(sqlite3_lib PRIVATE
            /W3
            /wd4101
            /wd4267
            /wd4189
            )
endif()

# Any new source files that you add to the plugin should be added here.
list(APPEND PLUGIN_SOURCES
        "sqlite3_arabic_phonetic_fuzzy_trigram_flutter_plugin.cpp"
        "sqlite3_arabic_phonetic_fuzzy_trigram_flutter_plugin.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/tokenizer/sqlite3-arabic-phonetic-fuzzy-trigram.c"
        )

# Define the plugin library target. Its name must not be changed (see comment
# on PLUGIN_NAME above).
add_library(${PLUGIN_NAME} SHARED
        "include/sqlite3_arabic_phonetic_fuzzy_trigram_flutter/sqlite3_arabic_phonetic_fuzzy_trigram_flutter_plugin_c_api.h"
        "sqlite3_arabic_phonetic_fuzzy_trigram_flutter_plugin_c_api.cpp"
        ${PLUGIN_SOURCES}
        )

set_target_properties(${PLUGIN_NAME} PROPERTIES
        OUTPUT_NAME "sqlite3-arabic-phonetic-fuzzy-trigram"
        )

# Apply a standard set of build settings
apply_standard_settings(${PLUGIN_NAME})

# Disable warnings as errors for C code (MSVC specific)
if(MSVC)
    target_compile_options(${PLUGIN_NAME} PRIVATE
            /W3
            /wd4101
            /wd4267
            /wd4189
            )
endif()

set_target_properties(${PLUGIN_NAME} PROPERTIES
        CXX_VISIBILITY_PRESET hidden)
target_compile_definitions(${PLUGIN_NAME} PRIVATE FLUTTER_PLUGIN_IMPL)

# SQLite compile definitions
target_compile_definitions(${PLUGIN_NAME} PRIVATE
        SQLITE_ENABLE_FTS3
        SQLITE_ENABLE_FTS3_PARENTHESIS
        SQLITE_ENABLE_FTS3_TOKENIZER
        SQLITE_ENABLE_FTS4
        SQLITE_ENABLE_FTS5
        SQLITE_ENABLE_RTREE
        )

target_include_directories(${PLUGIN_NAME} INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}/include")

# Link against the static sqlite3 library
target_link_libraries(${PLUGIN_NAME} PRIVATE flutter flutter_wrapper_plugin sqlite3_lib)

set(sqlite3_arabic_phonetic_fuzzy_trigram_flutter_bundled_libraries
        ""
        PARENT_SCOPE
        )