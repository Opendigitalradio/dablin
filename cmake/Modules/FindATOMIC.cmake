# - Try to find libatomic
# Once done this will define
#  ATOMIC_FOUND - System has libatomic
#  ATOMIC_LIBRARIES - The libraries needed to use libatomic

find_library(ATOMIC_LIBRARY NAMES atomic libatomic.so.1
    HINTS ${PC_ATOMIC_LIBDIR} ${PC_ATOMIC_LIBRARY_DIRS}
    )

set(ATOMIC_LIBRARIES ${ATOMIC_LIBRARY})

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set ATOMIC_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(ATOMIC DEFAULT_MSG
                                  ATOMIC_LIBRARY)

mark_as_advanced(ATOMIC_LIBRARY)
