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

# Any new source files that you add to the plugin should be added here.
list(APPEND PLUGIN_SOURCES
        "sqlite3_arabic_phonetic_fuzzy_trigram_flutter_plugin.cpp"
        "sqlite3_arabic_phonetic_fuzzy_trigram_flutter_plugin.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/sqlite3_arabic_phonetic_fuzzy_trigram/sqlite3.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/sqlite3_arabic_phonetic_fuzzy_trigram/common.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/sqlite3_arabic_phonetic_fuzzy_trigram/sqlite3-arabic-phonetic-fuzzy-trigram.c"
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

# Apply a standard set of build settings that are configured in the
# application-level CMakeLists.txt. This can be removed for plugins that want
# full control over build settings.
apply_standard_settings(${PLUGIN_NAME})

# Symbols are hidden by default to reduce the chance of accidental conflicts
# between plugins. This should not be removed; any symbols that should be
# exported should be explicitly exported with the FLUTTER_PLUGIN_EXPORT macro.
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

# Source include directories and library dependencies. Add any plugin-specific
# dependencies here.
target_include_directories(${PLUGIN_NAME} INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(${PLUGIN_NAME} PRIVATE flutter flutter_wrapper_plugin)

# List of absolute paths to libraries that should be bundled with the plugin.
# This list could contain prebuilt libraries, or libraries created by an
# external build triggered from this build file.
set(sqlite3_arabic_phonetic_fuzzy_trigram_flutter_bundled_libraries
        ""
        PARENT_SCOPE
)

