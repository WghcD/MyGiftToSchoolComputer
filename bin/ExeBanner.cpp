#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <cstdio>
#include <rpc.h>                // for UuidCreate, UuidToStringA, RpcStringFreeA

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "rpcrt4.lib")

//f:\programm\tdm-gcc-64\bin\g++ "E:\Data\Project\万花劫持器\bin\ExeBanner.cpp" -o "E:\Data\Project\万花劫持器\bin\ExeBanner.exe" -lrpcrt4

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

// 4. AppLocker – 通过 PowerShell 导入拒绝规则
bool BlockViaAppLocker(const std::string& filePath) {
    std::string guid = GenerateGuid();
    std::string tempFile = "applocker_policy_temp.xml";

    std::string xml = "<AppLockerPolicy Version=\"1\">\n";
    xml += "  <RuleCollection Type=\"Exe\" EnforcementMode=\"Enabled\">\n";
    xml += "    <FilePathRule Id=\"" + guid + "\" Name=\"Block " + GetFileName(filePath) + "\" Description=\"\" UserOrGroupSid=\"S-1-1-0\" Action=\"Deny\">\n";
    xml += "      <Conditions>\n";
    xml += "        <FilePathCondition Path=\"" + filePath + "\" />\n";
    xml += "      </Conditions>\n";
    xml += "    </FilePathRule>\n";
    xml += "  </RuleCollection>\n";
    xml += "</AppLockerPolicy>";

    // 写入临时文件（ANSI编码）
    HANDLE hFile = CreateFileA(tempFile.c_str(), GENERIC_WRITE, 0, NULL,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        std::cerr << "[AppLocker] 无法创建临时策略文件" << std::endl;
        return false;
    }
    DWORD written;
    WriteFile(hFile, xml.c_str(), (DWORD)xml.length(), &written, NULL);
    CloseHandle(hFile);

    // 构造 PowerShell 命令
    std::string psCommand = "powershell.exe -NoProfile -ExecutionPolicy Bypass -Command \"Set-AppLockerPolicy -XMLPolicy (Get-Content -Path '";
    psCommand += tempFile;
    psCommand += "' -Raw)\"";

    // 执行命令
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    BOOL success = CreateProcessA(NULL, &psCommand[0], NULL, NULL, FALSE,
        CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    if (success) {
        WaitForSingleObject(pi.hProcess, 15000);
        DWORD exitCode;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        // 删除临时文件
        DeleteFileA(tempFile.c_str());

        if (exitCode == 0) {
            std::cout << "[AppLocker] 已添加拒绝规则（可能覆盖了现有策略）" << std::endl;
            return true;
        } else {
            std::cerr << "[AppLocker] PowerShell 执行失败，退出代码: " << exitCode << std::endl;
            return false;
        }
    } else {
        std::cerr << "[AppLocker] 无法启动 PowerShell" << std::endl;
        DeleteFileA(tempFile.c_str());
        return false;
    }
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