/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "log.h"

#include <stdarg.h>
#include <stdio.h>
#include <windows.h>

#define LOG_LINE_CHARS 512
#define LOG_ROTATE_BYTES (1024 * 1024)

bool g_logEnabled;

static HANDLE g_file = INVALID_HANDLE_VALUE;
static wchar_t g_path[MAX_PATH];
static DWORD g_size;

/**
 * @brief Opens the log for appending, moving a large previous file aside first.
 * @return true when the file is open.
 */
static bool open_file(void)
{
    if (g_path[0] == L'\0') {
        return false;
    }
    WIN32_FILE_ATTRIBUTE_DATA info;
    if (GetFileAttributesExW(g_path, GetFileExInfoStandard, &info) && info.nFileSizeHigh == 0 &&
        info.nFileSizeLow >= LOG_ROTATE_BYTES) {
        wchar_t old[MAX_PATH + 4];
        swprintf(old, MAX_PATH + 4, L"%ls.1", g_path);
        MoveFileExW(g_path, old, MOVEFILE_REPLACE_EXISTING);
    }
    g_file = CreateFileW(g_path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS,
                         FILE_ATTRIBUTE_NORMAL, NULL);
    g_size = 0;
    if (g_file != INVALID_HANDLE_VALUE) {
        g_size = GetFileSize(g_file, NULL);
    }
    return g_file != INVALID_HANDLE_VALUE;
}

void log_write(const char *format, ...)
{
    if (g_file == INVALID_HANDLE_VALUE && !open_file()) {
        g_logEnabled = false;
        return;
    }
    char line[LOG_LINE_CHARS];
    SYSTEMTIME now;
    GetLocalTime(&now);
    int used = snprintf(line, sizeof line, "%02u:%02u:%02u.%03u ", now.wHour, now.wMinute, now.wSecond,
                        now.wMilliseconds);
    va_list arguments;
    va_start(arguments, format);
    int body = vsnprintf(line + used, sizeof line - (size_t)used - 2, format, arguments);
    va_end(arguments);
    if (body < 0) {
        body = 0;
    }
    if (used + body > (int)sizeof line - 3) {
        body = (int)sizeof line - 3 - used;
    }
    used += body;
    line[used++] = '\r';
    line[used++] = '\n';
    DWORD written = 0;
    WriteFile(g_file, line, (DWORD)used, &written, NULL);
    g_size += written;
    if (g_size >= LOG_ROTATE_BYTES) {
        CloseHandle(g_file);
        g_file = INVALID_HANDLE_VALUE;
        open_file();
    }
}

void log_set(bool enabled, const wchar_t *path)
{
    if (enabled == g_logEnabled && (!enabled || path == NULL)) {
        return;
    }
    if (!enabled) {
        log_write("log off");
        g_logEnabled = false;
        if (g_file != INVALID_HANDLE_VALUE) {
            CloseHandle(g_file);
            g_file = INVALID_HANDLE_VALUE;
        }
        return;
    }
    if (path) {
        wcsncpy(g_path, path, MAX_PATH - 1);
    }
    g_logEnabled = true;
}
