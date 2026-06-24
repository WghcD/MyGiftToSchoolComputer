#pragma once
#include<windows.h>
#include<cstdio>
#include<string>
#include <algorithm>
#include<fstream>
#include<vector>
#include <winternl.h>  
#include <aclapi.h>
#include <sddl.h>
#include <cstdlib>
#include <psapi.h>
#include <TlHelp32.h>  
#include <wbemidl.h>
#include <comdef.h>
#include "NtApi.h"
#include "admin.h"
#include "Interaction.h"
#include "Session.h"
#include "APCInject.h"


#include "ServiceManage.h"
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "wbemuuid.lib")
#define GetProcess getProcess
#define GetPid getPid

using namespace std;

/**
 * @brief 将窄字符串转为宽字符串（仅支持ASCII字符集）
 * @param narrowStr 输入窄字符串（ASCII字符）
 * @return 转换后的宽字符串指针（需调用者释放内存）
 * 
 * 注意：仅保证前128个ASCII字符正确转换
 *       非ASCII字符将转为乱码（0x3F '?'）
 */
wchar_t* ConvertToWideString(const std::string& narrowStr) {
    // 创建宽字符缓冲区
    size_t bufferSize = narrowStr.size() + 1;  // +1 for null terminator
    wchar_t* wideBuffer = new wchar_t[bufferSize];
    
    // ASCII字符直接映射（0-127）
    for (size_t i = 0; i < narrowStr.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(narrowStr[i]);
        wideBuffer[i] = (c < 128) ? static_cast<wchar_t>(c) : L'?';
    }
    
    wideBuffer[narrowStr.size()] = L'\0';  // 添加宽字符串终止符
    return wideBuffer;
}

bool SetServiceBinPath(const std::string& serviceName, const std::string& newPath) {
    std::wstring wServiceName = std::wstring(serviceName.begin(), serviceName.end());
    std::wstring wNewPath = std::wstring(newPath.begin(), newPath.end());

    SC_HANDLE hSCM = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (hSCM == nullptr) {
        return false;
    }

    SC_HANDLE hService = OpenServiceW(hSCM, wServiceName.c_str(), SERVICE_CHANGE_CONFIG);
    if (hService == nullptr) {
        CloseServiceHandle(hSCM);
        return false;
    }

    BOOL result = ChangeServiceConfigW(
        hService,
        SERVICE_NO_CHANGE,
        SERVICE_NO_CHANGE,
        SERVICE_NO_CHANGE,
        wNewPath.c_str(),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr
    );

    CloseServiceHandle(hService);
    CloseServiceHandle(hSCM);

    return (result != 0);
}

char* addStrings(const char* str1, const char* str2) {
    // 处理空指针情况
    if (!str1) str1 = "";
    if (!str2) str2 = "";
    
    // 计算总长度（含结束符）
    size_t len1 = strlen(str1);
    size_t len2 = strlen(str2);
    size_t totalLen = len1 + len2 + 1;
    
    // 分配内存
    char* result = new char[totalLen];
    
    // 安全拼接
    strcpy(result, str1);      // 复制第一部分
    strcat(result, str2);      // 追加第二部分
    
    return result;
}

bool GetFullControl(DWORD targetPid) {
    //HANDLE hProcess = OpenProcess(WRITE_DAC | READ_CONTROL, FALSE, targetPid);
	HANDLE hProcess = GetHandle(targetPid);
    
	if(!hProcess){cout<<"NtOpenProcess Failed\n";return false;  }
    // 获取当前进程令牌
    HANDLE hToken;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        CloseHandle(hProcess);
        return false;
    }

    // 获取当前用户SID
    DWORD dwSize = 0;
    GetTokenInformation(hToken, TokenUser, NULL, 0, &dwSize);
    PTOKEN_USER pTokenUser = (PTOKEN_USER)LocalAlloc(LPTR, dwSize);
    if (!GetTokenInformation(hToken, TokenUser, pTokenUser, dwSize, &dwSize)) {
        CloseHandle(hToken);
        CloseHandle(hProcess);
        LocalFree(pTokenUser);
        return false;
    }

    // 创建新的DACL
    EXPLICIT_ACCESS ea;
    ZeroMemory(&ea, sizeof(EXPLICIT_ACCESS));                               
    ea.grfAccessPermissions = PROCESS_ALL_ACCESS;
    ea.grfAccessMode = GRANT_ACCESS;
    ea.grfInheritance = NO_INHERITANCE;
    ea.Trustee.TrusteeForm = TRUSTEE_IS_SID;
    ea.Trustee.TrusteeType = TRUSTEE_IS_USER;
    ea.Trustee.ptstrName = (LPTSTR)pTokenUser->User.Sid;

    PACL pNewDacl = NULL;
    if (SetEntriesInAcl(1, &ea, NULL, &pNewDacl) != ERROR_SUCCESS) {
        LocalFree(pTokenUser);
        CloseHandle(hToken);
        CloseHandle(hProcess);
        return false;
    }

    // 设置新的安全描述符
    DWORD dwResult = SetSecurityInfo(
        hProcess,
        SE_KERNEL_OBJECT,
        DACL_SECURITY_INFORMATION,
        NULL,
        NULL,
        pNewDacl,
        NULL
    );

    LocalFree(pTokenUser);
    LocalFree(pNewDacl);
    CloseHandle(hToken);
    CloseHandle(hProcess);

    return dwResult == ERROR_SUCCESS;
}

string GetCurrentDirectory() {
    char exePath[MAX_PATH] = {0};
    
    // 获取可执行文件完整路径
    if (GetModuleFileNameA(NULL, exePath, MAX_PATH) == 0) {
        return "";  // 获取失败返回空字符串
    }
    
    // 查找最后一个路径分隔符
    char* lastSlash = strrchr(exePath, '\\');
    if (lastSlash != nullptr) {
        *lastSlash = '\0';  // 截断文件名部分
    }
    
    return std::string(exePath);  // 返回目录路径
}


bool isProcessExist(const char *procressName)                //此函数用于检查进程是否存在 进程名不区分大小写
{
    char pName[MAX_PATH];                                //和PROCESSENTRY32结构体中的szExeFile字符数组保持一致，便于比较
    strcpy(pName,procressName);                            //拷贝数组
    CharLowerBuff(pName,MAX_PATH);                        //将名称转换为小写
    PROCESSENTRY32 currentProcess;                        //存放快照进程信息的一个结构体
    currentProcess.dwSize = sizeof(currentProcess);        //在使用这个结构之前，先设置它的大小
    HANDLE hProcess = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);//给系统内的所有进程拍一个快照

    if (hProcess == INVALID_HANDLE_VALUE)
    {
        printf("CreateToolhelp32Snapshot()调用失败!\n");
        return false;
    }

    bool bMore=Process32First(hProcess,&currentProcess);        //获取第一个进程信息
    while(bMore)
    {
        CharLowerBuff(currentProcess.szExeFile,MAX_PATH);        //将进程名转换为小写
        if (strcmp(currentProcess.szExeFile,pName)==0)            //比较是否存在此进程
        {
            CloseHandle(hProcess);                                //清除hProcess句柄
            return true;
        }
        bMore=Process32Next(hProcess,&currentProcess);            //遍历下一个
    }

    CloseHandle(hProcess);    //清除hProcess句柄
    return false;//进程不存在
}
bool isProcessExist(int pid) {
    if (pid <= 0)
        return false;
    // 直接尝试打开进程（无需遍历进程列表）
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (hProcess) {
        CloseHandle(hProcess);
        return true;
    }
    return false;
}



vector<int> getPid(const std::string& processName) {
    PROCESSENTRY32 processInfo;
    processInfo.dwSize = sizeof(processInfo);
	vector<int> Pid;
    HANDLE processesSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (processesSnapshot == INVALID_HANDLE_VALUE) {
        return Pid;
    }

    std::string targetName = processName;
    // 统一转换为小写比较
    std::transform(targetName.begin(), targetName.end(), targetName.begin(), ::tolower);
    // 移除可能的.exe后缀
	while(targetName.substr(targetName.size() - 4) == ".exe"){targetName = targetName.substr(0, targetName.size() - 4);}
    

    int pid;
    if (Process32First(processesSnapshot, &processInfo)) {
        do {
            std::string currentName = processInfo.szExeFile;
            std::transform(currentName.begin(), currentName.end(), currentName.begin(), ::tolower);
            
            // 比较进程名（带或不带.exe）
            if (currentName == targetName || 
                (currentName.size() > 4 && 
                 currentName.substr(0, currentName.size() - 4) == targetName)) {
                pid = processInfo.th32ProcessID;
                Pid.push_back(pid);
            }
        } while (Process32Next(processesSnapshot, &processInfo));
    }

    CloseHandle(processesSnapshot);
	if(Pid.size()!=0){cout<<"Find "<<Pid.size()<<" processes named "<<processName<<"\n";}
    return Pid;
}


int getSinglePid(const std::string& processName) {
    PROCESSENTRY32 processInfo;
    processInfo.dwSize = sizeof(processInfo);

    HANDLE processesSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (processesSnapshot == INVALID_HANDLE_VALUE) {
        return -1;
    }

    std::string targetName = processName;
    // 统一转换为小写比较
    std::transform(targetName.begin(), targetName.end(), targetName.begin(), ::tolower);
    // 移除可能的.exe后缀
	while(targetName.substr(targetName.size() - 4) == ".exe"){targetName = targetName.substr(0, targetName.size() - 4);}
    

    int pid=-1;
    if (Process32First(processesSnapshot, &processInfo)) {
        do {
            std::string currentName = processInfo.szExeFile;
            std::transform(currentName.begin(), currentName.end(), currentName.begin(), ::tolower);
            
            // 比较进程名（带或不带.exe）
            if (currentName == targetName || 
                (currentName.size() > 4 && 
                 currentName.substr(0, currentName.size() - 4) == targetName)) {
                pid = processInfo.th32ProcessID;
				break;
            }
        } while (Process32Next(processesSnapshot, &processInfo));
    }

    CloseHandle(processesSnapshot);
    return pid;
}


bool TerminateProcessById(DWORD pid) {
    // 打开目标进程获取句柄
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (hProcess == NULL) {
        return false; // 无法打开进程
    }
    
    // 终止进程
    BOOL result = TerminateProcess(hProcess, 0);
    
    // 清理资源
    CloseHandle(hProcess);
    
    return result != FALSE;
}


DWORD GetProcessIDOfMainWindow() {//返回当前顶层窗口进程PID
	HWND hwnd = GetForegroundWindow();
    DWORD lpdwProcessId;
    DWORD threadId = GetWindowThreadProcessId(hwnd, &lpdwProcessId);
    return lpdwProcessId;
}



bool isNumber(const char* in){
	for(const char* p=in;*p!='\0';p++){
		if(*p<'0'||*p>'9'){return false;}
	}
	return true;
}





void SuspendProcessByThreads(DWORD pid) {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    THREADENTRY32 te = { sizeof(THREADENTRY32) };
    
    // 暂停目标进程所有线程
    if (Thread32First(hSnapshot, &te)) {
        do {
            if (te.th32OwnerProcessID == pid) {
                HANDLE hThread = OpenThread(THREAD_SUSPEND_RESUME, FALSE, te.th32ThreadID);
                if (hThread) {
                    SuspendThread(hThread);
                    CloseHandle(hThread);
                }
            }
        } while (Thread32Next(hSnapshot, &te));
    }
    CloseHandle(hSnapshot);


}



// 恢复指定进程的所有线程
void ResumeProcess(DWORD pid) {
    HANDLE hThreadSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (hThreadSnap == INVALID_HANDLE_VALUE) return;

    THREADENTRY32 te32;
    te32.dwSize = sizeof(THREADENTRY32);
    
    // 遍历所有线程
    if (Thread32First(hThreadSnap, &te32)) {
        do {
            // 只处理属于目标进程的线程
            if (te32.th32OwnerProcessID == pid) {
                HANDLE hThread = OpenThread(THREAD_SUSPEND_RESUME, FALSE, te32.th32ThreadID);
                if (hThread) {
                    // 递减挂起计数直到线程恢复运行
                    while (ResumeThread(hThread) > 0);
                    CloseHandle(hThread);
                }
            }
        } while (Thread32Next(hThreadSnap, &te32));
    }
    CloseHandle(hThreadSnap);
}

typedef NTSTATUS(__cdecl* PRTLSETPROCESSISCRITICAL)(IN BOOLEAN NewValue, OUT PBOOLEAN OldValue OPTIONAL, IN BOOLEAN NeedBreaks);
BOOLEAN BSODCriticalProtect(bool isEnable)
{

	HMODULE  hNtdllMod = GetModuleHandleA("ntdll.dll");
	if (!hNtdllMod)
		return FALSE;
 
	PRTLSETPROCESSISCRITICAL pRtlSetProcessIsCritical;
	pRtlSetProcessIsCritical = (PRTLSETPROCESSISCRITICAL)GetProcAddress(hNtdllMod, "RtlSetProcessIsCritical");//盗用windows保护系统进程的金钟罩
	if (!pRtlSetProcessIsCritical)
		return FALSE;
 
	NTSTATUS status = pRtlSetProcessIsCritical(isEnable, NULL, FALSE);

 
	return TRUE;
}



/**
 * 快速启动可执行文件（ANSI 版本）
 * 
 * @param exePath 可执行文件路径（ANSI 字符串）
 * @return 成功启动返回 true，否则返回 false
 */
bool LaunchExe(const char* exePath) {
    STARTUPINFO si = { sizeof(si) };
    PROCESS_INFORMATION pi = {0};
    
    // 核心启动函数
    BOOL success = CreateProcessA(
        NULL,           // 应用程序名（使用命令行参数）
        (LPSTR)exePath, // 命令行（包含完整路径）
        NULL,           // 进程安全属性
        NULL,           // 线程安全属性
        FALSE,          // 句柄继承选项
        CREATE_NO_WINDOW, // 创建标志：不显示窗口
        NULL,           // 环境变量
        NULL,           // 当前目录
        &si,            // 启动信息
        &pi             // 进程信息
    );

    if (success) {
        // 立即关闭不需要的句柄
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return true;
    }
    return false;
}


/*
 * @brief 以当前进程权限启动新进程
 * @param commandLine 完整命令行（可包含参数）
 * @return 成功返回进程句柄，失败返回NULL（调用GetLastError获取错误码）
 */
HANDLE System(const std::string& commandLine) {
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi = {0};
    
    // 创建可修改的命令行缓冲区
    std::vector<char> cmdBuf(commandLine.begin(), commandLine.end());
    cmdBuf.push_back('\0');  // 确保NULL终止

    // 创建进程
    if (!CreateProcessA(
        NULL,                   // 从命令行解析可执行文件
        cmdBuf.data(),          // 可写命令行缓冲区
        NULL,                   // 进程安全属性
        NULL,                   // 线程安全属性
        FALSE,                  // 不继承句柄
        CREATE_NEW_CONSOLE,                      // 无特殊标志
        NULL,                   // 继承当前环境块
        NULL,                   // 继承当前目录
        &si,                    // 启动信息
        &pi                     // 接收进程信息
    )) {
        return NULL;  // 创建失败
    }

    // 关闭不需要的线程句柄
    CloseHandle(pi.hThread);
    
    return pi.hProcess;  // 返回进程句柄（调用者需关闭）
}


// 定义未公开API的函数指针类型
typedef NTSTATUS(NTAPI* pfnNtSuspendProcess)(HANDLE ProcessHandle);

/**
 * 挂起指定进程（ANSI兼容实现）
 * 
 * @param pid 目标进程ID
 * @return 成功返回true，失败返回false
 */
bool SuspendProcess(DWORD pid) {
    // 1. 打开目标进程
    HANDLE hProcess = OpenProcess(PROCESS_SUSPEND_RESUME, FALSE, pid);
    if (hProcess == NULL) {
        return false;
    }

    // 2. 获取NtSuspendProcess函数地址
    HMODULE hNtDll = GetModuleHandle("ntdll.dll");  // ANSI版本使用
    pfnNtSuspendProcess NtSuspendProcess = 
        (pfnNtSuspendProcess)GetProcAddress(hNtDll, "NtSuspendProcess");
    
    if (NtSuspendProcess == NULL) {
        CloseHandle(hProcess);
        return false;
    }

    // 3. 执行挂起操作
    NTSTATUS status = NtSuspendProcess(hProcess);
    
    // 4. 清理资源
    CloseHandle(hProcess);
    return NT_SUCCESS(status);
}
