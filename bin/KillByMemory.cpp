#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>

// 获取进程 PID（句柄 → PID）
DWORD GetProcessIdByHandle(HANDLE hProcess) {
    // 优先用 GetProcessId，若无 QUERY_LIMITED 权限则可能失败，
    // 0x1478 包含 PROCESS_QUERY_LIMITED_INFORMATION，所以可用
    return GetProcessId(hProcess);
}

// 暴力杀掉目标进程（通过内存破坏 + 劫持所有线程）
BOOL KillProcessByMemory(HANDLE hProcess) {
    DWORD pid = GetProcessIdByHandle(hProcess);
    if (pid == 0) return FALSE;

    // ---------- 1. 破坏 PEB 的关键字段 ----------
    // 通过 NtQueryInformationProcess 获取 PEB 地址
    typedef LONG NTSTATUS;
    typedef struct _PROCESS_BASIC_INFORMATION {
        PVOID Reserved1;
        PVOID PebBaseAddress;
        PVOID Reserved2[2];
        ULONG_PTR UniqueProcessId;
        PVOID Reserved3;
    } PROCESS_BASIC_INFORMATION;

    HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
    if (hNtdll) {
        typedef NTSTATUS (NTAPI *pNtQueryInformationProcess)(
            HANDLE, ULONG, PVOID, ULONG, PULONG);
        pNtQueryInformationProcess NtQueryInformationProcess =
            (pNtQueryInformationProcess)GetProcAddress(hNtdll, "NtQueryInformationProcess");
        if (NtQueryInformationProcess) {
            PROCESS_BASIC_INFORMATION pbi = {0};
            ULONG retLen = 0;
            if (NtQueryInformationProcess(hProcess, 0/*ProcessBasicInformation*/,
                &pbi, sizeof(pbi), &retLen) == 0 && pbi.PebBaseAddress) {
                // 写入 0 到 PEB->Ldr 偏移 (64位：0x18，32位：0x0C)
                PVOID ldrAddr = (PBYTE)pbi.PebBaseAddress + (sizeof(PVOID) == 8 ? 0x18 : 0x0C);
                ULONG_PTR zero = 0;
                WriteProcessMemory(hProcess, ldrAddr, &zero, sizeof(zero), NULL);
            }
        }
    }

    // ---------- 2. 劫持主线程（以及所有能打开的线程） ----------
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return FALSE;

    THREADENTRY32 te;
    te.dwSize = sizeof(te);
    if (!Thread32First(hSnapshot, &te)) {
        CloseHandle(hSnapshot);
        return FALSE;
    }

    BOOL bSuccess = FALSE;
    do {
        if (te.th32OwnerProcessID != pid) continue;

        // 打开线程，请求挂起/恢复和上下文操作权限
        HANDLE hThread = OpenThread(
            THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_SET_CONTEXT,
            FALSE, te.th32ThreadID);
        if (!hThread) continue;

        // 挂起线程
        SuspendThread(hThread);

        // 获取线程上下文
        CONTEXT ctx = {0};
        ctx.ContextFlags = CONTEXT_CONTROL; // 只修改指令指针
        if (GetThreadContext(hThread, &ctx)) {
            // 将 RIP/EIP 设为一个非法地址，恢复后立即崩溃
#if defined(_AMD64_)
            ctx.Rip = 0xDEADBEEFDEADBEEF;
#elif defined(_ARM64_)
            ctx.Pc = 0xDEADBEEFDEADBEEF;
#else
            ctx.Eip = 0xDEADBEEF;
#endif
            SetThreadContext(hThread, &ctx);
            bSuccess = TRUE;
        }

        // 恢复线程（让它跳向死亡）
        ResumeThread(hThread);
        CloseHandle(hThread);
    } while (Thread32Next(hSnapshot, &te));

    CloseHandle(hSnapshot);
    return bSuccess;
}

// 使用方法：
int main() {
    // 以 0x1478 权限打开目标进程（示例）
    DWORD targetPid = 8756;
    HANDLE hProc = OpenProcess(
        PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_VM_WRITE,
        FALSE, targetPid);
    if (hProc) {
        KillProcessByMemory(hProc);
        CloseHandle(hProc);
    }
    return 0;
}