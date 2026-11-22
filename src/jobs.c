/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "jobs.h"

#include <string.h>

#include "clipboard.h"

/** @brief A pending configuration write. */
typedef struct SaveJob {
    bool pending;
    Config config;
    wchar_t path[MAX_PATH];
} SaveJob;

/** @brief A pending clipboard copy. */
typedef struct CopyJob {
    bool pending;
    HWND owner;
    Canvas canvas;
    RECT region;
    bool opaque;
    wchar_t path[MAX_PATH];
} CopyJob;

/** @brief Shared queue state; the lock protects everything below it. */
static CRITICAL_SECTION g_lock;
static volatile LONG g_lockReady;
static SaveJob g_save;
static CopyJob g_copy;
static bool g_running;
static HANDLE g_thread;

/**
 * @brief Initializes the lock on first use.
 *
 * Every caller runs on the message-loop thread, so the lazy initialization needs no further guard.
 */
static void ensure_lock(void)
{
    if (!g_lockReady) {
        InitializeCriticalSection(&g_lock);
        g_lockReady = 1;
    }
}

/**
 * @brief Worker thread body: drains both queues and exits when they are empty.
 * @param parameter Unused.
 * @return Always 0.
 */
static DWORD WINAPI worker_main(LPVOID parameter)
{
    (void)parameter;
    for (;;) {
        EnterCriticalSection(&g_lock);
        SaveJob *save = NULL;
        CopyJob copy;
        bool haveCopy = false;
        static SaveJob local;
        if (g_save.pending) {
            local = g_save;
            g_save.pending = false;
            save = &local;
        } else if (g_copy.pending) {
            copy = g_copy;
            g_copy.pending = false;
            memset(&g_copy.canvas, 0, sizeof g_copy.canvas);
            haveCopy = true;
        } else {
            g_running = false;
            LeaveCriticalSection(&g_lock);
            return 0;
        }
        LeaveCriticalSection(&g_lock);
        if (save) {
            config_save(&save->config, save->path);
        } else if (haveCopy) {
            if (copy.path[0]) {
                clipboard_save_region(&copy.canvas, copy.region, copy.opaque, copy.path);
            } else {
                clipboard_copy_region(copy.owner, &copy.canvas, copy.region, copy.opaque);
            }
            canvas_destroy(&copy.canvas);
        }
    }
}

/**
 * @brief Starts the worker when it is not running. Must be called with the lock held.
 * @return true when a worker is running afterwards.
 */
static bool start_worker_locked(void)
{
    if (g_running) {
        return true;
    }
    if (g_thread) {
        CloseHandle(g_thread);
        g_thread = NULL;
    }
    g_running = true;
    g_thread = CreateThread(NULL, 0, worker_main, NULL, 0, NULL);
    if (!g_thread) {
        g_running = false;
    }
    return g_running;
}

bool jobs_save_config(const Config *config, const wchar_t *path)
{
    size_t length = wcslen(path);
    if (length >= MAX_PATH) {
        return false;
    }
    ensure_lock();
    EnterCriticalSection(&g_lock);
    g_save.config = *config;
    memcpy(g_save.path, path, (length + 1) * sizeof(wchar_t));
    g_save.pending = true;
    bool started = start_worker_locked();
    if (!started) {
        g_save.pending = false;
    }
    LeaveCriticalSection(&g_lock);
    return started;
}

/**
 * @brief Queues an export of a canvas region, to the clipboard or to a file.
 * @param owner Clipboard owner window.
 * @param canvas Rendered canvas; ownership moves to the job on success.
 * @param region Region to export.
 * @param opaqueBackdrop true when the region is fully opaque.
 * @param path Destination file, or NULL for the clipboard.
 * @return true when queued.
 */
static bool queue_export(HWND owner, Canvas *canvas, RECT region, bool opaqueBackdrop, const wchar_t *path)
{
    ensure_lock();
    EnterCriticalSection(&g_lock);
    if (g_copy.pending) {
        canvas_destroy(&g_copy.canvas);
    }
    g_copy.path[0] = 0;
    if (path) {
        wcsncpy(g_copy.path, path, MAX_PATH - 1);
        g_copy.path[MAX_PATH - 1] = 0;
    }
    g_copy.owner = owner;
    g_copy.canvas = *canvas;
    g_copy.region = region;
    g_copy.opaque = opaqueBackdrop;
    g_copy.pending = true;
    bool started = start_worker_locked();
    if (started) {
        memset(canvas, 0, sizeof *canvas);
    } else {
        g_copy.pending = false;
        memset(&g_copy.canvas, 0, sizeof g_copy.canvas);
    }
    LeaveCriticalSection(&g_lock);
    return started;
}

bool jobs_copy_region(HWND owner, Canvas *canvas, RECT region, bool opaqueBackdrop)
{
    return queue_export(owner, canvas, region, opaqueBackdrop, NULL);
}

bool jobs_save_region(Canvas *canvas, RECT region, bool opaqueBackdrop, const wchar_t *path)
{
    return queue_export(NULL, canvas, region, opaqueBackdrop, path);
}

void jobs_shutdown(DWORD timeoutMs)
{
    if (!g_lockReady) {
        return;
    }
    HANDLE thread = g_thread;
    if (thread) {
        WaitForSingleObject(thread, timeoutMs);
    }
    EnterCriticalSection(&g_lock);
    if (g_copy.pending) {
        canvas_destroy(&g_copy.canvas);
        g_copy.pending = false;
    }
    g_save.pending = false;
    LeaveCriticalSection(&g_lock);
    if (g_thread) {
        CloseHandle(g_thread);
        g_thread = NULL;
    }
}
