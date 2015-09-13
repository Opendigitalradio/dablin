# - Try to find libfec
# Once done this will define
#  FEC_FOUND - System has libfec
#  FEC_INCLUDE_DIRS - The libfec include directories
#  FEC_LIBRARIES - The libraries needed to use libfec
#  FEC_DEFINITIONS - Compiler switches required for using libfec

find_package(PkgConfig)
pkg_check_modules(PC_FEC QUIET libfec)
set(FEC_DEFINITIONS ${PC_FEC_CFLAGS_OTHER})

find_path(FEC_INCLUDE_DIR fec.h
    HINTS ${PC_FEC_INCLUDEDIR} ${PC_FEC_INCLUDE_DIRS}
    )

find_library(FEC_LIBRARY NAMES fec
    HINTS ${PC_FEC_LIBDIR} ${PC_FEC_LIBRARY_DIRS}
    )

set(FEC_LIBRARIES ${FEC_LIBRARY})
set(FEC_INCLUDE_DIRS ${FEC_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set FEC_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(FEC DEFAULT_MSG
                                  FEC_LIBRARY FEC_INCLUDE_DIR)

mark_as_advanced(FEC_INCLUDE_DIR FEC_LIBRARY)
