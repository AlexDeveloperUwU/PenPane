/* SPDX-License-Identifier: GPL-3.0-or-later */
/**
 * @file version.h
 * @brief Product name and version shared by the program and its resource script.
 *
 * Only preprocessor definitions appear here so the resource compiler can include the file.
 */
#ifndef PENPANE_VERSION_H
#define PENPANE_VERSION_H

#define PENPANE_VERSION_MAJOR 0
#define PENPANE_VERSION_MINOR 1
#define PENPANE_VERSION_PATCH 0
#define PENPANE_VERSION_STRING "0.1.0"
#define PENPANE_PRODUCT_NAME "PenPane"

#ifndef RC_INVOKED
#define PENPANE_WIDEN_INNER(text) L##text
#define PENPANE_WIDEN(text) PENPANE_WIDEN_INNER(text)
#define PENPANE_VERSION_TEXT PENPANE_WIDEN(PENPANE_VERSION_STRING)
#endif

#endif
