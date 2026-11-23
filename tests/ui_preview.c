/* SPDX-License-Identifier: GPL-3.0-or-later */
/**
 * @file ui_preview.c
 * @brief Visual harness: shows the three islands over a dark background and applies their events.
 *
 * Options: --tool N, --sections HEX, --autohide MS (islands start as handles and open on click),
 * --pin HEX (bit 0 toolbar, 1 properties, 2 history pinned open), --idle (drawing off, toolbar only),
 * --lock, --whiteboard, --noundo, --demo (cycles state every second), --quit MS.
 * Without --autohide every island stays expanded.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include "text.h"
#include "ui.h"
#include "ui_internal.h"

#define PREVIEW_CLASS L"PenPaneUiPreviewBackground"
#define DEMO_TIMER 1
#define QUIT_TIMER 2

/** @brief Harness state shared with the window procedure and the UI handler. */
typedef struct Preview {
    Ui *ui;
    UiModel model;
    UiSettings settings;
    bool demo;
    int step;
    int quitMs;
} Preview;

static Preview g_preview;
static char g_dump_path[MAX_PATH];
static bool g_test_drag;

/**
 * @brief Paints a faux whiteboard scene so shadows and transparency are visible against content.
 * @param window Background window.
 */
static void paint_background(HWND window)
{
    PAINTSTRUCT paint;
    HDC dc = BeginPaint(window, &paint);
    RECT client;
    GetClientRect(window, &client);
    HBRUSH brush = CreateSolidBrush(RGB(0x12, 0x12, 0x12));
    HGDIOBJ hollow = GetStockObject(NULL_BRUSH);
    FillRect(dc, &client, brush);
    DeleteObject(brush);
    SelectObject(dc, hollow);
    HPEN pen = CreatePen(PS_SOLID, 3, RGB(0xff, 0x83, 0x83));
    HGDIOBJ previous = SelectObject(dc, pen);
    Rectangle(dc, 300, 300, 700, 520);
    DeleteObject(SelectObject(dc, previous));
    pen = CreatePen(PS_SOLID, 3, RGB(0x56, 0xa2, 0xe8));
    previous = SelectObject(dc, pen);
    Ellipse(dc, 800, 260, 1100, 480);
    DeleteObject(SelectObject(dc, previous));
    EndPaint(window, &paint);
}

/**
 * @brief Applies one UI event to the preview model, mimicking the application.
 * @param user Preview pointer.
 * @param event Event reported by the UI.
 */
static void on_event(void *user, const UiEvent *event)
{
    Preview *preview = user;
    UiModel *model = &preview->model;
    switch (event->kind) {
    case UI_EVENT_SELECT_TOOL: model->tool = event->tool; break;    case UI_EVENT_SET_STROKE: model->style.stroke = STROKE_PALETTE[event->index]; break;
    case UI_EVENT_SET_FILL: model->style.fill = FILL_PALETTE[event->index]; break;
    case UI_EVENT_SET_WIDTH:
        model->style.pressure = event->index >= WIDTH_PRESET_COUNT;
        if (!model->style.pressure) {
            model->style.width = WIDTH_PRESETS[event->index];
        }
        break;
    case UI_EVENT_SET_STROKE_STYLE: model->style.strokeStyle = (StrokeStyle)event->index; break;
    case UI_EVENT_SET_OPACITY: model->style.opacity = event->value; break;
    case UI_EVENT_ACTION:
        if (event->action == ACTION_TOGGLE_TOOL_LOCK) {
            model->toolLocked = !model->toolLocked;
        } else if (event->action == ACTION_TOGGLE_MODE) {
            model->whiteboard = !model->whiteboard;
        } else if (event->action == ACTION_TOGGLE_OVERLAY) {
            model->drawing = !model->drawing;
            ui_set_visible(preview->ui, true, model->drawing);
        } else if (event->action == ACTION_ASK_CLEAR) {
            model->dialogs |= UI_DIALOG_BIT(PANEL_CONFIRM);
        } else if (event->action == ACTION_OPEN_SETTINGS) {
            model->dialogs ^= UI_DIALOG_BIT(PANEL_SETTINGS);
        } else if (event->action == ACTION_CLEAR) {
            model->dialogs &= ~UI_DIALOG_BIT(PANEL_CONFIRM);
        } else if (event->action == ACTION_TOGGLE_DRAWINGS) {
            model->drawingsHidden = !model->drawingsHidden;
        } else if (event->action == ACTION_EMERGENCY_EXIT) {
            PostQuitMessage(0);
        }
        break;
    case UI_EVENT_PANEL_MOVED:
        printf("moved panel %d to %d,%d\n", (int)event->panel, event->x, event->y);
        fflush(stdout);
        break;
    case UI_EVENT_PANEL_PINNED:
        preview->settings.pinned[event->panel] = event->index != 0;
        printf("panel %d pinned %d\n", (int)event->panel, event->index);
        fflush(stdout);
        break;
    case UI_EVENT_SET_FONT: model->style.face = (TextFace)event->index; break;
    case UI_EVENT_SET_LASER_MODE: model->laserPointer = event->index != 0; break;
    case UI_EVENT_CLOSE_DIALOG: model->dialogs &= ~UI_DIALOG_BIT(event->panel); break;
    case UI_EVENT_SET_ARROWHEADS: model->style.arrowHeads = (ArrowHeads)event->index; break;
    case UI_EVENT_SET_DEBUG_LOG: model->debugLog = event->index != 0; break;
    case UI_EVENT_RESET_OPTIONS:
    case UI_EVENT_RESET_POSITIONS:
    case UI_EVENT_SEND_KEY:
    case UI_EVENT_TOOL_SHORTCUT:
    case UI_EVENT_TOOL_SHORTCUT_CLEAR:
    case UI_EVENT_TOOL_SHORTCUT_KEY:
    case UI_EVENT_SETTINGS_TAB:
    case UI_EVENT_EDIT_HEX:
    case UI_EVENT_EDIT_END:
        break;
    case UI_EVENT_SET_AUTO_HIDE: model->autoHide = event->index != 0; break;
    case UI_EVENT_SET_EXCLUDE_CAPTURE: model->excludeCapture = event->index != 0; break;
    case UI_EVENT_TOOL_VISIBLE:
        model->hiddenTools = event->index != 0 ? model->hiddenTools & ~(1u << event->tool)
                                               : model->hiddenTools | (1u << event->tool);
        break;
    case UI_EVENT_TOOL_MOVE:
        for (int i = 0; i < TOOL_COUNT; i++) {
            int other = i + event->index;
            if (model->toolOrder[i] == event->tool && other >= 0 && other < TOOL_COUNT) {
                Tool swap = model->toolOrder[i];
                model->toolOrder[i] = model->toolOrder[other];
                model->toolOrder[other] = swap;
                break;
            }
        }
        break;
    case UI_EVENT_SET_STROKE_COLOR: model->style.stroke = event->color; break;
    case UI_EVENT_SET_PASTEL: model->pastel = event->index != 0; break;
    case UI_EVENT_OPEN_PICKER:
        model->dialogs ^= UI_DIALOG_BIT(PANEL_COLOR);
        break;
    case UI_EVENT_PICK_HUE:
    case UI_EVENT_PICK_SV:
        if (event->kind == UI_EVENT_PICK_HUE) {
            model->pickerHue = event->value;
        } else {
            model->pickerSat = event->value;
            model->pickerVal = event->extra;
        }
        model->style.stroke = color_from_hsv(model->pickerHue, model->pickerSat, model->pickerVal);
        break;
    case UI_EVENT_SLIDER_END:
        break;
    }
    ui_update(preview->ui, model);
}

/**
 * @brief Advances the scripted demo by one step.
 * @param preview Harness state.
 */
static void demo_step(Preview *preview)
{
    UiModel *model = &preview->model;
    preview->step++;
    model->tool = (Tool)(preview->step % TOOL_COUNT);
    model->style.stroke = STROKE_PALETTE[preview->step % STROKE_PALETTE_SIZE];
    model->style.fill = FILL_PALETTE[preview->step % FILL_PALETTE_SIZE];
    model->style.opacity = (float)(preview->step % 11) / 10.0f;
    model->canUndo = (preview->step & 1) != 0;
    model->canRedo = (preview->step & 2) != 0;
    ui_update(preview->ui, model);
}

/**
 * @brief Window procedure of the background window.
 * @param window Window handle.
 * @param message Message id.
 * @param wparam First parameter.
 * @param lparam Second parameter.
 * @return Message result.
 */
static LRESULT CALLBACK background_procedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message) {
    case WM_PAINT:
        paint_background(window);
        return 0;
    case WM_TIMER:
        if (wparam == DEMO_TIMER) {
            demo_step(&g_preview);
        } else if (wparam == QUIT_TIMER) {
            PostQuitMessage(0);
        }
        return 0;
    case WM_KEYDOWN:
        if (wparam == VK_ESCAPE) {
            PostQuitMessage(0);
        }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(window, message, wparam, lparam);
    }
}

/**
 * @brief Parses the command line into the harness state.
 * @param preview State to fill.
 * @param argc Argument count.
 * @param argv Wide argument vector.
 * @param idle Receives true for --idle.
 */
static void parse_arguments(Preview *preview, int argc, wchar_t **argv, bool *idle)
{
    preview->model.learningTool = -1;
    for (int i = 1; i < argc; i++) {
        if (wcscmp(argv[i], L"--tool") == 0 && i + 1 < argc) {
            preview->model.tool = (Tool)_wtoi(argv[++i]);
        } else if (wcscmp(argv[i], L"--sections") == 0 && i + 1 < argc) {
            preview->model.sections = (unsigned)wcstoul(argv[++i], NULL, 16);
        } else if (wcscmp(argv[i], L"--autohide") == 0 && i + 1 < argc) {
            preview->settings.autoHide = true;
            preview->settings.autoHideMs = _wtoi(argv[++i]);
        } else if (wcscmp(argv[i], L"--pin") == 0 && i + 1 < argc) {
            unsigned mask = (unsigned)wcstoul(argv[++i], NULL, 16);
            for (int panel = 0; panel < PANEL_COUNT; panel++) {
                preview->settings.pinned[panel] = (mask >> panel & 1u) != 0;
            }
        } else if (wcscmp(argv[i], L"--quit") == 0 && i + 1 < argc) {
            preview->quitMs = _wtoi(argv[++i]);
        } else if (wcscmp(argv[i], L"--idle") == 0) {
            *idle = true;
        } else if (wcscmp(argv[i], L"--lock") == 0) {
            preview->model.toolLocked = true;
        } else if (wcscmp(argv[i], L"--whiteboard") == 0) {
            preview->model.whiteboard = true;
        } else if (wcscmp(argv[i], L"--noundo") == 0) {
            preview->model.canUndo = false;
            preview->model.canRedo = false;
        } else if (wcscmp(argv[i], L"--demo") == 0) {
            preview->demo = true;
        } else if (wcscmp(argv[i], L"--exclude") == 0) {
            preview->settings.excludeFromCapture = true;
        } else if (wcscmp(argv[i], L"--dialog") == 0 && i + 1 < argc) {
            ++i;
            preview->model.dialogs |= UI_DIALOG_BIT(wcscmp(argv[i], L"settings") == 0 ? PANEL_SETTINGS
                                                        : wcscmp(argv[i], L"color") == 0 ? PANEL_COLOR
                                                                                         : PANEL_CONFIRM);
        } else if (wcscmp(argv[i], L"--pastel") == 0) {
            preview->model.pastel = true;
        } else if (wcscmp(argv[i], L"--custom") == 0 && i + 1 < argc) {
            static const Color SAMPLE[8] = {0xFFE63946, 0xFF2A9D8F, 0xFFE9C46A, 0xFF8338EC,
                                            0xFFFF006E, 0xFF06D6A0, 0xFF118AB2, 0xFFFB8500};
            int count = _wtoi(argv[++i]);
            preview->model.customCount = count > 8 ? 8 : count;
            memcpy(preview->model.custom, SAMPLE, sizeof SAMPLE);
        } else if (wcscmp(argv[i], L"--hidden") == 0 && i + 1 < argc) {
            preview->model.hiddenTools = (unsigned)wcstoul(argv[++i], NULL, 16);
        } else if (wcscmp(argv[i], L"--tab") == 0 && i + 1 < argc) {
            preview->model.settingsTab = _wtoi(argv[++i]);
        } else if (wcscmp(argv[i], L"--shortcuts") == 0) {
            wcscpy(preview->model.toolShortcut[TOOL_PEN], L"F15");
            wcscpy(preview->model.toolShortcut[TOOL_ERASER], L"Ctrl+Alt+F9 x2");
            preview->model.learningTool = TOOL_TEXT;
        } else if (wcscmp(argv[i], L"--hide-drawings") == 0) {
            preview->model.drawingsHidden = true;
        } else if (wcscmp(argv[i], L"--test-drag") == 0) {
            g_test_drag = true;
        } else if (wcscmp(argv[i], L"--dump") == 0 && i + 1 < argc) {
            WideCharToMultiByte(CP_ACP, 0, argv[++i], -1, g_dump_path, (int)sizeof g_dump_path, NULL, NULL);
        }
    }
}

/**
 * @brief Entry point of the preview program: parses the options, shows the islands and runs the message loop.
 * @param instance Module instance of the executable.
 * @param previous Unused.
 * @param command Unused; the command line is read with CommandLineToArgvW.
 * @param show Unused.
 * @return Process exit code.
 */
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous, PWSTR command, int show)
{
    (void)previous;
    (void)command;
    (void)show;
    SetProcessDPIAware();

    Preview *preview = &g_preview;
    preview->model.style = style_default();
    preview->model.style.opacity = 1.0f;
    preview->model.tool = TOOL_PEN;
    preview->model.sections = 0x3F;
    preview->model.canUndo = true;
    preview->model.canRedo = false;
    preview->model.drawing = true;
    preview->model.autoHide = true;
    preview->model.excludeCapture = true;
    tool_order_default(preview->model.toolOrder);
    bool idle = false;
    int argc;
    wchar_t **argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    parse_arguments(preview, argc, argv, &idle);
    LocalFree(argv);
    preview->model.drawing = !idle;
    color_to_hsv(preview->model.style.stroke, &preview->model.pickerHue, &preview->model.pickerSat,
                 &preview->model.pickerVal);

    WNDCLASSEXW windowClass = {0};
    windowClass.cbSize = sizeof windowClass;
    windowClass.lpfnWndProc = background_procedure;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursorW(NULL, IDC_ARROW);
    windowClass.lpszClassName = PREVIEW_CLASS;
    RegisterClassExW(&windowClass);

    RECT work;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    HWND background = CreateWindowExW(0, PREVIEW_CLASS, L"UI preview", WS_POPUP | WS_VISIBLE, 0, 0,
                                      GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), NULL,
                                      NULL, instance, NULL);
    preview->ui = ui_create(instance, on_event, preview, &preview->settings);
    if (!background || !preview->ui) {
        return 1;
    }
    ui_place_defaults(preview->ui, work, false);
    ui_update(preview->ui, &preview->model);
    ui_set_visible(preview->ui, true, preview->model.drawing);
    if (g_dump_path[0]) {
        if (g_test_drag) {
            static const float SQUARE[4][2] = {{0.85f, 0.25f}, {0.9f, 0.5f}, {1.4f, 1.3f}, {1.4f, 1.3f}};
            static const float HUE[3][2] = {{0.7f, 0.5f}, {0.3f, 3.0f}, {0.3f, 3.0f}};
            bool a = ui_test_drag(preview->ui, PANEL_COLOR, ITEM_SV, SQUARE, 4);
            printf("square drag ok=%d sat=%.2f val=%.2f\n", a, preview->model.pickerSat, preview->model.pickerVal);
            bool b = ui_test_drag(preview->ui, PANEL_COLOR, ITEM_HUE, HUE, 3);
            printf("hue drag ok=%d hue=%.2f stroke=%06X\n", b, preview->model.pickerHue,
                   (unsigned)(preview->model.style.stroke & 0xFFFFFFu));
            fflush(stdout);
        }
        bool ok = ui_dump(preview->ui, g_dump_path);
        ui_destroy(preview->ui);
        text_shutdown();
        return ok ? 0 : 1;
    }
    if (preview->demo) {
        SetTimer(background, DEMO_TIMER, 1000, NULL);
    }
    if (preview->quitMs > 0) {
        SetTimer(background, QUIT_TIMER, (UINT)preview->quitMs, NULL);
    }

    MSG message;
    while (GetMessageW(&message, NULL, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    ui_destroy(preview->ui);
    text_shutdown();
    return 0;
}
