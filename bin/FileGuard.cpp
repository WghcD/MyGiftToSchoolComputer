// FileProtector.cpp


#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>

// ========== 提权函数 ==========
BOOL EnableDebugPrivilege() {
    HANDLE hToken;
    TOKEN_PRIVILEGES tkp;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
        return FALSE;
    if (!LookupPrivilegeValueA(NULL, "SeDebugPrivilege", &tkp.Privileges[0].Luid)) {
        CloseHandle(hToken);
        return FALSE;
    }
    tkp.PrivilegeCount = 1;
    tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    BOOL bRet = AdjustTokenPrivileges(hToken, FALSE, &tkp, 0, NULL, 0);
    CloseHandle(hToken);
    return bRet && GetLastError() == ERROR_SUCCESS;
}

// ========== 核心锁定：将句柄转入winlogon.exe ==========
BOOL LockFileByDuplicateHandle(const char* lpFilePath) {
    // 1. 查找 winlogon.exe 进程ID
    DWORD dwCsrssPid = 0;
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) {
        printf("[ERROR] CreateToolhelp32Snapshot failed, error: %lu\n", GetLastError());
        return FALSE;
    }

    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(PROCESSENTRY32);
    if (Process32First(hSnap, &pe)) {
        do {
            if (lstrcmpiA(pe.szExeFile, "winlogon.exe") == 0) {
                dwCsrssPid = pe.th32ProcessID;
                break;
            }
        } while (Process32Next(hSnap, &pe));
    }
    CloseHandle(hSnap);

    if (dwCsrssPid == 0) {
        printf("[ERROR] Cannot find winlogon.exe process\n");
        return FALSE;
    }
    printf("[INFO] Found winlogon.exe PID: %lu\n", dwCsrssPid);

    // 2. 打开 winlogon.exe 进程（需要 PROCESS_DUP_HANDLE 权限）
    HANDLE hProcess = OpenProcess(PROCESS_DUP_HANDLE, FALSE, dwCsrssPid);
    if (!hProcess) {
        printf("[ERROR] OpenProcess failed, error: %lu\n", GetLastError());
        return FALSE;
    }

    // 3. 打开目标文件（dwShareMode=0 独占模式）
    HANDLE hFile = CreateFileA(
        lpFilePath,
        GENERIC_READ | GENERIC_WRITE | DELETE,
        0,                      // 不共享，禁止其他进程访问
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);

    if (hFile == INVALID_HANDLE_VALUE) {
        printf("[ERROR] Cannot open file, error: %lu\n", GetLastError());
        CloseHandle(hProcess);
        return FALSE;
    }

    // 4. 复制句柄到 winlogon.exe，并关闭本地句柄
    HANDLE hDup = NULL;
    BOOL bRet = DuplicateHandle(
        GetCurrentProcess(),    // 源进程
        hFile,                  // 要复制的句柄
        hProcess,               // 目标进程 (winlogon.exe)
        &hDup,                  // 输出：目标进程中的句柄
        0,
        FALSE,
        DUPLICATE_SAME_ACCESS | DUPLICATE_CLOSE_SOURCE); // 复制后自动关闭源句柄

    CloseHandle(hProcess);

    if (bRet) {
        printf("[SUCCESS] Handle duplicated to winlogon.exe, file locked!\n");
    } else {
        printf("[ERROR] DuplicateHandle failed, error: %lu\n", GetLastError());
        // 如果复制失败，手动关闭源句柄
        CloseHandle(hFile);
    }

    return bRet;
}

// ========== 保护单个文件 ==========
BOOL ProtectSingleFile(const char* lpFilePath) {
    printf("\n[INFO] Protecting: %s\n", lpFilePath);

    // 检查文件是否存在
    DWORD dwAttr = GetFileAttributesA(lpFilePath);
    if (dwAttr == INVALID_FILE_ATTRIBUTES) {
        printf("[ERROR] File not found: %s\n", lpFilePath);
        return FALSE;
    }

    // 锁定文件（核心方法）
    if (!LockFileByDuplicateHandle(lpFilePath)) {
        printf("[WARN] File locking failed\n");
        return FALSE;
    }

    // 设置只读属性（可选）
    dwAttr |= FILE_ATTRIBUTE_READONLY;
    SetFileAttributesA(lpFilePath, dwAttr);
    printf("[INFO] File attributes set to READONLY\n");

    return TRUE;
}

// ========== 显示帮助 ==========
VOID ShowHelp(const char* szExeName) {
    printf("\n");
    printf("FileProtector - R3 File Protection Tool\n");
    printf("Usage: %s <file1> <file2> ...\n\n", szExeName);
    printf("Examples:\n");
    printf("  %s C:\\test.txt D:\\data.bin\n", szExeName);
    printf("\n");
    printf("How it works:\n");
    printf("  - Opens each file with exclusive access (no sharing).\n");
    printf("  - Duplicates the file handle into the system process (winlogon.exe).\n");
    printf("  - The lock persists until system reboot.\n");
    printf("\n");
    printf("Notes:\n");
    printf("  - Protected files can be READ and EXECUTED, but NOT written or deleted.\n");
    printf("  - Run as Administrator for best results.\n");
    printf("\n");
}

// ========== 主函数 ==========
int main(int argc, char* argv[]) {
	HWND consoleWindow = GetConsoleWindow();//这个版本专用
    ShowWindow(consoleWindow, SW_HIDE);
    if (argc < 2) {
        ShowHelp(argv[0]);
        return 1;
    }

    // 检查是否为帮助选项
    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        ShowHelp(argv[0]);
        return 0;
    }

    // 提权
    if (!EnableDebugPrivilege()) {
        printf("[WARN] Failed to enable SeDebugPrivilege. Some operations may fail.\n");
        printf("[WARN] Please run as Administrator.\n");
    }

    // 保护每个文件
    int nSuccess = 0;
    for (int i = 1; i < argc; i++) {
        if (ProtectSingleFile(argv[i])) {
            nSuccess++;
        }
    }

    printf("\n[INFO] Protected %d of %d files.\n", nSuccess, argc - 1);
    printf("[INFO] Locked files will remain protected until system reboot.\n");
    printf("[INFO] Exiting...\n");

    return (nSuccess == argc - 1) ? 0 : 1;
}