// Tiny LoadLibrary injector for Minecraft Bedrock.
// usage: TarInjector.exe [path\to\TarClient.dll]   (defaults to TarClient.dll next to the exe)
#include <windows.h>

#include <aclapi.h>
#include <sddl.h>
#include <tlhelp32.h>

#include <cstdio>
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

    int fail(const char* message) {
        std::printf("[-] %s (error %lu)\n", message, GetLastError());
        std::printf("press enter to exit\n");
        std::getchar();
        return 1;
    }
}

int wmain(int argc, wchar_t** argv) {
    std::wstring dll;
    if (argc > 1) {
        dll = argv[1];
    } else {
        wchar_t exe[MAX_PATH];
        GetModuleFileNameW(nullptr, exe, MAX_PATH);
        dll = exe;
        dll = dll.substr(0, dll.find_last_of(L"\\/") + 1) + L"TarClient.dll";
    }

    wchar_t full[MAX_PATH];
    if (!GetFullPathNameW(dll.c_str(), MAX_PATH, full, nullptr) || GetFileAttributesW(full) == INVALID_FILE_ATTRIBUTES)
        return fail("couldn't find the dll");
    dll = full;

    DWORD pid = findProcess(L"Minecraft.Windows.exe");
    if (!pid) return fail("Minecraft isn't running, open the game first");
    std::printf("[+] found Minecraft (pid %lu)\n", pid);

    allowAppContainer(dll);

    HANDLE process = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!process) return fail("couldn't open the game process");

    size_t bytes = (dll.size() + 1) * sizeof(wchar_t);
    void* remote = VirtualAllocEx(process, nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remote || !WriteProcessMemory(process, remote, dll.c_str(), bytes, nullptr)) {
        CloseHandle(process);
        return fail("couldn't write into the game");
    }

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

    if (!result) return fail("LoadLibrary returned null, the dll failed to load");
    std::printf("[+] injected! press Insert in game to open the menu\n");
    Sleep(1500);
    return 0;
}
