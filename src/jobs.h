/**
 * @file jobs.h
 * @brief Runs slow work (configuration writes, PNG encoding) off the message-loop thread.
 *
 * The low-level mouse hook is serviced by the thread that owns the message loop. Work that can take
 * tens of milliseconds there makes the system drop the hook and lags the pointer of the whole
 * machine, so disk writes and image encoding run on a short-lived worker thread instead. The worker
 * exists only while there is something to do.
 */
#ifndef INK_JOBS_H
#define INK_JOBS_H

#include <stdbool.h>
#include <windows.h>

#include "canvas.h"
#include "config.h"

/**
 * @brief Queues a configuration write; a newer request replaces one that has not started yet.
 * @param config Configuration to write; copied before the call returns.
 * @param path Destination file path; copied before the call returns.
 * @return true when the request was queued.
 */
bool jobs_save_config(const Config *config, const wchar_t *path);

/**
 * @brief Queues copying a canvas region to the clipboard as PNG.
 * @param owner Window that owns the clipboard while it is open.
 * @param canvas Fully rendered canvas; on success its ownership moves to the job and it is zeroed.
 * @param region Region to copy.
 * @param opaqueBackdrop true when the region is fully opaque.
 * @return true when queued; on false the caller still owns the canvas.
 */
bool jobs_copy_region(HWND owner, Canvas *canvas, RECT region, bool opaqueBackdrop);

/**
 * @brief Waits for running work to finish and discards work that has not started.
 * @param timeoutMs Longest time to wait for the worker.
 */
void jobs_shutdown(DWORD timeoutMs);

#endif
