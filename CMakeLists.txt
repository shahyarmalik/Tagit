cmake_minimum_required(VERSION 3.21)

project(TagIt
    VERSION 1.0.0
    DESCRIPTION "Intelligent Cross-Platform Music Library Manager"
    HOMEPAGE_URL "https://github.com/tagit/tagit"
    LANGUAGES CXX C)          # C needed for the SQLite amalgamation

# ---------------------------------------------------------------------------
# C++ standard
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Qt MOC / UIC / RCC automation
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Default to Release when the user hasn't specified a build type
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

# IntelliSense / clangd support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
option(TAGIT_BUILD_TESTS  "Build TagIt unit tests"               ON)
option(TAGIT_BUILD_DOCS   "Build Doxygen API documentation"      OFF)
option(TAGIT_ENABLE_ASAN  "Enable AddressSanitizer (gcc/clang)"  OFF)

include(CompilerWarnings)

# ---------------------------------------------------------------------------
# FetchContent — fetch & build all C++ deps from source so the user only
# needs Qt6 installed (one apt line).  All deps are built as static libs.
# ---------------------------------------------------------------------------
include(FetchContent)
set(FETCHCONTENT_QUIET OFF) # show download progress

# Prevent FetchContent targets from polluting install()
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

# Disable AUTOMOC/AUTOUIC/AUTORCC for all fetched deps — they are plain C/C++
# libraries and don't need Qt's meta-object system.
set(CMAKE_AUTOMOC OFF)
set(CMAKE_AUTOUIC OFF)
set(CMAKE_AUTORCC OFF)

# ---- spdlog (header-only / static, very fast) ----------------------------
FetchContent_Declare(spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.14.1
    GIT_SHALLOW    TRUE)
set(SPDLOG_BUILD_SHARED  OFF CACHE BOOL "" FORCE)
set(SPDLOG_INSTALL       OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(spdlog)
set(TAGIT_HAS_SPDLOG ON)
message(STATUS "TagIt: spdlog fetched")

# ---- SQLite3 amalgamation (single .c file, ultra-lightweight) ------------
# We use the official amalgamation zip — no other deps required.
FetchContent_Declare(sqlite3_src
    URL      https://www.sqlite.org/2024/sqlite-amalgamation-3460100.zip
    URL_HASH SHA256=77823cb110929c2bcb0f5d48e4833b5c59a8a6e40cdea3936b99e199dbbe5784
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(sqlite3_src)

# Build SQLite as a tiny static lib from the amalgamation
add_library(sqlite3_static STATIC "${sqlite3_src_SOURCE_DIR}/sqlite3.c")
target_include_directories(sqlite3_static PUBLIC "${sqlite3_src_SOURCE_DIR}")
target_compile_definitions(sqlite3_static PUBLIC
    SQLITE_THREADSAFE=1
    SQLITE_DEFAULT_MEMSTATUS=0   # disable memory statistics for speed
    SQLITE_DEFAULT_WAL_SYNCHRONOUS=1
    SQLITE_LIKE_DOESNT_MATCH_BLOBS
    SQLITE_MAX_EXPR_DEPTH=0      # unlimited expression depth
    SQLITE_OMIT_DEPRECATED
    SQLITE_OMIT_PROGRESS_CALLBACK
    SQLITE_USE_ALLOCA)
set_target_properties(sqlite3_static PROPERTIES POSITION_INDEPENDENT_CODE ON)
add_library(SQLite::SQLite3 ALIAS sqlite3_static)
set(TAGIT_HAS_SQLITE ON)
message(STATUS "TagIt: SQLite3 amalgamation fetched")

# ---- TagLib (full static build) ------------------------------------------
FetchContent_Declare(taglib
    GIT_REPOSITORY https://github.com/taglib/taglib.git
    GIT_TAG        v2.0.2
    GIT_SHALLOW    TRUE)
set(BUILD_SHARED_LIBS             OFF CACHE BOOL "" FORCE)
set(TAGLIB_BUILD_EXAMPLES         OFF CACHE BOOL "" FORCE)
set(TAGLIB_BUILD_TESTS            OFF CACHE BOOL "" FORCE)
set(TAGLIB_BUILD_BINDINGS         OFF CACHE BOOL "" FORCE)
set(WITH_ZLIB                     OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(taglib)
# TagLib v2 exposes the raw target name "tag" during FetchContent builds.
# The installed config adds the TagLib::TagLib alias, so we create it here.
if(NOT TARGET TagLib::TagLib)
    add_library(TagLib::TagLib ALIAS tag)
endif()
set(TAGIT_HAS_TAGLIB ON)
message(STATUS "TagIt: TagLib fetched")

# ---- GoogleTest (only when tests are enabled) ----------------------------
if(TAGIT_BUILD_TESTS)
    FetchContent_Declare(googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.15.2
        GIT_SHALLOW    TRUE)
    set(gtest_force_shared_crt   ON  CACHE BOOL "" FORCE)
    set(BUILD_GMOCK              OFF CACHE BOOL "" FORCE)
    set(INSTALL_GTEST            OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
    message(STATUS "TagIt: GoogleTest fetched")
endif()

# ---- Qt6 (must be system-installed — too large to fetch) -----------------
find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets Network Multimedia)
message(STATUS "TagIt: Qt ${Qt6_VERSION} found")

# Re-enable AUTOMOC/AUTOUIC/AUTORCC for our own Qt-based targets only
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
# ---------------------------------------------------------------------------
# Core library  (model + platform + application core, no UI)
# ---------------------------------------------------------------------------
set(TAGIT_CORE_HEADERS
    src/model/AudioMetadata.h
    src/model/Song.h
    src/platform/IPlatformServices.h
    src/platform/FilesystemService.h
    src/platform/DatabaseService.h
    src/platform/TagService.h
    src/platform/NetworkService.h
    src/platform/linux/LinuxPlatformServices.h
    src/core/SettingsManager.h
    src/core/ILibraryManager.h
    src/core/Logger.h
    src/core/FilenameIntelligence.h
    src/core/MetadataEngine.h
    src/core/LibraryManager.h
    src/core/BackupManager.h
    src/core/DuplicateEngine.h
    src/core/FileOrganizer.h
    src/core/SearchEngine.h
    src/core/ArtworkManager.h
    src/core/ApplicationCore.h
)

set(TAGIT_CORE_SOURCES
    src/model/AudioMetadata.cpp
    src/model/Song.cpp
    src/platform/FilesystemService.cpp
    src/platform/DatabaseService.cpp
    src/platform/TagService.cpp
    src/platform/NetworkService.cpp
    src/platform/linux/LinuxPlatformServices.cpp
    src/core/SettingsManager.cpp
    src/core/Logger.cpp
    src/core/FilenameIntelligence.cpp
    src/core/MetadataEngine.cpp
    src/core/LibraryManager.cpp
    src/core/BackupManager.cpp
    src/core/DuplicateEngine.cpp
    src/core/FileOrganizer.cpp
    src/core/SearchEngine.cpp
    src/core/ArtworkManager.cpp
    src/core/ApplicationCore.cpp
)

add_library(tagit_core STATIC ${TAGIT_CORE_SOURCES} ${TAGIT_CORE_HEADERS})
add_library(TagIt::Core ALIAS tagit_core)

target_include_directories(tagit_core PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>"
    "$<INSTALL_INTERFACE:include>")

target_compile_definitions(tagit_core PUBLIC
    TAGIT_VERSION="${PROJECT_VERSION}"
    TAGIT_HAS_TAGLIB
    TAGIT_HAS_SQLITE
    TAGIT_HAS_SPDLOG)

target_link_libraries(tagit_core PUBLIC
    Qt6::Core
    Qt6::Network
    TagLib::TagLib
    SQLite::SQLite3
    spdlog::spdlog)

# TagLib headers are in a "taglib/" subdirectory inside the fetched source.
# We expose them so #include <fileref.h> works without a path prefix.
target_include_directories(tagit_core PUBLIC
    "$<BUILD_INTERFACE:${taglib_SOURCE_DIR}/taglib>"
    "$<BUILD_INTERFACE:${taglib_SOURCE_DIR}/taglib/toolkit>"
    "$<BUILD_INTERFACE:${taglib_SOURCE_DIR}/taglib/mpeg>"
    "$<BUILD_INTERFACE:${taglib_SOURCE_DIR}/taglib/mpeg/id3v2>"
    "$<BUILD_INTERFACE:${taglib_SOURCE_DIR}/taglib/mpeg/id3v2/frames>"
    "$<BUILD_INTERFACE:${taglib_SOURCE_DIR}/taglib/flac>"
    "$<BUILD_INTERFACE:${taglib_SOURCE_DIR}/taglib/ogg>"
    "$<BUILD_INTERFACE:${taglib_SOURCE_DIR}/taglib/ogg/vorbis>"
    "$<BUILD_INTERFACE:${taglib_SOURCE_DIR}/taglib/ogg/opus>"
    "$<BUILD_INTERFACE:${taglib_SOURCE_DIR}/taglib/mp4>")

target_compile_features(tagit_core PUBLIC cxx_std_20)
enable_warnings(tagit_core)

if(TAGIT_ENABLE_ASAN)
    target_compile_options(tagit_core PRIVATE -fsanitize=address -fno-omit-frame-pointer)
    target_link_options(tagit_core    PRIVATE -fsanitize=address)
endif()

# ---------------------------------------------------------------------------
# Application executable  (Qt Widgets UI)
# ---------------------------------------------------------------------------
set(TAGIT_UI_HEADERS
    src/ui/MainWindow.h
    src/ui/SongTableModel.h
    src/ui/LibraryBrowser.h
    src/ui/MetadataInspector.h
    src/ui/ArtworkInspector.h
    src/ui/ActivityLogView.h
    src/ui/SettingsDialog.h
    src/ui/PlayerWidget.h
)

set(TAGIT_UI_SOURCES
    src/ui/MainWindow.cpp
    src/ui/SongTableModel.cpp
    src/ui/LibraryBrowser.cpp
    src/ui/MetadataInspector.cpp
    src/ui/ArtworkInspector.cpp
    src/ui/ActivityLogView.cpp
    src/ui/SettingsDialog.cpp
    src/ui/PlayerWidget.cpp
)

add_executable(tagit
    src/main.cpp
    src/Application.cpp
    src/Application.h
    ${TAGIT_UI_SOURCES}
    ${TAGIT_UI_HEADERS})

target_include_directories(tagit PRIVATE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>")

target_link_libraries(tagit PRIVATE
    TagIt::Core
    Qt6::Widgets
    Qt6::Multimedia)

target_compile_features(tagit PRIVATE cxx_std_20)
enable_warnings(tagit)

if(TAGIT_ENABLE_ASAN)
    target_compile_options(tagit PRIVATE -fsanitize=address -fno-omit-frame-pointer)
    target_link_options(tagit    PRIVATE -fsanitize=address)
endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(TAGIT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ---------------------------------------------------------------------------
# Documentation
# ---------------------------------------------------------------------------
if(TAGIT_BUILD_DOCS)
    find_package(Doxygen QUIET)
    if(Doxygen_FOUND)
        add_custom_target(docs
            COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            COMMENT "Generating API documentation with Doxygen"
            VERBATIM)
    else()
        message(WARNING "TagIt: Doxygen not found — 'docs' target unavailable")
    endif()
endif()

# ---------------------------------------------------------------------------
# Install / packaging
# ---------------------------------------------------------------------------
include(GNUInstallDirs)

install(TARGETS tagit
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(TARGETS tagit_core
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

install(DIRECTORY src/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/tagit
    FILES_MATCHING PATTERN "*.h")

set(CPACK_PACKAGE_NAME                "tagit")
set(CPACK_PACKAGE_VERSION             "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Intelligent Cross-Platform Music Library Manager")
set(CPACK_PACKAGE_VENDOR              "TagIt")
set(CPACK_PACKAGE_CONTACT             "dev@tagit.example")
set(CPACK_GENERATOR                   "DEB")
include(CPack)
