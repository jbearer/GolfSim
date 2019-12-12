/**
 * \file os.h
 * \brief Detection of OS-specific facilities.
 *
 * Include this file in any source file which is intended to have platform-
 * dependent behavior. To gain access to non-ANSI library features (such as
 * POSIX-specific libraries) include this file before including any system
 * headers.
 *
 * Do not include this file from a header file. With this convention, any file
 * which does not directly include "os.h" will be portable.
 */

#ifndef GOLF_OS_H
#define GOLF_OS_H

#if defined(GOLF_OS_LINUX) || defined(GOLF_OS_MAC)
# define GOLF_OS_POSIX
# define _POSIX_C_SOURCE 200809L
#endif

#endif
