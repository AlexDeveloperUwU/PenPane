// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file setup.c
 * @brief Per-user installer and uninstaller for PenPane in one executable, with a wizard window.
 *
 * The installer carries PenPane.exe and the license as resources. It installs under
 * %LOCALAPPDATA%\Programs because PenPane keeps its settings and log next to the executable, so the
 * folder must stay writable without administrator rights. The installed copy of this program is
 * Uninstall.exe.
 *
 * Switches: /S (silent, no wizard), /D=<folder> (install folder), /uninstall (remove). /cleanup is internal.
 */
#define COBJMACROS
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

#include "version.h"

#define ICON_ID 1
#define PAYLOAD_ID 100
#define LICENSE_ID 101
#define MESSAGE_CLASS L"PenPaneMessageWindow"
#define UNINSTALL_KEY L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\PenPane"
#define RUN_KEY L"Software\\Microsoft\\Windows\\CurrentVersion\\Run"
#define TITLE L"PenPane Setup"
#define WIZARD_CLASS L"PenPaneSetupWizard"

#define UNCHANGED (-1)

enum { PAGE_WELCOME, PAGE_LICENSE, PAGE_OPTIONS, PAGE_PROGRESS, PAGE_FINISH, PAGE_COUNT };

#define ON(page) (1u << (page))
#define ALL_PAGES 0x1Fu
#define ID_BACK 1001
#define ID_BROWSE 1002
#define ID_PATH 1003

#define MAX_ITEMS 40

typedef struct {
    HWND window;
    unsigned pages;
} Item;

static bool silent;
static HINSTANCE instance;

static struct {
    HWND window;
    HFONT font;
    HFONT titleFont;
    int dpi;
    int page;
    bool upgrade;
    wchar_t folder[MAX_PATH];
    Item items[MAX_ITEMS];
    int count;
    HWND headerBand, headerTitle, headerSub, headerIcon;
    HWND back, next, cancel;
    HWND path, desktop, autostart, launch, status, progress, welcomeText, finishText, optionsNote;
} ui;

static const wchar_t *const HEADER_TITLE[PAGE_COUNT] = {L"Welcome to PenPane Setup", L"License Agreement",
                                                        L"Install Options", L"Installing", L"Setup Complete"};
static const wchar_t *const HEADER_SUB[PAGE_COUNT] = {
    L"Setup will guide you through the installation.",
    L"Please review the license terms before installing PenPane.",
    L"Choose where to install PenPane and what to set up.", L"Please wait while PenPane is installed.",
    L"PenPane is ready to use."};

/**
 * @brief Shows a message unless running silently.
 * @param flags MessageBox flags.
 * @return The MessageBox result, or IDYES when silent.
 */
static int say(const wchar_t *text, UINT flags)
{
    if (silent) {
        return IDYES;
    }
    return MessageBoxW(ui.window, text, TITLE, flags);
}

/**
 * @brief Joins two path parts with a backslash.
 * @return false when the result does not fit.
 */
static bool join(wchar_t *out, size_t size, const wchar_t *left, const wchar_t *right)
{
    int written = _snwprintf(out, size, L"%ls\\%ls", left, right);
    if (written < 0 || (size_t)written >= size) {
        out[0] = L'\0';
        return false;
    }
    return true;
}

/**
 * @brief Resolves a known folder to a path.
 * @return false when the folder cannot be resolved or does not fit.
 */
static bool known_folder(REFKNOWNFOLDERID id, wchar_t *out, size_t size)
{
    PWSTR path = NULL;
    if (FAILED(SHGetKnownFolderPath(id, 0, NULL, &path))) {
        return false;
    }
    bool fits = wcslen(path) < size;
    if (fits) {
        wcscpy(out, path);
    }
    CoTaskMemFree(path);
    return fits;
}

/**
 * @brief Builds the path of PenPane.lnk inside a known folder.
 */
static bool link_path(REFKNOWNFOLDERID id, wchar_t *out, size_t size)
{
    wchar_t folder[MAX_PATH];
    return known_folder(id, folder, ARRAYSIZE(folder)) && join(out, size, folder, L"PenPane.lnk");
}

/**
 * @brief Asks a running PenPane to quit and ends it when it does not.
 */
static void stop_running(void)
{
    HWND window = FindWindowW(MESSAGE_CLASS, NULL);
    if (!window) {
        return;
    }
    DWORD process = 0;
    GetWindowThreadProcessId(window, &process);
    PostMessageW(window, WM_CLOSE, 0, 0);
    for (int i = 0; i < 50 && IsWindow(window); i++) {
        Sleep(100);
    }
    if (IsWindow(window) && process != 0) {
        HANDLE handle = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, process);
        if (handle) {
            TerminateProcess(handle, 0);
            WaitForSingleObject(handle, 3000);
            CloseHandle(handle);
        }
    }
}

/**
 * @brief Writes the embedded PenPane.exe to a path, replacing an existing file.
 * @return false on failure.
 */
static bool write_payload(const wchar_t *target)
{
    HRSRC resource = FindResourceW(NULL, MAKEINTRESOURCEW(PAYLOAD_ID), RT_RCDATA);
    HGLOBAL loaded = resource ? LoadResource(NULL, resource) : NULL;
    const void *data = loaded ? LockResource(loaded) : NULL;
    DWORD size = resource ? SizeofResource(NULL, resource) : 0;
    if (!data || size == 0) {
        return false;
    }
    wchar_t temporary[MAX_PATH];
    if (_snwprintf(temporary, ARRAYSIZE(temporary), L"%ls.new", target) < 0) {
        return false;
    }
    HANDLE file = CreateFileW(temporary, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }
    DWORD written = 0;
    bool ok = WriteFile(file, data, size, &written, NULL) && written == size;
    CloseHandle(file);
    for (int i = 0; ok && i < 30; i++) {
        if (MoveFileExW(temporary, target, MOVEFILE_REPLACE_EXISTING)) {
            return true;
        }
        Sleep(100);
    }
    DeleteFileW(temporary);
    return false;
}

/**
 * @brief Creates or removes PenPane.lnk in a known folder.
 * @param id Folder that holds the shortcut (Start menu programs or desktop).
 * @param target Executable the shortcut starts; NULL removes the shortcut.
 * @param folder Install folder, used as the working directory.
 */
static void set_shortcut(REFKNOWNFOLDERID id, const wchar_t *target, const wchar_t *folder)
{
    wchar_t link[MAX_PATH];
    if (!link_path(id, link, ARRAYSIZE(link))) {
        return;
    }
    if (!target) {
        DeleteFileW(link);
        return;
    }
    IShellLinkW *shell = NULL;
    if (FAILED(CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, &IID_IShellLinkW,
                                (void **)&shell))) {
        return;
    }
    IShellLinkW_SetPath(shell, target);
    IShellLinkW_SetWorkingDirectory(shell, folder);
    IShellLinkW_SetDescription(shell, L"PenPane screen annotation overlay");
    IPersistFile *persist = NULL;
    if (SUCCEEDED(IShellLinkW_QueryInterface(shell, &IID_IPersistFile, (void **)&persist))) {
        IPersistFile_Save(persist, link, TRUE);
        IPersistFile_Release(persist);
    }
    IShellLinkW_Release(shell);
}

static void set_text(HKEY key, const wchar_t *name, const wchar_t *value)
{
    RegSetValueExW(key, name, 0, REG_SZ, (const BYTE *)value, (DWORD)((wcslen(value) + 1) * sizeof(wchar_t)));
}

/**
 * @brief Sets or clears the start-with-Windows entry that PenPane's own tray menu toggles.
 * @param program Path of the installed PenPane.exe; NULL clears the entry.
 */
static void set_autostart(const wchar_t *program)
{
    HKEY key;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, RUN_KEY, 0, NULL, 0, KEY_SET_VALUE, NULL, &key, NULL) !=
        ERROR_SUCCESS) {
        return;
    }
    if (program) {
        wchar_t command[MAX_PATH + 4];
        _snwprintf(command, ARRAYSIZE(command), L"\"%ls\"", program);
        set_text(key, L"PenPane", command);
    } else {
        RegDeleteValueW(key, L"PenPane");
    }
    RegCloseKey(key);
}

/**
 * @brief Registers the program in Settings > Apps.
 */
static void register_uninstall(const wchar_t *folder, const wchar_t *program, const wchar_t *uninstaller,
                               DWORD sizeKb)
{
    HKEY key;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, UNINSTALL_KEY, 0, NULL, 0, KEY_SET_VALUE, NULL, &key, NULL) !=
        ERROR_SUCCESS) {
        return;
    }
    wchar_t command[MAX_PATH + 32];
    set_text(key, L"DisplayName", L"PenPane");
    set_text(key, L"DisplayVersion", PENPANE_VERSION_TEXT);
    set_text(key, L"Publisher", L"PenPane contributors");
    set_text(key, L"DisplayIcon", program);
    set_text(key, L"InstallLocation", folder);
    _snwprintf(command, ARRAYSIZE(command), L"\"%ls\" /uninstall", uninstaller);
    set_text(key, L"UninstallString", command);
    _snwprintf(command, ARRAYSIZE(command), L"\"%ls\" /uninstall /S", uninstaller);
    set_text(key, L"QuietUninstallString", command);
    DWORD one = 1;
    RegSetValueExW(key, L"NoModify", 0, REG_DWORD, (const BYTE *)&one, sizeof one);
    RegSetValueExW(key, L"NoRepair", 0, REG_DWORD, (const BYTE *)&one, sizeof one);
    RegSetValueExW(key, L"EstimatedSize", 0, REG_DWORD, (const BYTE *)&sizeKb, sizeof sizeKb);
    RegCloseKey(key);
}

/**
 * @brief Reports installation progress to the wizard; does nothing without a wizard window.
 */
static void step(int percent, const wchar_t *text)
{
    if (!ui.window) {
        return;
    }
    SetWindowTextW(ui.status, text);
    SendMessageW(ui.progress, PBM_SETPOS, (WPARAM)percent, 0);
    UpdateWindow(ui.window);
}

/**
 * @brief Installs or upgrades PenPane into a folder.
 * @param folder Install folder.
 * @param desktop 1 or 0 to add or remove the desktop shortcut, UNCHANGED to leave it.
 * @param autostart 1 or 0 to enable or disable start with Windows, UNCHANGED to leave it.
 * @return NULL on success, otherwise a message for the user.
 */
static const wchar_t *perform_install(const wchar_t *folder, int desktop, int autostart)
{
    wchar_t program[MAX_PATH];
    wchar_t uninstaller[MAX_PATH];
    wchar_t self[MAX_PATH];
    if (!join(program, ARRAYSIZE(program), folder, L"PenPane.exe") ||
        !join(uninstaller, ARRAYSIZE(uninstaller), folder, L"Uninstall.exe") ||
        GetModuleFileNameW(NULL, self, ARRAYSIZE(self)) >= ARRAYSIZE(self)) {
        return L"The install folder path is too long.";
    }
    step(5, L"Closing PenPane if it is running...");
    stop_running();
    step(20, L"Creating the install folder...");
    if (SHCreateDirectoryExW(NULL, folder, NULL) != ERROR_SUCCESS &&
        GetFileAttributesW(folder) == INVALID_FILE_ATTRIBUTES) {
        return L"The install folder could not be created.";
    }
    step(35, L"Copying PenPane...");
    if (!write_payload(program)) {
        return L"PenPane.exe could not be written. Close it and try again.";
    }
    step(60, L"Writing the uninstaller...");
    if (!CopyFileW(self, uninstaller, FALSE)) {
        return L"The uninstaller could not be written.";
    }
    step(75, L"Creating shortcuts...");
    set_shortcut(&FOLDERID_Programs, program, folder);
    if (desktop != UNCHANGED) {
        set_shortcut(&FOLDERID_Desktop, desktop ? program : NULL, folder);
    }
    if (autostart != UNCHANGED) {
        set_autostart(autostart ? program : NULL);
    }
    step(90, L"Registering PenPane with Windows...");
    WIN32_FILE_ATTRIBUTE_DATA info;
    DWORD sizeKb = GetFileAttributesExW(program, GetFileExInfoStandard, &info) ? info.nFileSizeLow / 1024 : 0;
    register_uninstall(folder, program, uninstaller, sizeKb);
    step(100, L"Done.");
    return NULL;
}

/**
 * @brief Picks the install folder: an existing installation first, else the per-user default.
 * @return false when no folder can be resolved.
 */
static bool default_folder(wchar_t *out, size_t size)
{
    DWORD bytes = (DWORD)(size * sizeof(wchar_t));
    if (RegGetValueW(HKEY_CURRENT_USER, UNINSTALL_KEY, L"InstallLocation", RRF_RT_REG_SZ, NULL, out,
                     &bytes) == ERROR_SUCCESS &&
        out[0] != L'\0') {
        ui.upgrade = true;
        return true;
    }
    wchar_t base[MAX_PATH];
    return known_folder(&FOLDERID_LocalAppData, base, ARRAYSIZE(base)) &&
           join(out, size, base, L"Programs\\PenPane");
}

/**
 * @brief Removes the registrations and program files, then hands the final folder removal to a temporary
 * copy.
 *
 * Only files the installer created are deleted; the folder is removed only when nothing else is left in it.
 * @return Process exit code.
 */
static int uninstall(void)
{
    wchar_t self[MAX_PATH];
    wchar_t folder[MAX_PATH];
    if (GetModuleFileNameW(NULL, self, ARRAYSIZE(self)) >= ARRAYSIZE(self)) {
        return 1;
    }
    wcscpy(folder, self);
    wchar_t *slash = wcsrchr(folder, L'\\');
    if (!slash) {
        return 1;
    }
    *slash = L'\0';
    if (say(L"Remove PenPane and its settings from this computer?", MB_YESNO | MB_ICONQUESTION) != IDYES) {
        return 2;
    }
    stop_running();
    static const wchar_t *const FILES[] = {L"PenPane.exe", L"PenPane.exe.new", L"PenPane.ini",
                                           L"PenPane.log", L"PenPane.log.1",   L"ink.ini"};
    for (size_t i = 0; i < ARRAYSIZE(FILES); i++) {
        wchar_t path[MAX_PATH];
        if (join(path, ARRAYSIZE(path), folder, FILES[i])) {
            DeleteFileW(path);
        }
    }
    set_shortcut(&FOLDERID_Programs, NULL, NULL);
    set_shortcut(&FOLDERID_Desktop, NULL, NULL);
    set_autostart(NULL);
    RegDeleteKeyW(HKEY_CURRENT_USER, UNINSTALL_KEY);

    wchar_t temp[MAX_PATH];
    wchar_t copy[MAX_PATH];
    wchar_t name[48];
    _snwprintf(name, ARRAYSIZE(name), L"PenPaneUninstall%lu.exe", GetCurrentProcessId());
    if (GetTempPathW(ARRAYSIZE(temp), temp) == 0 || !join(copy, ARRAYSIZE(copy), temp, name) ||
        !CopyFileW(self, copy, FALSE)) {
        return 1;
    }
    wchar_t command[2 * MAX_PATH + 32];
    _snwprintf(command, ARRAYSIZE(command), L"\"%ls\" /cleanup \"%ls\"", copy, folder);
    STARTUPINFOW startup = {.cb = sizeof startup};
    PROCESS_INFORMATION process;
    if (!CreateProcessW(copy, command, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, temp, &startup, &process)) {
        return 1;
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    say(L"PenPane was removed.", MB_ICONINFORMATION);
    return 0;
}

/**
 * @brief Runs from a temporary copy: deletes Uninstall.exe and the folder, then deletes itself.
 * @return Process exit code.
 */
static int cleanup(const wchar_t *folder)
{
    wchar_t uninstaller[MAX_PATH];
    if (join(uninstaller, ARRAYSIZE(uninstaller), folder, L"Uninstall.exe")) {
        for (int i = 0; i < 100 && !DeleteFileW(uninstaller) &&
                        GetFileAttributesW(uninstaller) != INVALID_FILE_ATTRIBUTES;
             i++) {
            Sleep(100);
        }
    }
    RemoveDirectoryW(folder);
    wchar_t self[MAX_PATH];
    if (GetModuleFileNameW(NULL, self, ARRAYSIZE(self)) < ARRAYSIZE(self)) {
        wchar_t command[MAX_PATH + 64];
        _snwprintf(command, ARRAYSIZE(command), L"cmd.exe /c ping -n 3 127.0.0.1 >nul & del /f /q \"%ls\"",
                   self);
        STARTUPINFOW startup = {.cb = sizeof startup};
        PROCESS_INFORMATION process;
        if (CreateProcessW(NULL, command, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &startup,
                           &process)) {
            CloseHandle(process.hThread);
            CloseHandle(process.hProcess);
        }
    }
    return 0;
}

static int scale(int value)
{
    return MulDiv(value, ui.dpi, 96);
}

/**
 * @brief Creates a child control on the given pages (a bit mask of ON(page)).
 */
static HWND make(unsigned pages, const wchar_t *className, const wchar_t *text, DWORD style, DWORD exStyle,
                 int x, int y, int w, int h, UINT id)
{
    HWND control = CreateWindowExW(exStyle, className, text, WS_CHILD | style, scale(x), scale(y), scale(w),
                                   scale(h), ui.window, (HMENU)(UINT_PTR)id, instance, NULL);
    SendMessageW(control, WM_SETFONT, (WPARAM)ui.font, TRUE);
    if (ui.count < MAX_ITEMS) {
        ui.items[ui.count].window = control;
        ui.items[ui.count].pages = pages;
        ui.count++;
    }
    return control;
}

static HWND label(unsigned pages, const wchar_t *text, int x, int y, int w, int h)
{
    return make(pages, L"STATIC", text, SS_LEFT | SS_NOPREFIX, 0, x, y, w, h, 0);
}

static HWND checkbox(unsigned pages, const wchar_t *text, int x, int y, int w, UINT id)
{
    return make(pages, L"BUTTON", text, BS_AUTOCHECKBOX | WS_TABSTOP, 0, x, y, w, 22, id);
}

/**
 * @brief Loads the embedded license with Windows line breaks into the license box.
 */
static void load_license(HWND edit)
{
    HRSRC resource = FindResourceW(NULL, MAKEINTRESOURCEW(LICENSE_ID), RT_RCDATA);
    HGLOBAL loaded = resource ? LoadResource(NULL, resource) : NULL;
    const char *data = loaded ? LockResource(loaded) : NULL;
    int size = resource ? (int)SizeofResource(NULL, resource) : 0;
    if (!data || size == 0) {
        SetWindowTextW(edit,
                       L"The license text is not available. See https://www.gnu.org/licenses/gpl-3.0.html");
        return;
    }
    wchar_t *text = malloc(((size_t)size * 2 + 1) * sizeof(wchar_t));
    wchar_t *plain = malloc(((size_t)size + 1) * sizeof(wchar_t));
    if (text && plain) {
        int length = MultiByteToWideChar(CP_UTF8, 0, data, size, plain, size);
        size_t out = 0;
        for (int i = 0; i < length; i++) {
            if (plain[i] == L'\n' && (i == 0 || plain[i - 1] != L'\r')) {
                text[out++] = L'\r';
            }
            text[out++] = plain[i];
        }
        text[out] = L'\0';
        SetWindowTextW(edit, text);
    }
    free(text);
    free(plain);
}

static int CALLBACK browse_callback(HWND window, UINT message, LPARAM param, LPARAM data)
{
    (void)param;
    if (message == BFFM_INITIALIZED) {
        SendMessageW(window, BFFM_SETSELECTIONW, TRUE, data);
    }
    return 0;
}

/**
 * @brief Lets the user pick a folder and appends "PenPane" unless the chosen folder already has that name.
 */
static void browse(void)
{
    wchar_t current[MAX_PATH];
    GetWindowTextW(ui.path, current, ARRAYSIZE(current));
    BROWSEINFOW info = {.hwndOwner = ui.window,
                        .lpszTitle = L"Choose the folder to install PenPane into:",
                        .ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE,
                        .lpfn = browse_callback,
                        .lParam = (LPARAM)current};
    PIDLIST_ABSOLUTE chosen = SHBrowseForFolderW(&info);
    if (!chosen) {
        return;
    }
    wchar_t folder[MAX_PATH];
    if (SHGetPathFromIDListW(chosen, folder)) {
        const wchar_t *name = wcsrchr(folder, L'\\');
        name = name ? name + 1 : folder;
        wchar_t result[MAX_PATH];
        if (_wcsicmp(name, L"PenPane") == 0 || !join(result, ARRAYSIZE(result), folder, L"PenPane")) {
            wcscpy(result, folder);
        }
        SetWindowTextW(ui.path, result);
    }
    CoTaskMemFree(chosen);
}

/**
 * @brief Shows one wizard page: its controls, header text and button states.
 */
static void show_page(int page)
{
    ui.page = page;
    for (int i = 0; i < ui.count; i++) {
        ShowWindow(ui.items[i].window, (ui.items[i].pages & ON(page)) ? SW_SHOW : SW_HIDE);
    }
    SetWindowTextW(ui.headerTitle, HEADER_TITLE[page]);
    SetWindowTextW(ui.headerSub, HEADER_SUB[page]);
    static const wchar_t *const NEXT_TEXT[PAGE_COUNT] = {L"Next >", L"I Agree", L"Install", L"Install",
                                                         L"Finish"};
    SetWindowTextW(ui.next, NEXT_TEXT[page]);
    bool busy = page == PAGE_PROGRESS;
    bool done = page == PAGE_FINISH;
    EnableWindow(ui.back, page == PAGE_LICENSE || page == PAGE_OPTIONS);
    ShowWindow(ui.back, done ? SW_HIDE : SW_SHOW);
    ShowWindow(ui.cancel, done ? SW_HIDE : SW_SHOW);
    EnableWindow(ui.next, !busy);
    EnableWindow(ui.cancel, !busy);
    if (!busy) {
        SetFocus(ui.next);
    }
    InvalidateRect(ui.window, NULL, TRUE);
}

/**
 * @brief Checks the folder from the options page and runs the installation.
 */
static void start_install(void)
{
    GetWindowTextW(ui.path, ui.folder, ARRAYSIZE(ui.folder));
    size_t length = wcslen(ui.folder);
    while (length > 3 && ui.folder[length - 1] == L'\\') {
        ui.folder[--length] = L'\0';
    }
    bool absolute =
        (length > 2 && ui.folder[1] == L':' && ui.folder[2] == L'\\') || wcsncmp(ui.folder, L"\\\\", 2) == 0;
    if (!absolute) {
        say(L"Enter a full folder path, for example C:\\Users\\you\\PenPane.", MB_ICONWARNING);
        return;
    }
    int desktop = SendMessageW(ui.desktop, BM_GETCHECK, 0, 0) == BST_CHECKED;
    int autostart = SendMessageW(ui.autostart, BM_GETCHECK, 0, 0) == BST_CHECKED;
    show_page(PAGE_PROGRESS);
    UpdateWindow(ui.window);
    const wchar_t *error = perform_install(ui.folder, desktop, autostart);
    if (error) {
        say(error, MB_ICONERROR);
        show_page(PAGE_OPTIONS);
        return;
    }
    show_page(PAGE_FINISH);
}

static void finish(void)
{
    bool launch = SendMessageW(ui.launch, BM_GETCHECK, 0, 0) == BST_CHECKED;
    wchar_t program[MAX_PATH];
    if (launch && join(program, ARRAYSIZE(program), ui.folder, L"PenPane.exe")) {
        ShellExecuteW(NULL, L"open", program, NULL, ui.folder, SW_SHOWNORMAL);
    }
    DestroyWindow(ui.window);
}

static void cancel(void)
{
    if (ui.page == PAGE_PROGRESS) {
        return;
    }
    if (ui.page == PAGE_FINISH || MessageBoxW(ui.window, L"Setup is not finished. Exit anyway?", TITLE,
                                              MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) == IDYES) {
        DestroyWindow(ui.window);
    }
}

static LRESULT CALLBACK wizard_procedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message) {
    case WM_COMMAND:
        switch (LOWORD(wparam)) {
        case IDOK:
            if (!IsWindowEnabled(ui.next)) {
                return 0;
            }
            if (ui.page == PAGE_OPTIONS) {
                start_install();
            } else if (ui.page == PAGE_FINISH) {
                finish();
            } else {
                show_page(ui.page + 1);
            }
            return 0;
        case ID_BACK:
            if (ui.page == PAGE_LICENSE || ui.page == PAGE_OPTIONS) {
                show_page(ui.page - 1);
            }
            return 0;
        case ID_BROWSE:
            browse();
            return 0;
        case IDCANCEL:
            cancel();
            return 0;
        }
        break;
    case WM_CTLCOLORSTATIC: {
        HWND control = (HWND)lparam;
        if (control == ui.headerBand || control == ui.headerTitle || control == ui.headerSub ||
            control == ui.headerIcon) {
            SetBkColor((HDC)wparam, RGB(255, 255, 255));
            return (LRESULT)GetStockObject(WHITE_BRUSH);
        }
        break;
    }
    case WM_CLOSE:
        cancel();
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

static HFONT make_font(int dpi, bool large)
{
    NONCLIENTMETRICSW metrics = {.cbSize = sizeof metrics};
    SystemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS, sizeof metrics, &metrics, 0, dpi);
    LOGFONTW font = metrics.lfMessageFont;
    if (large) {
        font.lfHeight = font.lfHeight * 13 / 10;
        font.lfWeight = FW_SEMIBOLD;
    }
    return CreateFontIndirectW(&font);
}

/**
 * @brief Builds the wizard window with every page's controls.
 * @return false when the window cannot be created.
 */
static bool create_wizard(void)
{
    ui.dpi = (int)GetDpiForSystem();
    ui.font = make_font(ui.dpi, false);
    ui.titleFont = make_font(ui.dpi, true);

    HICON big =
        LoadImageW(instance, MAKEINTRESOURCEW(ICON_ID), IMAGE_ICON, GetSystemMetricsForDpi(SM_CXICON, ui.dpi),
                   GetSystemMetricsForDpi(SM_CYICON, ui.dpi), 0);
    HICON small = LoadImageW(instance, MAKEINTRESOURCEW(ICON_ID), IMAGE_ICON,
                             GetSystemMetricsForDpi(SM_CXSMICON, ui.dpi),
                             GetSystemMetricsForDpi(SM_CYSMICON, ui.dpi), 0);
    WNDCLASSEXW windowClass = {.cbSize = sizeof windowClass,
                               .lpfnWndProc = wizard_procedure,
                               .hInstance = instance,
                               .hIcon = big,
                               .hIconSm = small,
                               .hCursor = LoadCursorW(NULL, IDC_ARROW),
                               .hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1),
                               .lpszClassName = WIZARD_CLASS};
    RegisterClassExW(&windowClass);

    const DWORD style = WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    RECT frame = {0, 0, scale(540), scale(400)};
    AdjustWindowRectExForDpi(&frame, style, FALSE, 0, (UINT)ui.dpi);
    int width = frame.right - frame.left;
    int height = frame.bottom - frame.top;
    RECT area;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &area, 0);
    ui.window = CreateWindowExW(
        0, WIZARD_CLASS, TITLE, style, area.left + (area.right - area.left - width) / 2,
        area.top + (area.bottom - area.top - height) / 2, width, height, NULL, NULL, instance, NULL);
    if (!ui.window) {
        return false;
    }

    ui.headerBand = make(ALL_PAGES, L"STATIC", L"", SS_WHITERECT, 0, 0, 0, 540, 64, 0);
    ui.headerTitle = make(ALL_PAGES, L"STATIC", L"", SS_LEFT | SS_NOPREFIX, 0, 20, 12, 420, 22, 0);
    SendMessageW(ui.headerTitle, WM_SETFONT, (WPARAM)ui.titleFont, TRUE);
    ui.headerSub = make(ALL_PAGES, L"STATIC", L"", SS_LEFT | SS_NOPREFIX, 0, 20, 38, 420, 18, 0);
    ui.headerIcon = make(ALL_PAGES, L"STATIC", L"", SS_ICON, 0, 476, 8, 48, 48, 0);
    SendMessageW(ui.headerIcon, STM_SETICON,
                 (WPARAM)LoadImageW(instance, MAKEINTRESOURCEW(ICON_ID), IMAGE_ICON, scale(48), scale(48), 0),
                 0);
    make(ALL_PAGES, L"STATIC", L"", SS_ETCHEDHORZ, 0, 0, 64, 540, 2, 0);

    HWND icon = make(ON(PAGE_WELCOME), L"STATIC", L"", SS_ICON, 0, 24, 88, 64, 64, 0);
    SendMessageW(icon, STM_SETICON,
                 (WPARAM)LoadImageW(instance, MAKEINTRESOURCEW(ICON_ID), IMAGE_ICON, scale(64), scale(64), 0),
                 0);
    ui.welcomeText = label(ON(PAGE_WELCOME), L"", 108, 90, 416, 240);

    HWND license = make(ON(PAGE_LICENSE), L"EDIT", L"",
                        ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL | WS_TABSTOP,
                        WS_EX_CLIENTEDGE, 16, 78, 508, 228, 0);
    load_license(license);
    label(ON(PAGE_LICENSE), L"If you accept the terms of the license, click I Agree to continue.", 16, 316,
          508, 20);

    label(ON(PAGE_OPTIONS), L"Install PenPane to:", 16, 84, 508, 18);
    ui.path = make(ON(PAGE_OPTIONS), L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP, WS_EX_CLIENTEDGE, 16, 106,
                   408, 24, ID_PATH);
    make(ON(PAGE_OPTIONS), L"BUTTON", L"Browse...", BS_PUSHBUTTON | WS_TABSTOP, 0, 432, 105, 92, 26,
         ID_BROWSE);
    ui.desktop = checkbox(ON(PAGE_OPTIONS), L"Create a desktop shortcut", 16, 152, 508, 0);
    ui.autostart = checkbox(ON(PAGE_OPTIONS), L"Start PenPane when I sign in to Windows", 16, 178, 508, 0);
    ui.optionsNote = label(ON(PAGE_OPTIONS), L"", 16, 222, 508, 60);

    ui.status = label(ON(PAGE_PROGRESS), L"", 16, 132, 508, 20);
    ui.progress = make(ON(PAGE_PROGRESS), PROGRESS_CLASSW, L"", PBS_SMOOTH, 0, 16, 158, 508, 20, 0);
    SendMessageW(ui.progress, PBM_SETRANGE32, 0, 100);

    ui.finishText = label(ON(PAGE_FINISH), L"", 16, 88, 508, 110);
    ui.launch = checkbox(ON(PAGE_FINISH), L"Start PenPane now", 16, 204, 508, 0);
    SendMessageW(ui.launch, BM_SETCHECK, BST_CHECKED, 0);

    make(ALL_PAGES, L"STATIC", L"", SS_ETCHEDHORZ, 0, 0, 352, 540, 2, 0);
    ui.back = make(ALL_PAGES, L"BUTTON", L"< Back", BS_PUSHBUTTON | WS_TABSTOP, 0, 262, 362, 84, 26, ID_BACK);
    ui.next = make(ALL_PAGES, L"BUTTON", L"Next >", BS_DEFPUSHBUTTON | WS_TABSTOP, 0, 352, 362, 84, 26, IDOK);
    ui.cancel =
        make(ALL_PAGES, L"BUTTON", L"Cancel", BS_PUSHBUTTON | WS_TABSTOP, 0, 442, 362, 82, 26, IDCANCEL);

    wchar_t shortcut[MAX_PATH];
    bool desktopExists = link_path(&FOLDERID_Desktop, shortcut, ARRAYSIZE(shortcut)) &&
                         GetFileAttributesW(shortcut) != INVALID_FILE_ATTRIBUTES;
    bool autostartExists = RegGetValueW(HKEY_CURRENT_USER, RUN_KEY, L"PenPane", RRF_RT_REG_SZ, NULL, NULL,
                                        NULL) == ERROR_SUCCESS;
    SendMessageW(ui.desktop, BM_SETCHECK, desktopExists ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(ui.autostart, BM_SETCHECK, autostartExists ? BST_CHECKED : BST_UNCHECKED, 0);
    SetWindowTextW(ui.path, ui.folder);

    SetWindowTextW(
        ui.welcomeText,
        ui.upgrade
            ? L"PenPane is already installed on this computer. This wizard updates it to "
              L"version " PENPANE_VERSION_TEXT L".\r\n\r\nYour settings are kept. Click Next to continue."
            : L"This wizard installs PenPane " PENPANE_VERSION_TEXT L" on your computer.\r\n\r\nPenPane is a "
              L"small screen annotation overlay for "
              L"Windows: draw over "
              L"anything on screen for presentations, demos and teaching.\r\n\r\nIt installs for your "
              L"account only "
              L"and needs no administrator rights. Click Next to continue.");
    SetWindowTextW(ui.optionsNote, L"PenPane keeps its settings and log in this folder, so choose a folder "
                                   L"you can write to. The Start menu "
                                   L"shortcut is always created.");
    SetWindowTextW(
        ui.finishText,
        L"PenPane"
        L" " PENPANE_VERSION_TEXT L" has been installed.\r\n\r\nIt runs in the notification area. Click its "
        L"toolbar handle, then the drawing button, to start drawing over the screen. Press "
        L"Ctrl+Alt+Shift+Esc at any "
        L"time to quit.\r\n\r\nTo remove PenPane later, use Settings > Apps.");
    show_page(PAGE_WELCOME);
    ShowWindow(ui.window, SW_SHOW);
    return true;
}

/**
 * @brief Runs the wizard until it closes.
 * @return Process exit code.
 */
static int run_wizard(void)
{
    INITCOMMONCONTROLSEX controls = {.dwSize = sizeof controls, .dwICC = ICC_PROGRESS_CLASS};
    InitCommonControlsEx(&controls);
    if (!create_wizard()) {
        return 1;
    }
    MSG message;
    while (GetMessageW(&message, NULL, 0, 0) > 0) {
        if (!IsDialogMessageW(ui.window, &message)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
    return ui.page == PAGE_FINISH ? 0 : 2;
}

int WINAPI wWinMain(HINSTANCE self, HINSTANCE previous, PWSTR commandLine, int show)
{
    (void)previous;
    (void)commandLine;
    (void)show;
    instance = self;
    int count = 0;
    wchar_t **arguments = CommandLineToArgvW(GetCommandLineW(), &count);
    bool remove = false;
    const wchar_t *cleanupFolder = NULL;
    wchar_t folder[MAX_PATH] = L"";
    for (int i = 1; arguments && i < count; i++) {
        if (_wcsicmp(arguments[i], L"/S") == 0) {
            silent = true;
        } else if (_wcsicmp(arguments[i], L"/uninstall") == 0) {
            remove = true;
        } else if (_wcsicmp(arguments[i], L"/cleanup") == 0 && i + 1 < count) {
            cleanupFolder = arguments[++i];
        } else if (_wcsnicmp(arguments[i], L"/D=", 3) == 0 && wcslen(arguments[i] + 3) < ARRAYSIZE(folder)) {
            wcscpy(folder, arguments[i] + 3);
        }
    }
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    int result;
    if (cleanupFolder) {
        result = cleanup(cleanupFolder);
    } else if (remove) {
        result = uninstall();
    } else {
        wchar_t chosen[MAX_PATH];
        bool resolved = default_folder(chosen, ARRAYSIZE(chosen));
        if (folder[0] == L'\0' && !resolved) {
            say(L"The default install folder could not be resolved.", MB_ICONERROR);
            result = 1;
        } else {
            if (folder[0] == L'\0') {
                wcscpy(folder, chosen);
            }
            wcscpy(ui.folder, folder);
            if (silent) {
                result = perform_install(folder, UNCHANGED, UNCHANGED) ? 1 : 0;
            } else {
                result = run_wizard();
            }
        }
    }
    CoUninitialize();
    LocalFree(arguments);
    return result;
}
