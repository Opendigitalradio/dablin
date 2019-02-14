########################################################################
# Project setup
########################################################################

cmake_minimum_required(VERSION 2.8.4)
project(dablin C CXX)

# Select the release build type by default to get optimization flags
if(NOT CMAKE_BUILD_TYPE)
   set(CMAKE_BUILD_TYPE "Release")
   message(STATUS "Build type not specified: defaulting to release.")
endif(NOT CMAKE_BUILD_TYPE)
set(CMAKE_BUILD_TYPE ${CMAKE_BUILD_TYPE} CACHE STRING "")

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/Modules)

include(CheckCXXSourceCompiles)

enable_testing()


########################################################################
# Build the FEC library before setting up DABlin application builds.
########################################################################

add_subdirectory(fec)


########################################################################
# Version information
########################################################################

# version derived from git (if possible)
execute_process(
	COMMAND git describe --dirty
	OUTPUT_VARIABLE VERSION_FROM_GIT
	ERROR_QUIET
	OUTPUT_STRIP_TRAILING_WHITESPACE
)

if(VERSION_FROM_GIT)
	add_definitions(-DDABLIN_VERSION="${VERSION_FROM_GIT}")
	set(VERSION_OUTPUT ${VERSION_FROM_GIT})
else()
	set(VERSION_OUTPUT "[see src/version.h]")
endif()

########################################################################
# Compiler specific setup
########################################################################

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_COMPILER_IS_CLANGXX 1)
endif()

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_COMPILER_IS_CLANGXX)
    # C++11
    add_compile_options(-std=c++0x)
    
    # warnings
    add_compile_options(-Wall)
    add_compile_options(-Wextra)
elseif(MSVC)
    # TODO
endif()
if(CMAKE_COMPILER_IS_GNUCXX)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        # strip symbols (TODO: add also for clang)
        SET(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} -s")
    endif()
    
    # hardening (TODO: add also for clang/Cygwin)
    add_compile_options(-fstack-protector-strong)
    add_compile_options(-fPIE)
    SET(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} -pie")
    if(NOT CYGWIN)
        SET(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} -z now")
    endif()
endif()

# support 2GB+ files on 32bit systems
add_definitions(-D_FILE_OFFSET_BITS=64)

# enable newer POSIX API
add_definitions(-D_POSIX_C_SOURCE=200809L)


########################################################################
# Find build dependencies
########################################################################

find_package(PkgConfig)
find_package(Threads REQUIRED)

# SDL2
if(NOT DISABLE_SDL)
    pkg_check_modules(SDL2 sdl2 REQUIRED)
	include_directories(${SDL2_INCLUDE_DIRS})
	link_directories(${SDL2_LIBRARY_DIRS})
	list(APPEND dablin_sources sdl_output.cpp)
else()
    add_definitions(-DDABLIN_DISABLE_SDL)
endif()

# libmpg123
pkg_check_modules(MPG123 libmpg123 REQUIRED)
include_directories(${MPG123_INCLUDE_DIRS})
link_directories(${MPG123_LIBRARY_DIRS})

# gtkmm 3.0
pkg_check_modules(GTKMM gtkmm-3.0)
if(GTKMM_FOUND)
    include_directories(${GTKMM_INCLUDE_DIRS})
    link_directories(${GTKMM_LIBRARY_DIRS})
endif()

if(USE_FDK-AAC)
    # fdk-aac / fdk-aac-dabplus
    pkg_check_modules(FDKAAC fdk-aac)
    pkg_check_modules(FDKAAC-DABPLUS fdk-aac-dabplus)
    if(NOT(FDKAAC_FOUND OR FDKAAC-DABPLUS_FOUND))
        message(FATAL_ERROR "fdk-aac or fdk-aac-dabplus required to compile dablin with USE_FDK-AAC\n")
    endif()
    if(FDKAAC-DABPLUS_FOUND)
        include_directories(${FDKAAC-DABPLUS_INCLUDE_DIRS})
        link_directories(${FDKAAC-DABPLUS_LIBRARY_DIRS})
        list(APPEND AAC_LIB ${FDKAAC-DABPLUS_LIBRARIES})
    else()
        include_directories(${FDKAAC_INCLUDE_DIRS})
        link_directories(${FDKAAC_LIBRARY_DIRS})
        list(APPEND AAC_LIB ${FDKAAC_LIBRARIES})
    endif()
    add_definitions(-DDABLIN_AAC_FDKAAC)
else()
    # libfaad2
    find_package(FAAD REQUIRED)
    include_directories(${FAAD_INCLUDE_DIRS})
    link_directories(${FAAD_LIBRARY_DIRS})
    list(APPEND AAC_LIB ${FAAD_LIBRARIES})
    add_definitions(-DDABLIN_AAC_FAAD2)
endif()

# iconv
find_package(ICONV REQUIRED)
include_directories(${ICONV_INCLUDE_DIRS})
link_directories(${ICONV_LIBRARY_DIRS})

# atomic
list(APPEND CMAKE_REQUIRED_FLAGS -std=c++0x)
check_cxx_source_compiles("
    #include <atomic>
    std::atomic<double> x(1.0);
    int main() {return x;}"
    NATIVE_ATOMICS_SUPPORT
    )
if(NOT NATIVE_ATOMICS_SUPPORT)
    find_package(ATOMIC REQUIRED)
    link_directories(${ATOMIC_LIBRARY_DIRS})
endif()

########################################################################
# Setup DABlin application build
########################################################################

add_subdirectory(src)
add_subdirectory(doc)

########################################################################
# Print Summary
########################################################################
message(STATUS "")
message(STATUS "##########################################################")
message(STATUS "## Building version: ${VERSION_OUTPUT}")
message(STATUS "## Using install prefix: ${CMAKE_INSTALL_PREFIX}")
if(NOT GTKMM_FOUND)
    message(STATUS "## gtkmm not found, thus do not build dablin_gtk")
endif()
message(STATUS "##########################################################")
message(STATUS "")


########################################################################
# Create uninstall target
########################################################################

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY)

add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)

