//
// Usa: LoadTCandAPP Volume.tc Int
//
//

#define UNICODE
#define LOGGER

#include <cstring>

#ifdef LOGGER
#include <spdlog/spdlog.h>
std::shared_ptr<spdlog::logger> logger;
#endif

#include <windows.h>
#include <winioctl.h>
#include <shellapi.h>
#include <shlwapi.h>

// FROM APIDRVR.H
#define TC_DRIVER L"\\\\.\\VeraCrypt"

#define TC_IOCTL(CODE) (CTL_CODE (FILE_DEVICE_UNKNOWN, 0x800 + (CODE), METHOD_BUFFERED, FILE_ANY_ACCESS))
#define TC_IOCTL_DISMOUNT_VOLUME TC_IOCTL (4)

typedef struct
{
        int nDosDriveNo;        /* Drive letter to unmount */
        BOOL ignoreOpenFiles;
        BOOL HiddenVolumeProtectionTriggered;
        int nReturnCode;        /* Return code back from driver: look at TCDEFS.H */
} UNMOUNT_STRUCT;

#pragma comment(linker,"/SUBSYSTEM:Windows")

#pragma comment(linker,"/DEFAULTLIB:SHELL32.lib")
#pragma comment(linker,"/DEFAULTLIB:SHLWAPI.lib")
#pragma comment(linker,"/DEFAULTLIB:USER32.lib")
#pragma comment(linker,"/DEFAULTLIB:KERNEL32.lib")

#define Exists(x) ((BOOL) (CreateFile(x, 0, 0, 0, OPEN_EXISTING, 0, 0) != INVALID_HANDLE_VALUE))

// Nota: in Win64 TrueCrypt è un'App nativa
#ifndef VERA_CRYPT
    LPWSTR TC = L"C:\\Program Files\\TrueCrypt\\TrueCrypt.exe";
#else
    LPWSTR TC = L"C:\\Program Files\\VeraCrypt\\VeraCrypt.exe";
#endif

// Percorso della versione a 32-bit in Windows x64
static LPWSTR wsProcesses[3] = {
   L"C:\\Program Files (x86)\\Mozilla Firefox\\firefox.exe", // 0
   L"C:\\Program Files (x86)\\Mozilla Thunderbird\\thunderbird.exe", // 1
};


static LPWSTR wsProfiles[3] = {
   L" -profile _:\\firefox", // 0
   L" -profile _:\\thunderbird", // 1
};


struct THRDATA {
    PROCESS_INFORMATION* pi;
    DWORD dwThreads=0;
    HWND hWnd;
    wchar_t DriveLetter;
} td;

void wait_function(struct THRDATA* p) {
    HANDLE hProcess = p->pi->hProcess; // these members are overwritten at every new app start
    HANDLE hThread = p->pi->hThread;

    WaitForSingleObject(hProcess, INFINITE);
    #ifdef LOGGER
        logger->info("Process<{}>: wait_function for process {} is ending...", GetCurrentProcessId(), (int) hProcess);
    #endif
    CloseHandle(hProcess);
    CloseHandle(hThread);
    PostMessage(p->hWnd, WM_USER+0xDE, 0, 0);
}

void cleanup_function(struct THRDATA* p) {
    wchar_t Drive[4] = L" :\\";
    Drive[0] = p->DriveLetter;
#if 0
    wchar_t Args[12] = L" /q /s /d A";
    Args[10] = p->DriveLetter;

    #ifdef LOGGER
        logger->info("Process<{}>: Dismounting encrypted drive", GetCurrentProcessId());
    #endif

    int i = 15;
    while (PathFileExists(Drive) && i-- > 0) {
        STARTUPINFO si;
        PROCESS_INFORMATION pi;
        ZeroMemory(&si, sizeof(STARTUPINFO));
        ZeroMemory(&pi, sizeof(PROCESS_INFORMATION));
        si.cb = sizeof(STARTUPINFO);
        BOOL bRet1 = CreateProcess(TC, Args, 0, 0, 0, 0, 0, 0, &si, &pi);
    #ifdef LOGGER
        logger->info("Process<{}>: CreateProcess returned {} for process {}", GetCurrentProcessId(), bRet1, (int) pi.hProcess);
    #endif
        Sleep(3000); // WaitForSingleProcess immediately returns!
    //~ #ifdef LOGGER
        //~ logger->info("Process<{}>: WaitForSingleObject returned {}", GetCurrentProcessId(), bRet2);
    //~ #endif
    }
#endif
    int i = 5;
    while (PathFileExists(Drive) && i-- > 0) {
        UNMOUNT_STRUCT unmount;
        DWORD dwResult;
        unmount.nDosDriveNo = (int) (p->DriveLetter - wchar_t('A'));
        HANDLE hDriver = CreateFile(TC_DRIVER,  (GENERIC_READ | GENERIC_WRITE), 0, 0, OPEN_EXISTING, 0, 0);
#ifdef LOGGER
        if (hDriver == INVALID_HANDLE_VALUE) {
            logger->error("Process<{}>: Can't open device driver", GetCurrentProcessId());
        }
#endif
        if (i == 0) {
            unmount.ignoreOpenFiles = true;
            #ifdef LOGGER
            logger->warn("Process<{}>: Last attempt, forcing dismount...", GetCurrentProcessId());
            #endif
        }
    
        BOOL bResult = DeviceIoControl (hDriver, TC_IOCTL_DISMOUNT_VOLUME, &unmount, sizeof (unmount), &unmount, sizeof (unmount), &dwResult, NULL);
#ifdef LOGGER
        if (!bResult) {
            logger->error("Process<{}>: DeviceIoControl failed", GetCurrentProcessId());
        } else {
            if (!unmount.nReturnCode)
                logger->info("Process<{}>: DeviceIoControl TC_IOCTL_DISMOUNT_VOLUME succeeded", GetCurrentProcessId());
            else
                // Typical: ERR_FILES_OPEN=6
                logger->warn("Process<{}>: DeviceIoControl TC_IOCTL_DISMOUNT_VOLUME failed with code {}", GetCurrentProcessId(), unmount.nReturnCode);
        }
#endif
        Sleep(5000);
    }

    if (!PathFileExists(Drive)) {
        ShutdownBlockReasonDestroy(p->hWnd);
        #ifdef LOGGER
            logger->info("Process<{}>: Drive was correctly dismounted", GetCurrentProcessId());
        #endif
        ExitProcess(0);
    } else {
        ;
        #ifdef LOGGER
            logger->error("Process<{}>: Drive cleanup failed!", GetCurrentProcessId());
        #endif
    }
}


LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_QUERYENDSESSION:
        ShutdownBlockReasonCreate(hWnd, L"Unità cifrate ancora connesse!");
    #ifdef LOGGER
        logger->info("Process<{}>: Received WM_QUERYENDSESSION", GetCurrentProcessId());
        logger->flush();
    #endif
        CreateThread(0, 0, (LPTHREAD_START_ROUTINE)&cleanup_function, &td, 0, 0);
        return 0;
        break;
    case WM_ENDSESSION:
    #ifdef LOGGER
        logger->info("Process<{}>: Received WM_ENDSESSION", GetCurrentProcessId());
        logger->flush();
    #endif
        break;
    case WM_DESTROY:
    #ifdef LOGGER
        logger->info("Process<{}>: Received WM_DESTROY", GetCurrentProcessId());
        logger->flush();
    #endif
        PostQuitMessage(0);
        break;
    case WM_USER+0xDE:
        if (--td.dwThreads == 0)
            PostQuitMessage(0);
    #ifdef LOGGER
        logger->info("Process<{}>: Received WM_USER+0xDE", GetCurrentProcessId());
    #endif
        break;
    case WM_USER+0xEE:
        if (lParam != 0xFF)
            td.DriveLetter=lParam;
    #ifdef LOGGER
        logger->info("Process<{}>: Received WM_USER+0xEE", GetCurrentProcessId());
    #endif
        STARTUPINFO si;
        PROCESS_INFORMATION pi;
        ZeroMemory(&si, sizeof(STARTUPINFO));
        ZeroMemory(&pi, sizeof(PROCESS_INFORMATION));
        si.cb = sizeof(STARTUPINFO);

        LPWSTR Args;
        Args = StrDup(wsProfiles[wParam]);
        Args[10] = td.DriveLetter;

        if (! CreateProcess(wsProcesses[wParam], Args, 0, 0, 0, 0, 0, 0, &si, &pi))
        {
            MessageBox(0, L"Non è stato possibile avviare l'App specificata!", 0, MB_OK|MB_ICONSTOP);
        }
        else {
            td.pi = &pi;
            td.dwThreads++;
            td.hWnd = hWnd;
            CreateThread(0, 0, (LPTHREAD_START_ROUTINE)&wait_function, &td, 0, 0);
            #ifdef LOGGER
            logger->info("Process<{}>: Created new thread waiting process {}", GetCurrentProcessId(), (int) pi.hProcess);
            #endif
        }
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Trova la prima lettera di unità disponibile
// GetVolumeInformation("A:\\", lpVolumeName, 255, 0, 0, 0,0, 0);
char FindFirstFreeDrive() {
    wchar_t Marker[21] = L" :\\LoadTCandApp.mark";

    for (int i=0; i < 25; i++) {
        Marker[0] = (wchar_t) 'A'+i;
        if (! ((1<<i) & GetLogicalDrives()) || PathFileExists(Marker))
            return 'A'+i;
    }

    return 0;
}


int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
            LPSTR lpCmdLine, int nCmdShow) {
    LPWSTR* Argv;
    LPWSTR Args;
    int Argc, i=0, r=1;
    HWND hWnd;
    LPWSTR lpwsClass = L"LoadTCandApp_Monitor_Class";

    Argv = CommandLineToArgvW(GetCommandLineW(), &Argc);

    if (Argc < 2)
        goto Exit;

    if (hWnd=FindWindow(lpwsClass, L"LoadTCandApp Monitor")) {
        PostMessage(hWnd, WM_USER+0xEE, StrToInt(Argv[2]), 0xFF);
        goto Exit;
    }

    // Logging by a 2nd instance MUST be avoided, or it will crash due to the log file already locked 
#ifdef LOGGER
    DWORD dwTchars = ExpandEnvironmentStrings(L"%TEMP%", nullptr, 0);
    LPTSTR lpDst = (LPTSTR) HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, dwTchars*2+40);
    ExpandEnvironmentStrings(L"%TEMP%", lpDst, dwTchars*2);
    lstrcat(lpDst, L"\\LoadTCandApp.log");
    LPSTR lpDst2 = (LPSTR) HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, dwTchars*2+40);
    WideCharToMultiByte(CP_ACP, 0, lpDst, -1, lpDst2, dwTchars*2+40, 0, 0);
    logger = spdlog::rotating_logger_mt("LoadTCandAPP", lpDst2, 1048576, 2);
    logger->info("Process<{}> starting main loop...", GetCurrentProcessId());
#endif // LOGGER

    wchar_t Drive[4] = L" :\\";
    Drive[0] = FindFirstFreeDrive();

    Args = (LPWSTR) LocalAlloc(LMEM_FIXED|LMEM_ZEROINIT, Argc + 2*lstrlen(GetCommandLineW()));

    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(STARTUPINFO));
    ZeroMemory(&pi, sizeof(PROCESS_INFORMATION));
    si.cb = sizeof(STARTUPINFO);

    if (! PathFileExists(Drive)) {
#ifndef VERA_CRYPT
        wsprintf(Args, L" /q /s /v %s /l %c", Argv[1], Drive[0]);
#else
        wsprintf(Args, L" /tc /quit /v %s /l %c", Argv[1], Drive[0]);
#endif
        if (! CreateProcess(TC, Args, 0, 0, 0, 0, 0, 0, &si, &pi)) {
            #ifdef LOGGER
                logger->error("Process<{}>: Failed mounting the encrypted volume", GetCurrentProcessId(), (int) hWnd);
            #endif
            MessageBox(0, L"Non è stato possibile montare l'unità cifrata", 0, MB_OK|MB_ICONSTOP);
            goto Exit;
        }
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }

    // Verifica la presenza del nuovo volume per 2 minuti
    while (! PathFileExists(Drive) && i < 120) {
        Sleep(1000);
        i++;
    }

    // Se il volume non c'è, esce silenziosamente
    if (! PathFileExists(Drive))
        goto Exit;

    WNDCLASSEXW wcex;
    wcex.cbSize         = sizeof(WNDCLASSEX);
    wcex.style          = 0;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = 0;
    wcex.hCursor        = 0;
    wcex.hbrBackground  = 0;
    wcex.lpszMenuName   = 0;
    wcex.lpszClassName  = lpwsClass;
    wcex.hIconSm        = 0;
    RegisterClassExW(&wcex);

    hWnd = CreateWindowW(lpwsClass, L"LoadTCandApp Monitor", WS_OVERLAPPEDWINDOW,
    CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

    ShowWindow(hWnd, 0);
    UpdateWindow(hWnd);
    
    PostMessage(hWnd, WM_USER+0xEE, StrToInt(Argv[2]), Drive[0]);

    #ifdef LOGGER
    logger->info("Process<{}>: starting message loop", GetCurrentProcessId());
    #endif

    // Ciclo di messaggi principale
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    #ifdef LOGGER
        logger->info("Process<{}>: main loop exited, cleaning up...", GetCurrentProcessId());
        logger->flush();
    #endif
    HANDLE hThread = CreateThread(0, 0, (LPTHREAD_START_ROUTINE)&cleanup_function, &td, 0, 0);
    WaitForSingleObject(hThread, INFINITE);

Exit:
    #ifdef LOGGER
        logger->info("Process<{}>: calling ExitProcess({})", GetCurrentProcessId(), r);
        logger->flush();
    #endif
    ExitProcess(r);
}
