// ============================================================
// AuroraGlass P2 — OutputDebugString capture (DebugView-style).
//
// Reads the DBWIN protocol (DBWIN_BUFFER / DBWIN_DATA_READY /
// DBWIN_BUFFER_READY) so the D3D11 debug layer's
// ReportLiveDeviceObjects() output from a child process can be inspected
// without an external DebugView. Diagnostic tool only.
//
// Usage: P2_LiveObject_Capture <exe-path> [args...]
// ============================================================

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <cstdio>
#include <string>
#include <vector>

static HANDLE g_hBufferReady = nullptr;
static HANDLE g_hDataReady   = nullptr;
static HANDLE g_hMap         = nullptr;
static void*  g_pBuffer      = nullptr;
static volatile LONG g_stop  = 0;

static DWORD WINAPI ReaderThread(LPVOID) {
    while (InterlockedCompareExchange(&g_stop, 0, 0) == 0) {
        DWORD w = WaitForSingleObject(g_hDataReady, 200);
        if (w == WAIT_OBJECT_0) {
            DWORD pid = *reinterpret_cast<DWORD*>(g_pBuffer);
            const char* msg = reinterpret_cast<const char*>(g_pBuffer) + sizeof(DWORD);
            std::printf("[dbg %lu] %s\n", (unsigned long)pid, msg);
            std::fflush(stdout);
            SetEvent(g_hBufferReady);
        }
    }
    return 0;
}

int main(int argc, char** argv) {
    if (argc < 2) { std::printf("usage: %s <exe> [args...]\n", argv[0]); return 2; }

    g_hBufferReady = CreateEventA(nullptr, FALSE, FALSE, "DBWIN_BUFFER_READY");
    g_hDataReady   = CreateEventA(nullptr, FALSE, FALSE, "DBWIN_DATA_READY");
    g_hMap         = CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, 4096, "DBWIN_BUFFER");
    if (!g_hBufferReady || !g_hDataReady || !g_hMap) {
        std::printf("capture setup failed (another DebugView/capture may own DBWIN). err=%lu\n",
                    (unsigned long)GetLastError());
        return 3;
    }
    g_pBuffer = MapViewOfFile(g_hMap, FILE_MAP_READ, 0, 0, 4096);
    if (!g_pBuffer) { std::printf("MapViewOfFile failed\n"); return 3; }

    SetEvent(g_hBufferReady);
    HANDLE hThread = CreateThread(nullptr, 0, ReaderThread, nullptr, 0, nullptr);

    std::string cmd;
    for (int i = 1; i < argc; ++i) { if (i > 1) cmd += ' '; cmd += argv[i]; }
    std::vector<char> cmdline(cmd.begin(), cmd.end());
    cmdline.push_back('\0');

    STARTUPINFOA si{}; si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    if (!CreateProcessA(nullptr, cmdline.data(), nullptr, nullptr, FALSE,
                        0, nullptr, nullptr, &si, &pi)) {
        std::printf("CreateProcess failed: %lu\n", (unsigned long)GetLastError());
        return 4;
    }
    std::printf("[capture] launched pid=%lu: %s\n", (unsigned long)pi.dwProcessId, cmd.c_str());

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    std::printf("[capture] child exited with %lu\n", (unsigned long)exitCode);

    Sleep(500);
    InterlockedExchange(&g_stop, 1);
    if (hThread) { WaitForSingleObject(hThread, 1000); CloseHandle(hThread); }

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    UnmapViewOfFile(g_pBuffer);
    CloseHandle(g_hMap);
    CloseHandle(g_hBufferReady);
    CloseHandle(g_hDataReady);
    return 0;
}
