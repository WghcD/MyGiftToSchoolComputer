#include <windows.h>
#include <shellapi.h>
#pragma once
#include <string>
#include <vector>

// 检查当前权限状态
bool IsAdminMode() {
    BOOL isAdmin = FALSE;
    PSID adminGroup = nullptr;
    
    SID_IDENTIFIER_AUTHORITY ntAuth = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&ntAuth, 2, SECURITY_BUILTIN_DOMAIN_RID,
        DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminGroup)) 
    {
        CheckTokenMembership(nullptr, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }
    return isAdmin != FALSE;
}

// 请求管理员权限（带命令行参数传递）
bool RequestAdminRightsWithArgs() {
    if (IsAdminMode()) return true; // 已是管理员权限

    // 获取当前可执行文件路径
    char exePath[MAX_PATH];
    GetModuleFileNameA(nullptr, exePath, MAX_PATH);

    // 获取完整命令行参数
    LPSTR cmdLine = GetCommandLineA();
    
    // 重新启动进程（带参数）
    SHELLEXECUTEINFOA sei = { sizeof(sei) };
    sei.lpVerb = "runas";
    sei.lpFile = exePath;
    sei.lpParameters = cmdLine; // 传递完整命令行参数
    sei.nShow = SW_SHOW;
    
    if (ShellExecuteExA(&sei)) {
        ExitProcess(0); // 成功启动后退出当前进程
        return true;
    }
    return false; // 用户取消UAC或启动失败
}



bool EnableDebugPrivilege() {
    HANDLE hToken;
    TOKEN_PRIVILEGES tkp;

    // 1. 打开当前进程令牌
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
        return false;
    }

    // 2. 获取调试特权LUID
    LUID luid;
    if (!LookupPrivilegeValue(NULL, "SeDebugPrivilege", &luid)) {
        CloseHandle(hToken);
        return false;
    }

    // 3. 配置特权属性
    tkp.PrivilegeCount = 1;
    tkp.Privileges[0].Luid = luid;
    tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    // 4. 启用特权
    if (!AdjustTokenPrivileges(hToken, FALSE, &tkp, sizeof(tkp), NULL, NULL)) {
        CloseHandle(hToken);
        return false;
    }

    // 5. 检查错误状态
    if (GetLastError() == ERROR_NOT_ALL_ASSIGNED) {
        CloseHandle(hToken);
        return false;
    }

    CloseHandle(hToken);
    return true;
}
