# FindChromaprint.cmake
# ---------------------------------------------------------------------------
# Locates the Chromaprint audio fingerprinting library.
#
# Produces the imported target Chromaprint::Chromaprint when found.

find_path(CHROMAPRINT_INCLUDE_DIR
    NAMES chromaprint.h
    DOC "Chromaprint include directory")

find_library(CHROMAPRINT_LIBRARY
    NAMES chromaprint
    DOC "Chromaprint library")

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Chromaprint
    REQUIRED_VARS CHROMAPRINT_INCLUDE_DIR CHROMAPRINT_LIBRARY)

if(Chromaprint_FOUND AND NOT TARGET Chromaprint::Chromaprint)
    add_library(Chromaprint::Chromaprint UNKNOWN IMPORTED)
    set_target_properties(Chromaprint::Chromaprint PROPERTIES
        IMPORTED_LOCATION "${CHROMAPRINT_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${CHROMAPRINT_INCLUDE_DIR}")
endif()

mark_as_advanced(CHROMAPRINT_INCLUDE_DIR CHROMAPRINT_LIBRARY)

