# FindTagLib.cmake
# ---------------------------------------------------------------------------
# Locates the TagLib audio metadata library.
#
# Produces the imported target TagLib::TagLib when found.

find_path(TAGLIB_INCLUDE_DIR
    NAMES tag.h fileref.h
    PATH_SUFFIXES taglib
    DOC "TagLib include directory")

find_library(TAGLIB_LIBRARY
    NAMES tag
    DOC "TagLib library")

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(TagLib
    REQUIRED_VARS TAGLIB_INCLUDE_DIR TAGLIB_LIBRARY
    VERSION_VAR TAGLIB_VERSION_STRING)

if(TagLib_FOUND AND NOT TARGET TagLib::TagLib)
    add_library(TagLib::TagLib UNKNOWN IMPORTED)
    set_target_properties(TagLib::TagLib PROPERTIES
        IMPORTED_LOCATION "${TAGLIB_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${TAGLIB_INCLUDE_DIR}")
endif()

mark_as_advanced(TAGLIB_INCLUDE_DIR TAGLIB_LIBRARY)

