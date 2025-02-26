cmake_minimum_required(VERSION 3.10.2 FATAL_ERROR)

#############
# VARIABLES #
#############

# VERSIONING
set(MAJOR_VERSION 1)
set(MINOR_VERSION 4)
set(PATCH_VERSION 1)
set(EXTRA_VERSION "-1")
set(VERSION_STRING ${MAJOR_VERSION}.${MINOR_VERSION}.${PATCH_VERSION}${EXTRA_VERSION})

# PROJECT NAME
project(wofi LANGUAGES C)
set(TARGET wofi)

# PROJECT FLAGS
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED True)

# COMPILER FLAGS
set(CMAKE_C_FLAGS_DEBUG_INIT "-O0 -g3 -ggdb -Wall")
set(CMAKE_C_FLAGS_RELEASE_INIT "-O3")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -rdynamic")

# PROJECT PATH
set(__MANUAL_PATH ${PROJECT_SOURCE_DIR}/man)
set(__HEADER_PATH ${PROJECT_SOURCE_DIR}/inc)
set(__PROTOCOL_PATH ${PROJECT_SOURCE_DIR}/proto)
set(__MODES_PATH ${PROJECT_SOURCE_DIR}/modes)
set(__SOURCE_PATH ${PROJECT_SOURCE_DIR}/src)

################
# DEPENDENCIES #
################

# PKGCONFIG
find_package(PkgConfig REQUIRED)

# LIBM
find_library(MATH_LIBRARY NAMES m libm HINTS ${CMAKE_PREFIX_PATH}/lib)

# GLIB
pkg_check_modules(GLIB2 glib-2.0 REQUIRED)
include_directories(${GLIB2_INCLUDE_DIRS})

# GOBJECT
pkg_check_modules(GLIB2 gobject-2.0 REQUIRED)
include_directories(${GOBJECT_INCLUDE_DIRS})

# GTK3
pkg_check_modules(GTK3 gtk+-3.0 REQUIRED)
include_directories(${GTK3_INCLUDE_DIRS})

# WAYLAND
pkg_check_modules(WAYLAND_CLIENT wayland-client REQUIRED)
include_directories(${WAYLAND_CLIENT_INCLUDE_DIRS})

# THREADS
find_library(THREADS_LIBRARY NAMES pthread HINTS ${CMAKE_PREFIX_PATH}/lib)

# DL
find_library(DL_LIBRARY NAMES dl libdl HINTS ${CMAKE_PREFIX_PATH}/lib)

#########
# SETUP #
#########

set(__ALL_OPTIONAL_FILES "")
if(ENABLE_RUN)
  set(__ALL_OPTIONAL_FILES ${__ALL_OPTIONAL_FILES} ${__MODES_PATH}/run.c)
endif()

if(ENABLE_DRUN)
  set(__ALL_OPTIONAL_FILES ${__ALL_OPTIONAL_FILES} ${__MODES_PATH}/drun.c)
  pkg_check_modules(GIO_UNIX gio-unix-2.0 REQUIRED)
  include_directories(${GIO_UNIX_DIRS})
endif()

if(ENABLE_DMENU)
  set(__ALL_OPTIONAL_FILES ${__ALL_OPTIONAL_FILES} ${__MODES_PATH}/dmenu.c)
endif()

# COMPILER PREPARATION
include_directories(${__HEADER_PATH} ${__SOURCE_PATH})
file(GLOB_RECURSE __ALL_HEADERS "${__HEADER_PATH}/*.h")
file(GLOB_RECURSE __ALL_PROTOCOLS "${__PROTOCOL_PATH}/*.c")
file(GLOB_RECURSE __ALL_SOURCES "${__SOURCE_PATH}/*.c")
set(HEADERS ${__ALL_HEADERS})
set(SOURCES ${__ALL_PROTOCOLS} ${__ALL_OPTIONAL_FILES} ${__ALL_SOURCES}) 

# COMPILER EXECUTION
add_executable(${TARGET} ${SOURCES} ${HEADERS})
add_definitions(-DVERSION="${VERSION_STRING}")
add_definitions (${GTK3_CFLAGS_OTHER})
target_link_libraries(${TARGET} ${MATH_LIBRARY})
target_link_libraries(${TARGET} ${GTK3_LIBRARIES})
target_link_libraries(${TARGET} ${GLIB2_LIBRARIES})
target_link_libraries(${TARGET} ${GOBJECT_LIBRARIES})
target_link_libraries(${TARGET} ${WAYLAND_CLIENT_LIBRARIES})
target_link_libraries(${TARGET} ${THREADS_LIBRARY})
target_link_libraries(${TARGET} ${DL_LIBRARY})
target_link_libraries(${TARGET} ${GIO_UNIX_LIBRARIES})

################
# INSTALLATION #
################

if(RUN_IN_PLACE)
  set(__SYSTEM_BINARY_PATH ${PROJECT_SOURCE_DIR}/bin)
else()
  include(GNUInstallDirs)
  set(__SYSTEM_SHARE_PATH "${CMAKE_INSTALL_PREFIX}/share/${TARGET}")
  set(__SYSTEM_MANUAL_PATH "${CMAKE_INSTALL_PREFIX}/share/man")
  set(__SYSTEM_HEADER_PATH "${CMAKE_INSTALL_PREFIX}/include/${TARGET}")
  set(__SYSTEM_BINARY_PATH "${CMAKE_INSTALL_PREFIX}/bin")
endif()

if(NOT RUN_IN_PLACE)
  install(FILES ${__MANUAL_PATH}/wofi.1 DESTINATION ${__SYSTEM_MANUAL_PATH}/man1)
  install(FILES ${__MANUAL_PATH}/wofi.3 DESTINATION ${__SYSTEM_MANUAL_PATH}/man3)
  install(FILES ${__MANUAL_PATH}/wofi.5 DESTINATION ${__SYSTEM_MANUAL_PATH}/man5)
  install(FILES ${__MANUAL_PATH}/wofi.7 DESTINATION ${__SYSTEM_MANUAL_PATH}/man7)
  install(FILES ${__MANUAL_PATH}/wofi-api.3 DESTINATION ${__SYSTEM_MANUAL_PATH}/man3)
  install(FILES ${__MANUAL_PATH}/wofi-config.3 DESTINATION ${__SYSTEM_MANUAL_PATH}/man3)
  install(FILES ${__MANUAL_PATH}/wofi-keys.7 DESTINATION ${__SYSTEM_MANUAL_PATH}/man7)
  install(FILES ${__MANUAL_PATH}/wofi-map.3 DESTINATION ${__SYSTEM_MANUAL_PATH}/man3)
  install(FILES ${__MANUAL_PATH}/wofi-utils.3 DESTINATION ${__SYSTEM_MANUAL_PATH}/man3)
  install(FILES ${__MANUAL_PATH}/wofi-widget-builder.3 DESTINATION ${__SYSTEM_MANUAL_PATH}/man3)
  install(FILES ${__HEADER_PATH}/map.h DESTINATION ${__SYSTEM_HEADER_PATH})
  install(FILES ${__HEADER_PATH}/utils_g.h DESTINATION ${__SYSTEM_HEADER_PATH})
  install(FILES ${__HEADER_PATH}/utils.h DESTINATION ${__SYSTEM_HEADER_PATH})
  install(FILES ${__HEADER_PATH}/widget_builder_api.h DESTINATION ${__SYSTEM_HEADER_PATH})
  install(FILES ${__HEADER_PATH}/wofi_api.h DESTINATION ${__SYSTEM_HEADER_PATH})
endif()

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/wofi DESTINATION ${__SYSTEM_BINARY_PATH} PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)

