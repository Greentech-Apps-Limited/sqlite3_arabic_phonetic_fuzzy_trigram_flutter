cmake_minimum_required(VERSION 3.14)
set(PROJECT_NAME "sqlite3_arabic_phonetic_fuzzy_trigram_flutter")
project(${PROJECT_NAME} LANGUAGES CXX)
set(PLUGIN_NAME "sqlite3_arabic_phonetic_fuzzy_trigram_flutter_plugin")


# Define the plugin library target. Its name must not be changed (see comment
# on PLUGIN_NAME above).
add_library(${PLUGIN_NAME} SHARED
  "sqlite3_arabic_phonetic_fuzzy_trigram_flutter_plugin.cc"
  "${CMAKE_CURRENT_SOURCE_DIR}/include/sqlite3_arabic_phonetic_fuzzy_trigram/sqlite3.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/include/sqlite3_arabic_phonetic_fuzzy_trigram/common.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/include/sqlite3_arabic_phonetic_fuzzy_trigram/sqlite3-arabic-phonetic-fuzzy-trigram.c"
)

target_compile_features(${PLUGIN_NAME} PUBLIC cxx_std_14)
target_compile_options(${PLUGIN_NAME} PRIVATE "$<$<NOT:$<CONFIG:Debug>>:-O3>")

# Symbols are hidden by default to reduce the chance of accidental conflicts
# between plugins. This should not be removed; any symbols that should be
# exported should be explicitly exported with the FLUTTER_PLUGIN_EXPORT macro.
set_target_properties(${PLUGIN_NAME} PROPERTIES
  CXX_VISIBILITY_PRESET hidden)
target_compile_definitions(${PLUGIN_NAME} PRIVATE FLUTTER_PLUGIN_IMPL)

# Source include directories and library dependencies. Add any plugin-specific
# dependencies here.
target_include_directories(${PLUGIN_NAME} INTERFACE
  "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(${PLUGIN_NAME} PRIVATE flutter)
target_link_libraries(${PLUGIN_NAME} PRIVATE PkgConfig::GTK)
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
