// ForgeShape for Windows (`DESKTOP-R1`): the Win32 shell around the
// platform-neutral desktop editor. It owns the window, turns Win32 messages
// into the editor's input samples, and answers the few questions only Windows
// can (which file, where settings live). Nothing in here is project truth.
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include "forgeshape_render_surface.h"  // windows.h, with the Vulkan Win32 define

#include <cctype>
#include <commdlg.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <shlobj.h>

#include <cstdio>
#include <string>
#include <vector>

#include "desktop_editor.h"

namespace {

std::wstring widen(const std::string& utf8) {
    if (utf8.empty()) return std::wstring();
    const int n = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    std::wstring out(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), &out[0], n);
    return out;
}

std::string narrow(const std::wstring& wide) {
    if (wide.empty()) return std::string();
    const int n = WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), nullptr,
                                      0, nullptr, nullptr);
    std::string out(static_cast<size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), &out[0], n, nullptr,
                        nullptr);
    return out;
}

std::wstring fileNameOf(const std::wstring& path) {
    const size_t slash = path.find_last_of(L"\\/");
    return slash == std::wstring::npos ? path : path.substr(slash + 1);
}

bool readWholeFile(const std::wstring& path, std::vector<uint8_t>* bytes) {
    FILE* file = _wfopen(path.c_str(), L"rb");
    if (file == nullptr) return false;
    bytes->clear();
    uint8_t buffer[65536];
    size_t n = 0;
    while ((n = fread(buffer, 1, sizeof(buffer), file)) > 0) bytes->insert(bytes->end(), buffer, buffer + n);
    fclose(file);
    return true;
}

class Win32Platform : public fsdesktop::DesktopPlatform {
public:
    HWND hwnd = nullptr;

    bool openFile(const char* filterName, const char* extension, std::vector<uint8_t>* bytes,
                  std::string* displayName) override {
        std::wstring filter = widen(filterName) + L" (*." + widen(extension) + L")";
        filter.push_back(L'\0');
        filter += L"*." + widen(extension);
        filter.push_back(L'\0');
        filter.push_back(L'\0');
        wchar_t path[MAX_PATH * 4] = L"";
        OPENFILENAMEW ofn{};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = hwnd;
        ofn.lpstrFilter = filter.c_str();
        ofn.lpstrFile = path;
        ofn.nMaxFile = MAX_PATH * 4;
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER;
        if (!GetOpenFileNameW(&ofn)) return false;
        if (!readWholeFile(path, bytes)) return false;
        *displayName = narrow(fileNameOf(path));
        lastPath = path;
        return true;
    }

    bool saveFile(const char* filterName, const char* extension, const std::string& suggested,
                  const std::vector<uint8_t>& bytes, std::string* displayName) override {
        std::wstring filter = widen(filterName) + L" (*." + widen(extension) + L")";
        filter.push_back(L'\0');
        filter += L"*." + widen(extension);
        filter.push_back(L'\0');
        filter.push_back(L'\0');
        wchar_t path[MAX_PATH * 4] = L"";
        const std::wstring suggestion = widen(suggested);
        wcsncpy_s(path, suggestion.c_str(), _TRUNCATE);
        const std::wstring ext = widen(extension);
        OPENFILENAMEW ofn{};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = hwnd;
        ofn.lpstrFilter = filter.c_str();
        ofn.lpstrFile = path;
        ofn.nMaxFile = MAX_PATH * 4;
        ofn.lpstrDefExt = ext.c_str();
        ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_EXPLORER;
        if (!GetSaveFileNameW(&ofn)) return false;
        // Written beside, then moved over the target, so a failed write never
        // destroys the file the user already had.
        const std::wstring temp = std::wstring(path) + L".writing";
        FILE* file = _wfopen(temp.c_str(), L"wb");
        if (file == nullptr) return false;
        const bool written = fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size();
        const bool flushed = fflush(file) == 0;
        fclose(file);
        if (!written || !flushed || !MoveFileExW(temp.c_str(), path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            DeleteFileW(temp.c_str());
            MessageBoxW(hwnd, L"The file could not be written.", L"ForgeShape", MB_ICONERROR);
            return false;
        }
        *displayName = narrow(fileNameOf(path));
        return true;
    }

    void setTitle(const std::string& utf8) override { SetWindowTextW(hwnd, widen(utf8).c_str()); }

    void requestQuit() override { PostMessageW(hwnd, WM_CLOSE, 0, 0); }

    std::string settingsDirectory() override {
        PWSTR roaming = nullptr;
        if (FAILED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &roaming))) return std::string();
        std::wstring dir = std::wstring(roaming) + L"\\ForgeShape";
        CoTaskMemFree(roaming);
        CreateDirectoryW(dir.c_str(), nullptr);
        return narrow(dir);
    }

    int askSaveChanges() override {
        const int answer = MessageBoxW(hwnd, L"Save the changes to this project?", L"ForgeShape",
                                       MB_YESNOCANCEL | MB_ICONQUESTION);
        return answer == IDYES ? 1 : answer == IDNO ? 2 : 0;
    }

    std::wstring lastPath;
};

struct App {
    Win32Platform platform;
    forgeshape::DesktopWindow window;
    fsdesktop::DesktopEditor editor;
    fsui::InputState input;
    bool running = true;
    bool started = false;
    bool minimized = false;
    UINT dpi = 96;
    wchar_t pendingSurrogate = 0;
};

App* g_app = nullptr;

void clearFrameInput(fsui::InputState* in) {
    for (int i = 0; i < 3; ++i) {
        in->mousePressed[i] = false;
        in->mouseReleased[i] = false;
    }
    in->wheel = 0.0f;
    in->text.clear();
    in->keys.clear();
}

void pointer(App& app, LPARAM lp) {
    app.input.mouseX = static_cast<float>(static_cast<short>(LOWORD(lp)));
    app.input.mouseY = static_cast<float>(static_cast<short>(HIWORD(lp)));
}

void button(App& app, HWND hwnd, int which, bool down, LPARAM lp) {
    pointer(app, lp);
    if (down) {
        app.input.mouseDown[which] = true;
        app.input.mousePressed[which] = true;
        SetCapture(hwnd);
    } else {
        app.input.mouseDown[which] = false;
        app.input.mouseReleased[which] = true;
        if (!app.input.mouseDown[0] && !app.input.mouseDown[1] && !app.input.mouseDown[2]) {
            ReleaseCapture();
        }
    }
}

LRESULT CALLBACK windowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    App* app = g_app;
    if (app == nullptr) return DefWindowProcW(hwnd, msg, wp, lp);
    switch (msg) {
        case WM_CLOSE:
            if (app->started && !app->editor.settleUnsavedChanges()) return 0;
            app->running = false;
            return 0;
        case WM_SIZE:
            app->minimized = wp == SIZE_MINIMIZED;
            if (app->started) app->editor.resized();
            return 0;
        case WM_DPICHANGED: {
            app->dpi = HIWORD(wp);
            const RECT* suggested = reinterpret_cast<const RECT*>(lp);
            SetWindowPos(hwnd, nullptr, suggested->left, suggested->top,
                         suggested->right - suggested->left, suggested->bottom - suggested->top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
            return 0;
        }
        case WM_MOUSEMOVE: pointer(*app, lp); return 0;
        case WM_LBUTTONDOWN: button(*app, hwnd, 0, true, lp); return 0;
        case WM_LBUTTONUP: button(*app, hwnd, 0, false, lp); return 0;
        case WM_RBUTTONDOWN: button(*app, hwnd, 1, true, lp); return 0;
        case WM_RBUTTONUP: button(*app, hwnd, 1, false, lp); return 0;
        case WM_MBUTTONDOWN: button(*app, hwnd, 2, true, lp); return 0;
        case WM_MBUTTONUP: button(*app, hwnd, 2, false, lp); return 0;
        case WM_MOUSEWHEEL: {
            POINT p{static_cast<short>(LOWORD(lp)), static_cast<short>(HIWORD(lp))};
            ScreenToClient(hwnd, &p);
            app->input.mouseX = static_cast<float>(p.x);
            app->input.mouseY = static_cast<float>(p.y);
            app->input.wheel += static_cast<float>(GET_WHEEL_DELTA_WPARAM(wp)) / WHEEL_DELTA;
            return 0;
        }
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN: {
            const bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            const bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            app->input.ctrl = ctrl;
            app->input.shift = shift;
            switch (wp) {
                case VK_RETURN: app->input.keys.push_back(fsui::Key::Enter); break;
                case VK_ESCAPE: app->input.keys.push_back(fsui::Key::Escape); break;
                case VK_BACK: app->input.keys.push_back(fsui::Key::Backspace); break;
                case VK_TAB: app->input.keys.push_back(fsui::Key::Tab); break;
                case VK_LEFT: app->input.keys.push_back(fsui::Key::Left); break;
                case VK_RIGHT: app->input.keys.push_back(fsui::Key::Right); break;
                case VK_HOME: app->input.keys.push_back(fsui::Key::Home); break;
                case VK_END: app->input.keys.push_back(fsui::Key::End); break;
                case VK_DELETE:
                    app->input.keys.push_back(fsui::Key::Delete);
                    app->editor.shortcut(0x7F, ctrl, shift);
                    break;
                default:
                    // Ctrl+letter acts, and Ctrl+1..8 the standard views.
                    if (ctrl && ((wp >= 'A' && wp <= 'Z') || (wp >= '1' && wp <= '8'))) {
                        app->editor.shortcut(static_cast<char>(wp), ctrl, shift);
                    }
                    break;
            }
            if (msg == WM_SYSKEYDOWN) return DefWindowProcW(hwnd, msg, wp, lp);
            return 0;
        }
        case WM_KEYUP:
            app->input.ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            app->input.shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            return 0;
        case WM_CHAR: {
            const wchar_t c = static_cast<wchar_t>(wp);
            if (c >= 0xD800 && c <= 0xDBFF) {
                app->pendingSurrogate = c;
                return 0;
            }
            char32_t cp = c;
            if (c >= 0xDC00 && c <= 0xDFFF && app->pendingSurrogate) {
                cp = 0x10000 + ((static_cast<char32_t>(app->pendingSurrogate) - 0xD800) << 10) + (c - 0xDC00);
                app->pendingSurrogate = 0;
            }
            if (cp >= 0x20 && cp != 0x7F && !app->input.ctrl) app->input.text.push_back(cp);
            return 0;
        }
        case WM_DROPFILES: {
            HDROP drop = reinterpret_cast<HDROP>(wp);
            wchar_t path[MAX_PATH * 4];
            if (DragQueryFileW(drop, 0, path, MAX_PATH * 4) > 0) {
                std::vector<uint8_t> bytes;
                const std::string name = narrow(fileNameOf(path));
                std::string lower = name;
                for (char& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                const bool project = lower.size() >= 6 && lower.compare(lower.size() - 6, 6, ".forge") == 0;
                if (!readWholeFile(path, &bytes)) {
                    // nothing to do: the file could not be read
                } else if (project) {
                    // A `.forge` REPLACES the project, so the open asks first.
                    if (app->editor.settleUnsavedChanges()) app->editor.openProjectBytes(bytes, name);
                } else {
                    // Any other file is a mesh to ADD (`INTERCHANGE-R1`):
                    // GLB, STL, OBJ, PLY or 3MF, sniffed from its bytes.
                    app->editor.importMeshBytes(bytes, name);
                }
            }
            DragFinish(drop);
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;  // the renderer paints every pixel
        default:
            return DefWindowProcW(hwnd, msg, wp, lp);
    }
}

void enablePerMonitorDpi() {
    // Asked for by name so the build runs on every Windows 10 the SDK targets.
    using SetContext = BOOL(WINAPI*)(HANDLE);
    if (HMODULE user = GetModuleHandleW(L"user32.dll")) {
        if (auto set = reinterpret_cast<SetContext>(GetProcAddress(user, "SetProcessDpiAwarenessContext"))) {
            set(reinterpret_cast<HANDLE>(-4));  // DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
        }
    }
}

std::vector<std::string> fontCandidates(bool bold) {
    wchar_t windowsDir[MAX_PATH] = L"C:\\Windows";
    GetWindowsDirectoryW(windowsDir, MAX_PATH);
    const std::string fonts = narrow(windowsDir) + "\\Fonts\\";
    if (bold) return {fonts + "segoeuib.ttf", fonts + "arialbd.ttf", fonts + "tahomabd.ttf"};
    return {fonts + "segoeui.ttf", fonts + "arial.ttf", fonts + "tahoma.ttf"};
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    enablePerMonitorDpi();
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    static App app;
    g_app = &app;

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc = windowProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(instance, L"FORGESHAPE_ICON");
    wc.hIconSm = wc.hIcon;
    wc.lpszClassName = L"ForgeShapeWindow";
    RegisterClassExW(&wc);

    const UINT systemDpi = GetDpiForSystem();
    const int w = MulDiv(1440, systemDpi, 96);
    const int h = MulDiv(900, systemDpi, 96);
    HWND hwnd = CreateWindowExW(WS_EX_APPWINDOW | WS_EX_ACCEPTFILES, wc.lpszClassName, L"ForgeShape",
                                WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, w, h, nullptr,
                                nullptr, instance, nullptr);
    if (hwnd == nullptr) return 1;
    app.platform.hwnd = hwnd;
    app.dpi = GetDpiForWindow(hwnd);
    // A dark title bar to sit with the dark workspace (Windows 10 2004+).
    const BOOL dark = TRUE;
    DwmSetWindowAttribute(hwnd, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &dark, sizeof(dark));
    ShowWindow(hwnd, show == SW_HIDE ? SW_SHOWNORMAL : show);
    UpdateWindow(hwnd);

    app.window.instance = instance;
    app.window.hwnd = hwnd;
    if (!app.editor.start(&app.window, &app.platform, fontCandidates(false), fontCandidates(true))) {
        MessageBoxW(hwnd,
                    L"ForgeShape could not start its Vulkan viewport.\n\n"
                    L"Please update the graphics driver: ForgeShape needs Vulkan 1.0.",
                    L"ForgeShape", MB_ICONERROR);
        DestroyWindow(hwnd);
        return 2;
    }
    app.started = true;

    // A .forge passed on the command line (a double-click, once the installer
    // has registered the file type) opens at once.
    int argc = 0;
    if (LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc)) {
        if (argc > 1) {
            std::vector<uint8_t> bytes;
            if (readWholeFile(argv[1], &bytes)) {
                app.editor.openProjectBytes(bytes, narrow(fileNameOf(argv[1])));
            }
        }
        LocalFree(argv);
    }

    MSG message{};
    while (app.running) {
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) app.running = false;
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        if (!app.running) break;
        if (app.minimized) {
            WaitMessage();
            continue;
        }
        RECT client{};
        GetClientRect(hwnd, &client);
        const int width = client.right - client.left;
        const int height = client.bottom - client.top;
        if (width <= 0 || height <= 0) {
            WaitMessage();
            continue;
        }
        // FIFO presentation paces this loop at the display's refresh rate.
        if (!app.editor.frame(width, height, static_cast<float>(app.dpi) / 96.0f, app.input)) {
            MessageBoxW(hwnd,
                        L"The graphics device stopped responding.\n\nPlease save your work and "
                        L"restart ForgeShape.",
                        L"ForgeShape", MB_ICONWARNING);
            app.running = false;
        }
        clearFrameInput(&app.input);
    }
    // The renderer lets go of the window before the window goes.
    app.editor.stop();
    DestroyWindow(hwnd);
    CoUninitialize();
    return 0;
}
