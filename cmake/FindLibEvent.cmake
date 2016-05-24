# - Try to find the LibEvent config processing library
# Once done this will define
#
# LIBEVENT_FOUND - System has LibEvent
# LIBEVENT_INCLUDE_DIR - the LibEvent include directory
# LIBEVENT_LIBRARIES 0 The libraries needed to use LibEvent

FIND_PATH(LIBEVENT_INCLUDE_DIRS NAMES event.h)
FIND_LIBRARY(LIBEVENT_LIBRARY NAMES event)
FIND_LIBRARY(LIBEVENT_CORE_LIBRARY NAMES event_core)
FIND_LIBRARY(LIBEVENT_PTHREADS_LIBRARY NAMES event_pthreads)
FIND_LIBRARY(LIBEVENT_EXTRA_LIBRARY NAMES event_extra)
FIND_LIBRARY(LIBEVENT_OPENSSL_LIBRARY NAMES event_openssl)

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(LibEvent DEFAULT_MSG LIBEVENT_LIBRARY LIBEVENT_INCLUDE_DIRS)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(LibEventPthreads DEFAULT_MSG LIBEVENT_PTHREADS_LIBRARY LIBEVENT_INCLUDE_DIRS)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(LibEventCore DEFAULT_MSG LIBEVENT_CORE_LIBRARY LIBEVENT_INCLUDE_DIRS)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(LibEventExtra DEFAULT_MSG LIBEVENT_EXTRA_LIBRARY LIBEVENT_INCLUDE_DIRS)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(LibEventOpenssl DEFAULT_MSG LIBEVENT_OPENSSL_LIBRARY LIBEVENT_INCLUDE_DIRS)

MARK_AS_ADVANCED(LIBEVENT_INCLUDE_DIRS LIBEVENT_LIBRARY LIBEVENT_PTHREADS_LIBRARY LIBEVENT_OPENSSL_LIBRARY LIBEVENT_CORE_LIBRARY LIBEVENT_EXTRA_LIBRARY)
