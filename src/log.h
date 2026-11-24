/* SPDX-License-Identifier: GPL-3.0-or-later */
/**
 * @file log.h
 * @brief Optional plain-text diagnostic log, off by default.
 *
 * While the log is off a LOG call costs one test of a flag. While it is on, each call formats one
 * line on the stack and writes it straight to the file: no heap, no buffering, nothing kept in
 * memory. The file is rotated once it passes a size limit, so it can never grow without bound.
 * Only events are logged (state changes, failures, notices), never pointer movement. The module is
 * for the UI thread only.
 */
#ifndef PENPANE_LOG_H
#define PENPANE_LOG_H

#include <stdbool.h>
#include <wchar.h>

/** @brief True while logging is on; read by the LOG macro. */
extern bool g_logEnabled;

/**
 * @brief Writes one timestamped line. Use through LOG.
 * @param format printf style format; %ls prints a wide string.
 */
void log_write(const char *format, ...);

/**
 * @brief Turns logging on or off.
 * @param enabled true to log.
 * @param path Log file, used when enabling; the previous file is kept as path + ".1" once it is large.
 */
void log_set(bool enabled, const wchar_t *path);

/** @brief Logs one line when logging is on. */
#define LOG(...)                                                                                             \
    do {                                                                                                     \
        if (g_logEnabled) {                                                                                  \
            log_write(__VA_ARGS__);                                                                          \
        }                                                                                                    \
    } while (0)

#endif
