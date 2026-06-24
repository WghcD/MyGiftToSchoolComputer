#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <cstdio>
#include <rpc.h>                // for UuidCreate, UuidToStringA, RpcStringFreeA

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "rpcrt4.lib")

//f:\programm\tdm-gcc-64\bin\g++ "E:\Data\Project\万花劫持器\bin\ExeBanner.cpp" -o "E:\Data\Project\万花劫持器\bin\ExeBanner.exe" -lrpcrt4 -mwindows

// 提取文件名（例如 "C:\\test\\app.exe" -> "app.exe"）
std::string GetFileName(const std::string& path) {
    size_t pos = path.find_last_of("\\/");
    if (pos != std::string::npos)
        return path.substr(pos + 1);
    return path;
}

// 生成随机GUID字符串（窄字符版本）
std::string GenerateGuid() {
    UUID uuid;
    if (UuidCreate(&uuid) != RPC_S_OK)
        return "{00000000-0000-0000-0000-000000000000}";

    unsigned char* guidStr = nullptr;
    if (UuidToStringA(&uuid, &guidStr) != RPC_S_OK)
        return "{00000000-0000-0000-0000-000000000000}";

    std::string result = reinterpret_cast<char*>(guidStr);
    RpcStringFreeA(&guidStr);
    return result;
}

// 检查是否以管理员权限运行
BOOL IsRunAsAdmin() {
    BOOL bElevated = FALSE;
    HANDLE hToken = NULL;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        TOKEN_ELEVATION elev;
        DWORD dwSize;
        if (GetTokenInformation(hToken, TokenElevation, &elev, sizeof(elev), &dwSize))
            bElevated = elev.TokenIsElevated;
        CloseHandle(hToken);
    }
    return bElevated;
}

// 1. 映像劫持
bool BlockViaImageHijack(const std::string& filePath) {
    std::string imageName = GetFileName(filePath);
    std::string keyPath = "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options\\" + imageName;
    HKEY hKey;
    LSTATUS status = RegCreateKeyExA(HKEY_LOCAL_MACHINE, keyPath.c_str(), 0, NULL,
        REG_OPTION_NON_VOLATILE, KEY_WOW64_64KEY | KEY_SET_VALUE, NULL, &hKey, NULL);
    if (status != ERROR_SUCCESS) {
        std::cerr << "[映像劫持] 创建注册表项失败，错误码: " << status << std::endl;
        return false;
    }

    // 设置一个无效路径
    const char* debuggerPath = "AUX";
    status = RegSetValueExA(hKey, "Debugger", 0, REG_SZ,
        (const BYTE*)debuggerPath, (DWORD)(strlen(debuggerPath) + 1));
    RegCloseKey(hKey);
    if (status != ERROR_SUCCESS) {
        std::cerr << "[映像劫持] 设置Debugger失败，错误码: " << status << std::endl;
        return false;
    }
    std::cout << "[映像劫持] 成功阻止 " << imageName << std::endl;
    return true;
}

// 2. DisallowRun 黑名单
bool BlockViaDisallowRun(const std::string& filePath) {
    std::string imageName = GetFileName(filePath);
    std::string policyKey = "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer";
    std::string disallowKey = policyKey + "\\DisallowRun";

    HKEY hKey;
    // 启用 DisallowRun 策略
    LSTATUS status = RegCreateKeyExA(HKEY_LOCAL_MACHINE, policyKey.c_str(), 0, NULL,
        REG_OPTION_NON_VOLATILE, KEY_WOW64_64KEY | KEY_SET_VALUE, NULL, &hKey, NULL);
    if (status != ERROR_SUCCESS) {
        std::cerr << "[DisallowRun] 打开策略键失败: " << status << std::endl;
        return false;
    }
    DWORD dwEnable = 1;
    status = RegSetValueExA(hKey, "DisallowRun", 0, REG_DWORD, (BYTE*)&dwEnable, sizeof(dwEnable));
    RegCloseKey(hKey);
    if (status != ERROR_SUCCESS) {
        std::cerr << "[DisallowRun] 启用策略失败: " << status << std::endl;
        return false;
    }

    // 创建 DisallowRun 子键并添加条目
    status = RegCreateKeyExA(HKEY_LOCAL_MACHINE, disallowKey.c_str(), 0, NULL,
        REG_OPTION_NON_VOLATILE, KEY_WOW64_64KEY | KEY_SET_VALUE | KEY_QUERY_VALUE, NULL, &hKey, NULL);
    if (status != ERROR_SUCCESS) {
        std::cerr << "[DisallowRun] 创建DisallowRun键失败: " << status << std::endl;
        return false;
    }

    // 找到下一个可用的编号
    int nextIndex = 1;
    char valueName[16];
    DWORD dataSize = 0;
    while (true) {
        sprintf_s(valueName, "%d", nextIndex);
        LSTATUS queryStatus = RegQueryValueExA(hKey, valueName, NULL, NULL, NULL, &dataSize);
        if (queryStatus == ERROR_FILE_NOT_FOUND)
            break;
        nextIndex++;
    }

    // 添加规则
    status = RegSetValueExA(hKey, valueName, 0, REG_SZ,
        (const BYTE*)imageName.c_str(), (DWORD)(imageName.length() + 1));
    RegCloseKey(hKey);
    if (status != ERROR_SUCCESS) {
        std::cerr << "[DisallowRun] 添加规则失败: " << status << std::endl;
        return false;
    }
    std::cout << "[DisallowRun] 已禁止 " << imageName << std::endl;
    return true;
}

// 3. SRP (软件限制策略) – 通过路径规则设置为“不允许”
bool BlockViaSRP(const std::string& filePath) {
    std::string srpBase = "SOFTWARE\\Policies\\Microsoft\\Windows\\Safer\\CodeIdentifiers";
    std::string pathsKey = srpBase + "\\0\\Paths";

    HKEY hKey;
    LSTATUS status = RegCreateKeyExA(HKEY_LOCAL_MACHINE, pathsKey.c_str(), 0, NULL,
        REG_OPTION_NON_VOLATILE, KEY_WOW64_64KEY | KEY_SET_VALUE, NULL, &hKey, NULL);
    if (status != ERROR_SUCCESS) {
        std::cerr << "[SRP] 创建Paths键失败: " << status << std::endl;
        return false;
    }

    // 使用GUID作为值名，值为目标路径
    std::string guid = GenerateGuid();
    status = RegSetValueExA(hKey, guid.c_str(), 0, REG_SZ,
        (const BYTE*)filePath.c_str(), (DWORD)(filePath.length() + 1));
    RegCloseKey(hKey);
    if (status != ERROR_SUCCESS) {
        std::cerr << "[SRP] 添加路径规则失败: " << status << std::endl;
        return false;
    }
    std::cout << "[SRP] 已将 " << filePath << " 设为不允许执行" << std::endl;
    return true;
}

// 辅助：将 ANSI（当前代码页，中文系统为 GBK）转换为 UTF?8
std::string AnsiToUtf8(const std::string& ansi) {
    // 第一步：ANSI → UTF?16 (宽字符)
    int wlen = MultiByteToWideChar(CP_ACP, 0, ansi.c_str(), -1, nullptr, 0);
    if (wlen == 0) return "";
    std::wstring wstr(wlen, L'\0');
    MultiByteToWideChar(CP_ACP, 0, ansi.c_str(), -1, &wstr[0], wlen);
    wstr.pop_back(); // 去除末尾的 L'\0'

    // 第二步：UTF?16 → UTF?8
    int u8len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (u8len == 0) return "";
    std::string utf8(u8len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &utf8[0], u8len, nullptr, nullptr);
    utf8.pop_back(); // 去除末尾的 '\0'
    return utf8;
}



// 在现有策略上追加拒绝规则（保留原有规则）
bool BlockViaAppLocker(const std::string& filePath) {/*
    std::string fileName = GetFileName(filePath);
    std::string pathUtf8 = AnsiToUtf8(filePath);
    std::string nameUtf8 = AnsiToUtf8(fileName);

    // 构建 PowerShell 脚本：获取现有策略 → 追加新规则 → 应用
    std::string psScript =
        "$ErrorActionPreference = 'Stop';\n"
        "$policy = Get-AppLockerPolicy -Local -ErrorAction SilentlyContinue;\n"
        "if ($policy -eq $null) {\n"
        "    # 无现有策略，创建初始策略：允许所有 + 拒绝目标\n"
        "    $allowRule = New-AppLockerRule -FilePathRule -Path '*' -Action Allow -User Everyone -Description 'Allow all';\n"
        "    $denyRule = New-AppLockerRule -FilePathRule -Path '" + pathUtf8 + "' -Action Deny -User Everyone -Description 'Block " + nameUtf8 + "';\n"
        "    $ruleCollection = New-AppLockerRuleCollection -RuleType Exe -Rule @($allowRule, $denyRule);\n"
        "    $policy = New-AppLockerPolicy -RuleCollection $ruleCollection;\n"
        "} else {\n"
        "    # 检查是否已有“允许所有”规则，若无则添加\n"
        "    $allowAll = $policy.RuleCollections[0].Rules | Where-Object { $_.Action -eq 'Allow' -and $_.Conditions[0].Path -eq '*' };\n"
        "    if ($allowAll -eq $null) {\n"
        "        $allowRule = New-AppLockerRule -FilePathRule -Path '*' -Action Allow -User Everyone -Description 'Allow all';\n"
        "        $policy.RuleCollections[0].Rules.Add($allowRule);\n"
        "    }\n"
        "    # 检查是否已存在相同路径的拒绝规则，若存在则跳过\n"
        "    $exist = $policy.RuleCollections[0].Rules | Where-Object { $_.Action -eq 'Deny' -and $_.Conditions[0].Path -eq '" + pathUtf8 + "' };\n"
        "    if ($exist -eq $null) {\n"
        "        $denyRule = New-AppLockerRule -FilePathRule -Path '" + pathUtf8 + "' -Action Deny -User Everyone -Description 'Block " + nameUtf8 + "';\n"
        "        $policy.RuleCollections[0].Rules.Add($denyRule);\n"
        "    }\n"
        "}\n"
        "Set-AppLockerPolicy -Policy $policy;\n";

    // 将脚本写入临时文件（UTF-8 with BOM）
    char tempPath[MAX_PATH];
    if (GetTempPathA(MAX_PATH, tempPath) == 0) {
        std::cerr << "[AppLocker] 无法获取临时目录" << std::endl;
        return false;
    }
    std::string scriptPath = std::string(tempPath) + "applocker_add.ps1";

    HANDLE hFile = CreateFileA(scriptPath.c_str(), GENERIC_WRITE, 0, NULL,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        std::cerr << "[AppLocker] 无法创建临时脚本文件" << std::endl;
        return false;
    }
    unsigned char bom[] = {0xEF, 0xBB, 0xBF};
    DWORD written;
    WriteFile(hFile, bom, sizeof(bom), &written, NULL);
    WriteFile(hFile, psScript.c_str(), (DWORD)psScript.size(), &written, NULL);
    CloseHandle(hFile);

    // 执行 PowerShell 脚本（使用 -File 参数）
    std::string psCommand = "powershell.exe -NoProfile -ExecutionPolicy Bypass -File \"" + scriptPath + "\"";
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    BOOL success = CreateProcessA(NULL, &psCommand[0], NULL, NULL, FALSE,
                                  NULL, NULL, NULL, &si, &pi);
    if (success) {
        WaitForSingleObject(pi.hProcess, 30000);  // 等待最多30秒
        DWORD exitCode;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        DeleteFileA(scriptPath.c_str());

        if (exitCode == 0) {
            std::cout << "[AppLocker] 已追加拒绝规则: " << fileName << std::endl;
            return true;
        } else {
            std::cerr << "[AppLocker] PowerShell 执行失败，退出代码: " << exitCode << std::endl;
            return false;
        }
    } else {
        std::cerr << "[AppLocker] 无法启动 PowerShell" << std::endl;
        DeleteFileA(scriptPath.c_str());
        return false;
    }*/
	return 1;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "用法: ExeBanner.exe <可执行文件完整路径>" << std::endl;
        std::cout << "示例: ExeBanner.exe \"C:\\Program Files\\MyApp\\app.exe\"" << std::endl;
        return 1;
    }

    std::string targetPath = argv[1];

    if (!IsRunAsAdmin()) {
        std::cout << "此程序需要管理员权限。请以管理员身份运行。" << std::endl;
        return 1;
    }

    std::cout << "开始阻止程序: " << targetPath << "\n" << std::endl;

    bool anySuccess = false;

    if (BlockViaImageHijack(targetPath)) anySuccess = true;
    if (BlockViaDisallowRun(targetPath)) anySuccess = true;
    if (BlockViaSRP(targetPath)) anySuccess = true;
    if (BlockViaAppLocker(targetPath)) anySuccess = true;

    if (anySuccess)
        std::cout << "\n部分或全部方法已应用。请重启或注销后使策略完全生效。" << std::endl;
    else
        std::cerr << "\n所有方法均失败，请检查权限和系统环境。" << std::endl;

    return anySuccess ? 0 : 1;
}