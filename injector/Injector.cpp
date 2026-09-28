// Tiny LoadLibrary injector for Minecraft Bedrock and Java.
// usage: TarInjector.exe [--bedrock | --java] [path\to\dll]
//   no edition flag: bedrock if it's running, otherwise java
//   no dll: TarClient.dll / TarClientJava.dll next to the exe if it's there, otherwise the copy
//   built into the exe gets unpacked to %APPDATA%\TarClient\bin and used
#include <windows.h>

#include <aclapi.h>
#include <sddl.h>
#include <tlhelp32.h>

#include <conio.h>

#include <cstdio>
#include <cstring>
#include <string>

namespace {
    DWORD findProcess(const wchar_t* name) {
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snap == INVALID_HANDLE_VALUE) return 0;
        PROCESSENTRY32W entry{ sizeof(entry) };
        DWORD pid = 0;
        for (BOOL ok = Process32FirstW(snap, &entry); ok; ok = Process32NextW(snap, &entry))
            if (_wcsicmp(entry.szExeFile, name) == 0) {
                pid = entry.th32ProcessID;
                break;
            }
        CloseHandle(snap);
        return pid;
    }

    // java runs inside java.exe / javaw.exe, and other java apps can be open too,
    // so look for the game's window instead (GLFW30 = 1.13+, LWJGL = 1.8 - 1.12)
    DWORD findJavaMinecraft() {
        struct Search { DWORD pid = 0; } search;
        EnumWindows(
            [](HWND hwnd, LPARAM param) -> BOOL {
                wchar_t cls[64] = {};
                GetClassNameW(hwnd, cls, 64);
                if (wcscmp(cls, L"GLFW30") != 0 && wcscmp(cls, L"LWJGL") != 0) return TRUE;
                wchar_t title[256] = {};
                GetWindowTextW(hwnd, title, 256);
                if (!wcsstr(title, L"Minecraft")) return TRUE;
                GetWindowThreadProcessId(hwnd, &reinterpret_cast<Search*>(param)->pid);
                return FALSE;
            },
            reinterpret_cast<LPARAM>(&search));
        return search.pid;
    }

    // uwp builds of the game run in an app container and can only load files that
    // "ALL APPLICATION PACKAGES" is allowed to read, so give it read access to the dll
    void allowAppContainer(const std::wstring& path) {
        PSID sid = nullptr;
        if (!ConvertStringSidToSidW(L"S-1-15-2-1", &sid)) return;

        PACL oldAcl = nullptr, newAcl = nullptr;
        PSECURITY_DESCRIPTOR sd = nullptr;
        if (GetNamedSecurityInfoW(path.c_str(), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, nullptr, nullptr, &oldAcl,
                                  nullptr, &sd) == ERROR_SUCCESS) {
            EXPLICIT_ACCESSW access{};
            access.grfAccessPermissions = GENERIC_READ | GENERIC_EXECUTE;
            access.grfAccessMode = GRANT_ACCESS;
            access.grfInheritance = NO_INHERITANCE;
            access.Trustee.TrusteeForm = TRUSTEE_IS_SID;
            access.Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
            access.Trustee.ptstrName = static_cast<LPWSTR>(sid);
            if (SetEntriesInAclW(1, &access, oldAcl, &newAcl) == ERROR_SUCCESS)
                SetNamedSecurityInfoW(const_cast<LPWSTR>(path.c_str()), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION,
                                      nullptr, nullptr, newAcl, nullptr);
        }
        if (newAcl) LocalFree(newAcl);
        if (sd) LocalFree(sd);
        LocalFree(sid);
    }

    // ---------------------------------------------------------------- console look

    bool vt = false; // colors only work if the console understands ansi escape codes (windows 10+)

    namespace Color {
        const char* accent() { return vt ? "\x1b[38;2;30;215;130m" : ""; }
        const char* deep() { return vt ? "\x1b[38;2;18;150;92m" : ""; }
        const char* dim() { return vt ? "\x1b[38;2;115;128;122m" : ""; }
        const char* text() { return vt ? "\x1b[38;2;232;234;236m" : ""; }
        const char* red() { return vt ? "\x1b[38;2;235;80;80m" : ""; }
        const char* reset() { return vt ? "\x1b[0m" : ""; }
    }

    void setupConsole() {
        SetConsoleTitleW(L"Tar Client Injector");
        SetConsoleOutputCP(CP_UTF8);
        HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD mode = 0;
        if (GetConsoleMode(out, &mode) && SetConsoleMode(out, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING)) vt = true;
        if (vt) std::printf("\x1b[?25l"); // hide the blinking cursor
    }

    void restoreConsole() {
        if (vt) std::printf("\x1b[?25h%s", Color::reset());
    }

    void banner() {
        static const char* lines[] = {
            "  ████████╗ █████╗ ██████╗ ",
            "  ╚══██╔══╝██╔══██╗██╔══██╗",
            "     ██║   ███████║██████╔╝",
            "     ██║   ██╔══██║██╔══██╗",
            "     ██║   ██║  ██║██║  ██║",
            "     ╚═╝   ╚═╝  ╚═╝╚═╝  ╚═╝",
        };
        std::printf("\n");
        for (int i = 0; i < 6; i++) {
            // fade from bright mint at the top to a deeper green at the bottom
            if (vt) std::printf("\x1b[38;2;%d;%d;%dm", 40 - i * 4, 225 - i * 14, 140 - i * 8);
            std::printf("%s%s\n", lines[i], Color::reset());
        }
        std::printf("\n  %sclient injector%s  %sv1.0.0 · bedrock + java%s\n", Color::accent(), Color::reset(), Color::dim(), Color::reset());
        std::printf("  %s─────────────────────────────────────%s\n\n", Color::deep(), Color::reset());
    }

    void ok(const char* message) { std::printf("  %s✔%s  %s%s%s\n", Color::accent(), Color::reset(), Color::text(), message, Color::reset()); }
    void info(const char* label, const char* value) {
        std::printf("  %s›%s  %s%-10s%s %s%s%s\n", Color::deep(), Color::reset(), Color::dim(), label, Color::reset(), Color::text(), value, Color::reset());
    }

    void waitForKey() {
        std::printf("\n  %spress any key to close%s", Color::dim(), Color::reset());
        _getch();
        std::printf("\n");
    }

    // ---------------------------------------------------------------- built in dlls

    constexpr int BEDROCK_DLL = 101, JAVA_DLL = 102; // ids from the generated embed .rc

    bool sameContents(const std::wstring& path, const void* data, DWORD size) {
        HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                               OPEN_EXISTING, 0, nullptr);
        if (f == INVALID_HANDLE_VALUE) return false;
        std::string existing(size, '\0');
        DWORD read = 0;
        bool same = GetFileSize(f, nullptr) == size && ReadFile(f, existing.data(), size, &read, nullptr) && read == size &&
                    memcmp(existing.data(), data, size) == 0;
        CloseHandle(f);
        return same;
    }

    bool writeFile(const std::wstring& path, const void* data, DWORD size) {
        HANDLE f = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (f == INVALID_HANDLE_VALUE) return false;
        DWORD written = 0;
        bool ok = WriteFile(f, data, size, &written, nullptr) && written == size;
        CloseHandle(f);
        return ok;
    }

    // unpacks a dll from inside the exe, returns an empty string if that fails
    std::wstring unpackDll(int id, const wchar_t* name) {
        HRSRC res = FindResourceW(nullptr, MAKEINTRESOURCEW(id), MAKEINTRESOURCEW(10)); // 10 = RT_RCDATA
        HGLOBAL loaded = res ? LoadResource(nullptr, res) : nullptr;
        const void* data = loaded ? LockResource(loaded) : nullptr;
        DWORD size = res ? SizeofResource(nullptr, res) : 0;
        if (!data || !size) return L"";

        wchar_t appdata[MAX_PATH];
        if (!GetEnvironmentVariableW(L"APPDATA", appdata, MAX_PATH)) return L"";
        std::wstring dir = std::wstring(appdata) + L"\\TarClient";
        CreateDirectoryW(dir.c_str(), nullptr);
        dir += L"\\bin";
        CreateDirectoryW(dir.c_str(), nullptr);

        std::wstring path = dir + L"\\" + name;
        if (sameContents(path, data, size)) return path; // already unpacked (maybe even loaded in the game right now)
        if (writeFile(path, data, size)) return path;

        // an older version is still loaded in a game and locked, use a new name instead
        std::wstring stem(name);
        stem = stem.substr(0, stem.find_last_of(L'.'));
        path = dir + L"\\" + stem + L"_" + std::to_wstring(GetTickCount()) + L".dll";
        return writeFile(path, data, size) ? path : L"";
    }

    int fail(const char* message) {
        DWORD err = GetLastError();
        std::printf("  %s✘  %s%s", Color::red(), message, Color::reset());
        if (err) std::printf("  %s(error %lu)%s", Color::dim(), err, Color::reset());
        std::printf("\n");
        waitForKey();
        restoreConsole();
        return 1;
    }

    std::string narrow(const std::wstring& w) {
        int size = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
        std::string out(size > 0 ? size - 1 : 0, '\0');
        if (size > 0) WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, out.data(), size, nullptr, nullptr);
        return out;
    }
}

int wmain(int argc, wchar_t** argv) {
    setupConsole();
    banner();

    enum class Edition { Auto, Bedrock, Java } edition = Edition::Auto;
    std::wstring dll;
    for (int i = 1; i < argc; i++) {
        if (_wcsicmp(argv[i], L"--java") == 0) edition = Edition::Java;
        else if (_wcsicmp(argv[i], L"--bedrock") == 0) edition = Edition::Bedrock;
        else dll = argv[i];
    }

    // wait for the game instead of giving up, esc to quit
    static const char* spinner[] = { "⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏" };
    DWORD pid = 0;
    for (int frame = 0;; frame++) {
        if (edition != Edition::Java && (pid = findProcess(L"Minecraft.Windows.exe"))) edition = Edition::Bedrock;
        else if (edition != Edition::Bedrock && (pid = findJavaMinecraft())) edition = Edition::Java;
        if (pid) break;

        std::printf("\r  %s%s%s  %swaiting for Minecraft to open%s  %s(esc to quit)%s   ", Color::accent(), spinner[frame % 10],
                    Color::reset(), Color::text(), Color::reset(), Color::dim(), Color::reset());
        std::fflush(stdout);
        if (_kbhit() && _getch() == 27) {
            std::printf("\n");
            restoreConsole();
            return 0;
        }
        Sleep(90);
    }
    std::printf("\r%80s\r", "");

    bool java = edition == Edition::Java;
    char pidText[32];
    std::snprintf(pidText, sizeof(pidText), "%lu", pid);
    info("edition", java ? "Java" : "Bedrock");
    info("process", pidText);

    const wchar_t* dllName = java ? L"TarClientJava.dll" : L"TarClient.dll";
    bool builtIn = false;
    if (dll.empty()) {
        // a dll sitting next to the exe wins (handy while developing), otherwise use the built in one
        wchar_t exe[MAX_PATH];
        GetModuleFileNameW(nullptr, exe, MAX_PATH);
        dll = exe;
        dll = dll.substr(0, dll.find_last_of(L"\\/") + 1) + dllName;
        if (GetFileAttributesW(dll.c_str()) == INVALID_FILE_ATTRIBUTES) {
            dll = unpackDll(java ? JAVA_DLL : BEDROCK_DLL, dllName);
            builtIn = true;
            if (dll.empty()) return fail("couldn't unpack the built in dll (antivirus might be blocking it)");
        }
    }

    wchar_t full[MAX_PATH];
    if (!GetFullPathNameW(dll.c_str(), MAX_PATH, full, nullptr) || GetFileAttributesW(full) == INVALID_FILE_ATTRIBUTES) {
        SetLastError(0);
        info("dll", narrow(dll).c_str());
        std::printf("\n");
        return fail("couldn't find the dll");
    }
    dll = full;
    std::string shown = narrow(dll.substr(dll.find_last_of(L"\\/") + 1));
    if (builtIn) shown += " (built in)";
    info("dll", shown.c_str());
    std::printf("\n");

    if (!java) allowAppContainer(dll);

    HANDLE process = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!process) return fail("couldn't open the game, try running as admin");
    ok("opened the game");

    size_t bytes = (dll.size() + 1) * sizeof(wchar_t);
    void* remote = VirtualAllocEx(process, nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remote || !WriteProcessMemory(process, remote, dll.c_str(), bytes, nullptr)) {
        CloseHandle(process);
        return fail("couldn't write into the game");
    }
    ok("wrote the dll path");

    auto loadLibrary = reinterpret_cast<LPTHREAD_START_ROUTINE>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW"));
    HANDLE thread = CreateRemoteThread(process, nullptr, 0, loadLibrary, remote, 0, nullptr);
    if (!thread) {
        VirtualFreeEx(process, remote, 0, MEM_RELEASE);
        CloseHandle(process);
        return fail("couldn't start the loader thread");
    }

    WaitForSingleObject(thread, 10000);
    DWORD result = 0;
    GetExitCodeThread(thread, &result);
    CloseHandle(thread);
    VirtualFreeEx(process, remote, 0, MEM_RELEASE);
    CloseHandle(process);

    if (!result) {
        SetLastError(0);
        return fail("the dll failed to load (antivirus or anticheat might be blocking it)");
    }
    ok("loaded Tar Client");

    std::printf("\n  %s▸ injected!%s %spress %sInsert%s%s in game to open the menu%s\n\n", Color::accent(), Color::reset(),
                Color::text(), Color::accent(), Color::reset(), Color::text(), Color::reset());
    for (int i = 3; i > 0; i--) {
        std::printf("\r  %sclosing in %d...%s", Color::dim(), i, Color::reset());
        std::fflush(stdout);
        Sleep(1000);
    }
    std::printf("\n");
    restoreConsole();
    return 0;
}
