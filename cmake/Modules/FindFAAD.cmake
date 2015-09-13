# - Try to find libfaad
# Once done this will define
#  FAAD_FOUND - System has libfaad
#  FAAD_INCLUDE_DIRS - The libfaad include directories
#  FAAD_LIBRARIES - The libraries needed to use libfaad
#  FAAD_DEFINITIONS - Compiler switches required for using libfaad

#find_package(PkgConfig)
#pkg_check_modules(PC_FAAD QUIET libfaad)
#set(FAAD_DEFINITIONS ${PC_FAAD_CFLAGS_OTHER})

find_path(FAAD_INCLUDE_DIR faad.h
    HINTS ${PC_FAAD_INCLUDEDIR} ${PC_FAAD_INCLUDE_DIRS}
    )

find_library(FAAD_LIBRARY NAMES faad
    HINTS ${PC_FAAD_LIBDIR} ${PC_FAAD_LIBRARY_DIRS}
    )

set(FAAD_LIBRARIES ${FAAD_LIBRARY})
set(FAAD_INCLUDE_DIRS ${FAAD_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set FAAD_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(FAAD DEFAULT_MSG
                                  FAAD_LIBRARY FAAD_INCLUDE_DIR)

mark_as_advanced(FAAD_INCLUDE_DIR FAAD_LIBRARY)
