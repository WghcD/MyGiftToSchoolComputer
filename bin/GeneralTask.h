#pragma once

#include"../process.h"

#include"../file.h"

void FileProtect(){
	for(int i=0;i<sizeof(allBinFile) / sizeof(allBinFile[0]);i++){
		if(!fileExist(string(string(BIN_PATH)+allBinFile[i]))||!fileExist(string(string(SECOND_BIN_PATH)+allBinFile[i]))){
			CopyFileToDirectory(addStrings(SECOND_BIN_PATH,allBinFile[i].c_str()),BIN_PATH);
			CopyFileToDirectory(addStrings(BIN_PATH,allBinFile[i].c_str()),SECOND_BIN_PATH);
		}
	}
	return;
}

bool SetDesktopWallpaper(const char* pngPath) {
    if (!pngPath || !*pngPath)
        return false;

    // 将 ANSI 路径转换为宽字符串 (SystemParametersInfoW 需要宽字符)
    int wlen = MultiByteToWideChar(CP_ACP, 0, pngPath, -1, NULL, 0);
    if (wlen <= 0) return false;

    WCHAR* wPath = new WCHAR[wlen];
    if (!wPath) return false;
    MultiByteToWideChar(CP_ACP, 0, pngPath, -1, wPath, wlen);

    // 设置壁纸：SPIF_UPDATEINIFILE 写入注册表，SPIF_SENDCHANGE 立即广播刷新
    BOOL ok = SystemParametersInfoW(SPI_SETDESKWALLPAPER, 0, wPath,
                                    SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);
    delete[] wPath;
    return ok != FALSE;
}

void SetFileProtectWithWinlogon(string path){
	string command="FileGuard.exe "+path;
	System(command);
}

void SetWallPaper(string path){
	SetDesktopWallpaper(path.c_str());
	string command="SetDesktopWallPaper.exe "+path;
	System(command);
}


void BanExe(string im){
	string command="ExeBanner.exe "+im;
	System(command);
}

DWORD WINAPI MonitorThread(LPVOID lpParam){
	OutputDebugStringA("Moniter Thread Started.");
	LPCWSTR A=ConvertToWideString(string(string(BIN_PATH)+string("expllorer.exe"))),B=ConvertToWideString(string(string(BIN_PATH)+string("ProgrammBanner.exe")));
	while(1){
		FileProtect();
		//SetDesktopWallpaper(WALLPAPER_PATH);
		if(!isProcessExist(getSinglePid("expllorer"))){LaunchAdminInteractiveProcess(A);}
		if(!isProcessExist(getSinglePid("ProgrammBanner"))){LaunchAdminInteractiveProcess(B);}
		Sleep(4000);
	}
	return 0;
}