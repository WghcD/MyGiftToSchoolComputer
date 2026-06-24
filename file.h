#pragma once
#include <Windows.h>
#include <iostream>
#include <string>
#include <cctype>  // 用于路径分隔符检查

bool CopyFileToDirectory(const std::string& sourcePath, const std::string& targetDir) {
    // 1. 验证源文件是否存在
    if (GetFileAttributesA(sourcePath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        DWORD error = GetLastError();
        std::cerr << "错误：源文件不存在或不可访问 - " << sourcePath 
                  << " (错误代码: " << error << ")" << std::endl;
        return false;
    }

    // 2. 提取源文件名
    size_t lastSlash = sourcePath.find_last_of("\\/");
    if (lastSlash == std::string::npos) {
        std::cerr << "错误：无效的源文件路径格式" << std::endl;
        return false;
    }
    std::string fileName = sourcePath.substr(lastSlash + 1);
    
    // 3. 构建目标路径
    std::string targetPath = targetDir;
    if (!targetPath.empty()) {
        char lastChar = targetPath[targetPath.size() - 1];
        if (lastChar != '\\' && lastChar != '/') {
            targetPath += '\\';
        }
    }
    targetPath += fileName;

    // 4. 创建目标目录（如果不存在）
    if (GetFileAttributesA(targetDir.c_str()) == INVALID_FILE_ATTRIBUTES) {
        if (!CreateDirectoryA(targetDir.c_str(), NULL)) {
            DWORD error = GetLastError();
            std::cerr << "错误：无法创建目标目录 - " << targetDir 
                      << " (错误代码: " << error << ")" << std::endl;
            return false;
        }
    }

    // 5. 执行文件复制
    if (!CopyFileA(sourcePath.c_str(), targetPath.c_str(), FALSE)) {
        DWORD error = GetLastError();
        std::cerr << "错误：无法复制文件 - " << sourcePath 
                  << " 到 " << targetPath 
                  << " (错误代码: " << error << ")" << std::endl;
        return false;
    }


    return true;
}

bool CopyFileWithCommand(const std::string& sourcePath, const std::string& targetPath) {
    // 构造 copy 命令
    std::string command = "copy /Y \"" + sourcePath + "\" \"" + targetPath + "\"";
	
    cout<<command<<"\n";
    
    system(command.c_str());

}

// 将本程序的完整绝对路径写入指定文件（覆盖写入）
bool WriteSelfPathToFile(const std::string& filePath) {
    char buffer[MAX_PATH];
    // 获取当前可执行文件的完整路径（包含文件名）
    DWORD len = GetModuleFileNameA(NULL, buffer, MAX_PATH);
    if (len == 0 || len == MAX_PATH) {
        // 获取失败或缓冲区不足
        return false;
    }

    std::ofstream outFile(filePath, std::ios::trunc);
    if (!outFile.is_open()) {
        return false;
    }
    outFile << buffer;
    outFile.close();
    return true;
}

// 从指定文件中读取一行路径，返回为 std::string；若失败则返回空字符串
std::string ReadPathFromFile(const std::string& filePath) {
    std::ifstream inFile(filePath);
    if (!inFile.is_open()) {
        return "";
    }
    std::string path;
    std::getline(inFile, path);
    inFile.close();
    return path;
}

bool fileExist(const std::string& filePath) {
    DWORD attrib = GetFileAttributesA(filePath.c_str());
    return (attrib != INVALID_FILE_ATTRIBUTES) 
        && !(attrib & FILE_ATTRIBUTE_DIRECTORY);
}

// 使用示例
/*
int main() {
    std::string source = "C:\\Documents\\report.txt";
    std::string targetDir = "D:\\Backup\\Documents";
    
    if (CopyFileToDirectoryA(source, targetDir)) {
        std::cout << "操作成功完成" << std::endl;
    } else {
        std::cout << "操作失败" << std::endl;
    }
    return 0;
}*/
