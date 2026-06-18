#pragma once
#include <Windows.h>
#include <WtsApi32.h>
#include <userenv.h>
#pragma comment(lib, "userenv.lib")

#pragma comment(lib, "Wtsapi32.lib")

// ANSI函数：从winlogon这类高权限 交互受限 上下文中启动交互式用户进程
BOOL LaunchInteractiveProcess(LPCSTR lpApplicationPath) {
    DWORD dwSessionId = WTSGetActiveConsoleSessionId();
    if (dwSessionId == 0xFFFFFFFF) return FALSE;

    HANDLE hUserToken = NULL;
    if (!WTSQueryUserToken(dwSessionId, &hUserToken)) 
        return FALSE;

    // 创建交互式环境块
    LPVOID lpEnvironment = NULL;
    CreateEnvironmentBlock(&lpEnvironment, hUserToken, FALSE);

    // 配置桌面环境
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    si.lpDesktop = "winsta0\\default";  // 关键：指定交互桌面

    // 创建进程
    BOOL bResult = CreateProcessAsUserA(
        hUserToken,                  // 用户令牌
        lpApplicationPath,           // 应用路径
        NULL,                        // 命令行
        NULL,                        // 进程安全属性
        NULL,                        // 线程安全属性
        FALSE,                       // 不继承句柄
        CREATE_UNICODE_ENVIRONMENT,  // 创建标志
        lpEnvironment,               // 环境变量
        NULL,                        // 当前目录
        &si,                         // 启动信息
        &pi                          // 进程信息
    );

    // 清理资源
    if (bResult) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    DestroyEnvironmentBlock(lpEnvironment);
    CloseHandle(hUserToken);
    
    return bResult;
}


/**
 * @brief 从 Session0 启动交互式管理员进程
 * @param lpApplicationPath 应用程序完整路径
 * @return 成功返回进程句柄，失败返回NULL
 */
HANDLE LaunchAdminInteractiveProcess(LPCWSTR lpApplicationPath) {
    DWORD dwSessionId = WTSGetActiveConsoleSessionId();
    if (dwSessionId == 0xFFFFFFFF) {
        return NULL;
    }

    HANDLE hUserToken = NULL;
    if (!WTSQueryUserToken(dwSessionId, &hUserToken)) {
        return NULL;
    }

    // 提升令牌权限（关键步骤）
    HANDLE hElevatedToken = NULL;
    TOKEN_PRIVILEGES tkp = { 0 };
    if (!DuplicateTokenEx(hUserToken, TOKEN_ALL_ACCESS, NULL, 
                         SecurityImpersonation, TokenPrimary, &hElevatedToken)) {
        CloseHandle(hUserToken);
        return NULL;
    }
    
    tkp.PrivilegeCount = 1;
    LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &tkp.Privileges[0].Luid);
    tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    AdjustTokenPrivileges(hElevatedToken, FALSE, &tkp, sizeof(tkp), NULL, NULL);

    // 创建交互环境
    LPVOID lpEnvironment = NULL;
    if (!CreateEnvironmentBlock(&lpEnvironment, hElevatedToken, FALSE)) {
        CloseHandle(hElevatedToken);
        CloseHandle(hUserToken);
        return NULL;
    }

    // 配置启动参数
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = { 0 };
    si.lpDesktop = const_cast<LPWSTR>(L"winsta0\\default");  // 交互桌面
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_SHOW;

    // 创建管理员进程
    BOOL bResult = CreateProcessAsUserW(
        hElevatedToken,
        lpApplicationPath,
        NULL, 
        NULL, 
        NULL, 
        FALSE, 
        CREATE_UNICODE_ENVIRONMENT,
        lpEnvironment, 
        NULL, 
        &si, 
        &pi
    );

    // 清理资源
    DestroyEnvironmentBlock(lpEnvironment);
    CloseHandle(hElevatedToken);
    CloseHandle(hUserToken);

    if (!bResult) {
        return NULL;
    }

    CloseHandle(pi.hThread);
    return pi.hProcess;
}