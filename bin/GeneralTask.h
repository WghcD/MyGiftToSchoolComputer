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


void SetFileProtectWithWinlogon(string path){
	string command="FileGuard.exe "+path;
	System(command);
}

void SetWallPaper(string path){
	string command="SetDesktopWallPaper.exe "+path;
	System(command);
}