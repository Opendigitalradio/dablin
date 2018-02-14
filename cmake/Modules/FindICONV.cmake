# - Try to find libiconv (or libc instead)
# Once done this will define
#  ICONV_FOUND - System has libiconv
#  ICONV_INCLUDE_DIRS - The libiconv include directories
#  ICONV_LIBRARIES - The libraries needed to use libiconv
#  ICONV_DEFINITIONS - Compiler switches required for using libiconv

#find_package(PkgConfig)
#pkg_check_modules(PC_ICONV QUIET libiconv)
#set(ICONV_DEFINITIONS ${PC_ICONV_CFLAGS_OTHER})

find_path(ICONV_INCLUDE_DIR iconv.h
    HINTS ${PC_ICONV_INCLUDEDIR} ${PC_ICONV_INCLUDE_DIRS}
    )

find_library(ICONV_LIBRARY NAMES libiconv_a iconv libiconv c
    HINTS ${PC_ICONV_LIBDIR} ${PC_ICONV_LIBRARY_DIRS}
    )

set(ICONV_LIBRARIES ${ICONV_LIBRARY})
set(ICONV_INCLUDE_DIRS ${ICONV_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set ICONV_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(ICONV DEFAULT_MSG
                                  ICONV_LIBRARY ICONV_INCLUDE_DIR)

mark_as_advanced(ICONV_INCLUDE_DIR ICONV_LIBRARY)
