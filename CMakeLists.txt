cmake_minimum_required(VERSION 3.10)  # bionic's cmake version

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Has to be set before `project()`, and ignored on non-macos:
set(CMAKE_OSX_DEPLOYMENT_TARGET 10.12 CACHE STRING "macOS deployment target (Apple clang only)")

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
  foreach(lang C CXX)
    if(NOT DEFINED CMAKE_${lang}_COMPILER_LAUNCHER AND NOT CMAKE_${lang}_COMPILER MATCHES ".*/ccache")
      message(STATUS "Enabling ccache for ${lang}")
      set(CMAKE_${lang}_COMPILER_LAUNCHER ${CCACHE_PROGRAM} CACHE STRING "")
    endif()
  endforeach()
endif()

project(lokinet
    VERSION 0.8.4
    DESCRIPTION "lokinet - IP packet onion router"
    LANGUAGES C CXX)

set(RELEASE_MOTTO "Proof of soon" CACHE STRING "Release motto")

add_definitions(-DLLARP_VERSION_MAJOR=${lokinet_VERSION_MAJOR})
add_definitions(-DLLARP_VERSION_MINOR=${lokinet_VERSION_MINOR})
add_definitions(-DLLARP_VERSION_PATCH=${lokinet_VERSION_PATCH})
if(RELEASE_MOTTO AND CMAKE_BUILD_TYPE MATCHES "[Rr][Ee][Ll][Ee][Aa][Ss][Ee]")
  add_definitions(-DLLARP_RELEASE_MOTTO="${RELEASE_MOTTO}")
endif()


list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")


# Core options
option(USE_AVX2 "enable avx2 code" OFF)
option(USE_NETNS "enable networking namespace support. Linux only" OFF)
option(NATIVE_BUILD "optimise for host system and FPU" ON)
option(EMBEDDED_CFG "optimise for older hardware or embedded systems" OFF)
option(BUILD_SHARED_LIBS "build lokinet libraries as shared libraries instead of static" OFF)
option(SHADOW "use shadow testing framework. linux only" OFF)
option(XSAN "use sanitiser, if your system has it (requires -DCMAKE_BUILD_TYPE=Debug)" OFF)
option(WITH_JEMALLOC "use jemalloc as allocator" OFF)
option(TESTNET "testnet build" OFF)
option(WITH_COVERAGE "generate coverage data" OFF)
option(USE_SHELLHOOKS "enable shell hooks on compile time (dangerous)" OFF)
option(WARNINGS_AS_ERRORS "treat all warnings as errors. turn off for development, on for release" OFF)
option(TRACY_ROOT "include tracy profiler source" OFF)
option(WITH_TESTS "build unit tests" ON)
option(WITH_HIVE "build simulation stubs" OFF)
option(BUILD_PACKAGE "builds extra components for making an installer (with 'make package')" OFF)

include(cmake/enable_lto.cmake)

option(BUILD_STATIC_DEPS "Download, build, and statically link against core dependencies" OFF)
option(STATIC_LINK "link statically against dependencies" ${BUILD_STATIC_DEPS})
if(BUILD_STATIC_DEPS AND NOT STATIC_LINK)
  message(FATAL_ERROR "Option BUILD_STATIC_DEPS requires STATIC_LINK to be enabled as well")
endif()
if(BUILD_STATIC_DEPS)
  include(StaticBuild)
endif()

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

include(CheckCXXSourceCompiles)
include(CheckLibraryExists)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

include(cmake/target_link_libraries_system.cmake)
include(cmake/add_import_library.cmake)
include(cmake/add_log_tag.cmake)
include(cmake/libatomic.cmake)
include(cmake/link_dep_libs.cmake)

if (STATIC_LINK)
  set(CMAKE_FIND_LIBRARY_SUFFIXES ${CMAKE_STATIC_LIBRARY_SUFFIX})
  message(STATUS "setting static library suffix search")
endif()

add_definitions(-D${CMAKE_SYSTEM_NAME})

if(MSVC_VERSION)
  enable_language(ASM_MASM)
  list(APPEND CMAKE_ASM_MASM_SOURCE_FILE_EXTENSIONS s)
  add_definitions(-D_WIN32_WINNT=0x0600 -DNOMINMAX -DSODIUM_STATIC)
else()
  enable_language(ASM)
endif()

include(cmake/solaris.cmake)
include(cmake/win32.cmake)

# No in-source building
include(MacroEnsureOutOfSourceBuild)
macro_ensure_out_of_source_build("${PROJECT_NAME} requires an out-of-source build.  Create a build directory and run 'cmake ${CMAKE_SOURCE_DIR} [options]'.")

# Always build PIC
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(cmake/unix.cmake)
include(cmake/check_for_std_optional.cmake)
include(cmake/check_for_std_filesystem.cmake)

if(NOT WIN32)
  if(IOS OR ANDROID)
    set(NON_PC_TARGET ON)
  else()
    include(TargetArch)
    target_architecture(COMPILE_ARCH)
    if(COMPILE_ARCH MATCHES i386 OR COMPILE_ARCH MATCHES x86_64)
      set(NON_PC_TARGET OFF)
    else()
      set(NON_PC_TARGET ON)
    endif()
  endif()
endif()

find_package(PkgConfig REQUIRED)


if(NOT BUILD_STATIC_DEPS)
  pkg_check_modules(LIBUV libuv>=1.18.0 IMPORTED_TARGET)
endif()
if(LIBUV_FOUND AND NOT BUILD_STATIC_DEPS)
  add_library(libuv INTERFACE)
  target_link_libraries(libuv INTERFACE PkgConfig::LIBUV)
else()
  if(NOT BUILD_STATIC_DEPS)
    message(FATAL_ERROR "Could not find libu >= 1.28.0; install it on your system or use -DBUILD_STATIC_DEPS=ON")
  endif()
endif()

if(NOT TARGET sodium)
# Allow -D DOWNLOAD_SODIUM=FORCE to download without even checking for a local libsodium
  option(DOWNLOAD_SODIUM "Allow libsodium to be downloaded and built locally if not found on the system" OFF)
  if(NOT DOWNLOAD_SODIUM STREQUAL "FORCE" AND NOT BUILD_STATIC_DEPS)
    pkg_check_modules(SODIUM libsodium>=1.0.18 IMPORTED_TARGET)
  endif()

  add_library(sodium INTERFACE)
  if(SODIUM_FOUND AND NOT DOWNLOAD_SODIUM STREQUAL "FORCE" AND NOT BUILD_STATIC_DEPS)
    target_link_libraries(sodium INTERFACE PkgConfig::SODIUM)
  else()
    if(NOT DOWNLOAD_SODIUM AND NOT BUILD_STATIC_DEPS)
      message(FATAL_ERROR "Could not find libsodium >= 1.0.18; either install it on your system or use -DDOWNLOAD_SODIUM=ON to download and build an internal copy")
    endif()
    message(STATUS "Sodium >= 1.0.18 not found, but DOWNLOAD_SODIUM specified, so downloading it")
    include(DownloadLibSodium)
    target_link_libraries(sodium INTERFACE sodium_vendor)
  endif()

  # Need this target export so that loki-mq properly picks up sodium
  export(TARGETS sodium NAMESPACE sodium:: FILE sodium-exports.cmake)
endif()

option(FORCE_OXENMQ_SUBMODULE "force using oxenmq submodule" OFF)
if(NOT FORCE_OXENMQ_SUBMODULE)
  pkg_check_modules(OXENMQ liboxenmq>=1.2.4)
endif()
if(OXENMQ_FOUND)
  add_library(oxenmq INTERFACE)
  link_dep_libs(oxenmq INTERFACE "${OXENMQ_LIBRARY_DIRS}" ${OXENMQ_LIBRARIES})
  target_include_directories(oxenmq INTERFACE ${OXENMQ_INCLUDE_DIRS})
  add_library(oxenmq::oxenmq ALIAS oxenmq)
  message(STATUS "Found system liboxenmq ${OXENMQ_VERSION}")
else()
  message(STATUS "using oxenmq submodule")
  add_subdirectory(${CMAKE_SOURCE_DIR}/external/oxen-mq)
endif()


# this is messing with release builds
add_compile_options(-U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0)

if (NOT CMAKE_SYSTEM_NAME MATCHES "Linux" AND SHADOW)
  message( FATAL_ERROR "shadow-framework is Linux only" )
endif()

if(XSAN)
  string(APPEND CMAKE_CXX_FLAGS_DEBUG " -fsanitize=${XSAN} -fno-omit-frame-pointer -fno-sanitize-recover")
  foreach(type EXE MODULE SHARED STATIC)
    string(APPEND CMAKE_${type}_LINKER_FLAGS_DEBUG " -fsanitize=${XSAN} -fno-omit-frame-pointer -fno-sanitize-recover")
  endforeach()
  message(STATUS "Doing a ${XSAN} sanitizer build")
endif()

if(CMAKE_BUILD_TYPE MATCHES "[Dd][Ee][Bb][Uu][Gg]")
  add_definitions(-DLOKINET_DEBUG=1)
endif()

if(WITH_SHELLHOOKS)
  add_definitions(-DENABLE_SHELLHOOKS)
endif()

if(TRACY_ROOT)
  include_directories(${TRACY_ROOT})
  add_definitions(-DTRACY_ENABLE)
endif()


if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wno-unknown-warning-option)
endif()

if (NOT MSVC_VERSION)
  add_compile_options(-Wall -Wextra -Wno-unknown-pragmas)
  # vla are evil
  add_compile_options(-Werror=vla)
  add_compile_options(-Wno-unused-function -Wno-deprecated-declarations)
endif()

include(cmake/coverage.cmake)

# these vars are set by the cmake toolchain spec
if (WOW64_CROSS_COMPILE OR WIN64_CROSS_COMPILE)
  include(cmake/cross_compile.cmake)
endif()

if(NATIVE_BUILD)
  if(CMAKE_SYSTEM_PROCESSOR STREQUAL ppc64le)
    add_compile_options(-mcpu=native -mtune=native)
  else()
    add_compile_options(-march=native -mtune=native)
  endif()
elseif(NOT NON_PC_TARGET)
  if (USE_AVX2)
    add_compile_options(-march=haswell -mtune=haswell -mfpmath=sse)
  else()
    # Public binary releases
    add_compile_options(-march=nocona -mtune=haswell -mfpmath=sse)
  endif()
endif()

set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)

if(USE_NETNS)
  add_definitions(-DNETNS=1)
else()
  add_definitions(-DNETNS=0)
endif()

if(TESTNET)
  add_definitions(-DTESTNET=1)
  # 5 times slower than realtime
  add_definitions(-DTESTNET_SPEED=5)
endif()

if(SHADOW)
  include(cmake/shadow.cmake)
endif()

unset(GIT_VERSION)
unset(GIT_VERSION_REAL)

if(NOT GIT_VERSION)
  exec_program("git" ${CMAKE_CURRENT_SOURCE_DIR} ARGS "rev-parse --short HEAD" OUTPUT_VARIABLE GIT_VERSION_UNSTRIP)
  string(STRIP "${GIT_VERSION_UNSTRIP}" GIT_VERSION)
endif()

string(REGEX REPLACE "^fatal.*$" nogit GIT_VERSION_REAL "${GIT_VERSION}")

find_package(PkgConfig REQUIRED)

if (NOT BUILD_STATIC_DEPS)
  pkg_check_modules(UNBOUND libunbound REQUIRED IMPORTED_TARGET)
  add_library(libunbound INTERFACE)
  target_link_libraries(libunbound INTERFACE PkgConfig::UNBOUND)
endif()

pkg_check_modules(SD libsystemd IMPORTED_TARGET)
# Default WITH_SYSTEMD to true if we found it
option(WITH_SYSTEMD "enable systemd integration for sd_notify" ${SD_FOUND})

# Base interface target where we set up global link libraries, definitions, includes, etc.
add_library(base_libs INTERFACE)

if(WITH_SYSTEMD AND (NOT ANDROID))
  if(NOT SD_FOUND)
    message(FATAL_ERROR "libsystemd not found")
  endif()
  target_link_libraries(base_libs INTERFACE PkgConfig::SD)
  target_compile_definitions(base_libs INTERFACE WITH_SYSTEMD)
endif()

option(SUBMODULE_CHECK "Enables checking that vendored library submodules are up to date" ON)
if(SUBMODULE_CHECK)
  find_package(Git)
  if(GIT_FOUND)
    function(check_submodule relative_path)
      execute_process(COMMAND git rev-parse "HEAD" WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${relative_path} OUTPUT_VARIABLE localHead)
      execute_process(COMMAND git rev-parse "HEAD:${relative_path}" WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} OUTPUT_VARIABLE checkedHead)
      string(COMPARE EQUAL "${localHead}" "${checkedHead}" upToDate)
      if (upToDate)
        message(STATUS "Submodule '${relative_path}' is up-to-date")
      else()
        message(FATAL_ERROR "Submodule '${relative_path}' is not up-to-date. Please update with\ngit submodule update --init --recursive\nor run cmake with -DSUBMODULE_CHECK=OFF")
      endif()
    endfunction ()

    message(STATUS "Checking submodules")
    check_submodule(external/nlohmann)
    check_submodule(external/cxxopts)
    check_submodule(external/ghc-filesystem)
    check_submodule(external/date)
    check_submodule(external/pybind11)
    check_submodule(external/sqlite_orm)
    check_submodule(external/oxen-mq)
    check_submodule(external/uvw)
  endif()
endif()

if(WITH_HIVE)
  add_subdirectory(external/pybind11 EXCLUDE_FROM_ALL)
endif()

set(JSON_BuildTests OFF CACHE INTERNAL "")
add_subdirectory(external/nlohmann EXCLUDE_FROM_ALL)
add_subdirectory(external/cxxopts EXCLUDE_FROM_ALL)
add_subdirectory(external/date EXCLUDE_FROM_ALL)

include_directories(SYSTEM external/sqlite_orm/include)

if(ANDROID)
  target_link_libraries(base_libs INTERFACE log)
  target_compile_definitions(base_libs INTERFACE ANDROID)
  set(ANDROID_PLATFORM_SRC android/ifaddrs.c)
endif()

if(TRACY_ROOT)
  target_link_libraries(base_libs INTERFACE dl)
endif()

if(WITH_HIVE)
  add_definitions(-DLOKINET_HIVE=1)
endif()

add_subdirectory(crypto)
add_subdirectory(llarp)
add_subdirectory(daemon)
if(WITH_HIVE)
  add_subdirectory(pybind)
endif()



if (NOT SHADOW)
  if(WITH_TESTS OR WITH_HIVE)
    add_subdirectory(test)
  endif()
  if(ANDROID)
    add_subdirectory(jni)
  endif()
endif()

add_subdirectory(docs)

# uninstall target
if(NOT TARGET uninstall)
  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY)

  add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)
endif()


if(BUILD_PACKAGE)
    include(cmake/installer.cmake)
endif()
