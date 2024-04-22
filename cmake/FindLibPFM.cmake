# CMake module for libpfm4
#
# The following variables will be set after configuration:
# LIBPFM_FOUND
# LIBPFM_LIBRARIES
# LIBPFM_INCLUDE_DIRS
# LIBPFM_VERSION

include(FeatureSummary)
include(FindPackageHandleStandardArgs)

find_path(LIBPFM_INCLUDE_DIR NAMES perfmon/pfmlib.h)
find_library(LIBPFM_LIBRARY NAMES pfm)

# Follow symbolic link and try to extract library version from the .so file.
get_filename_component(LIBPFM_LIBRARY_REALPATH "${LIBPFM_LIBRARY}" REALPATH)
string(REGEX MATCH "([0-9]+\\.[0-9]+\\.[0-9]+)$" LIBPFM_VERSION ${LIBPFM_LIBRARY_REALPATH})

find_package_handle_standard_args(LibPFM REQUIRED_VARS LIBPFM_LIBRARY LIBPFM_INCLUDE_DIR VERSION_VAR LIBPFM_VERSION)

if (LIBPFM_FOUND)
    set(LIBPFM_LIBRARIES "pfm")
    set(LIBPFM_INCLUDE_DIRS "${LIBPFM_INCLUDE_DIR}")
    mark_as_advanced(LIBPFM_LIBRARIES LIBPFM_INCLUDE_DIRS LIBPFM_VERSION)
endif()
