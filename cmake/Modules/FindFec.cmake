# Try to find FEC library and include path.
# Once done this will define
#
# FEC_INCLUDE_DIRS - where to find fec.h, etc.
# FEC_LIBRARIES - List of libraries when using libFEC.
# FEC_FOUND - True if libFEC found.

find_path(FEC_INCLUDE_DIR fec.h DOC "The directory where fec.h resides")
find_library(FEC_LIBRARY NAMES fec DOC "The libFEC library")

if(FEC_INCLUDE_DIR AND FEC_LIBRARY)
  set(FEC_FOUND 1)
  set(FEC_LIBRARIES ${FEC_LIBRARY})
  set(FEC_INCLUDE_DIRS ${FEC_INCLUDE_DIR})
else(FEC_INCLUDE_DIR AND FEC_LIBRARY)
  set(FEC_FOUND 0)
  set(FEC_LIBRARIES)
  set(FEC_INCLUDE_DIRS)
endif(FEC_INCLUDE_DIR AND FEC_LIBRARY)

mark_as_advanced(FEC_INCLUDE_DIR)
mark_as_advanced(FEC_LIBRARY)
mark_as_advanced(FEC_FOUND)

if(NOT FEC_FOUND)
  set(FEC_DIR_MESSAGE "libfec was not found. Make sure FEC_LIBRARY and FEC_INCLUDE_DIR are set.")
  if(NOT FEC_FIND_QUIETLY)
    message(STATUS "${FEC_DIR_MESSAGE}")
  else(NOT FEC_FIND_QUIETLY)
    if(FEC_FIND_REQUIRED)
      message(FATAL_ERROR "${FEC_DIR_MESSAGE}")
    endif(FEC_FIND_REQUIRED)
  endif(NOT FEC_FIND_QUIETLY)
endif(NOT FEC_FOUND)
