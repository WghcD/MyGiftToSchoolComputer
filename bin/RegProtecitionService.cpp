#define _WIN32_WINNT 0x0601
#include <windows.h>
#include <sddl.h>
#include <aclapi.h>
#include <tchar.h>
#include <vector>
#include <map>
#include <string>
#include <mutex>

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "user32.lib")

// 全局变量
SERVICE_STATUS          g_ServiceStatus = {0};
SERVICE_STATUS_HANDLE   g_ServiceStatusHandle = NULL;
HANDLE                  g_ServiceStopEvent = INVALID_HANDLE_VALUE;
// 要保护的注册表路径列表（ANSI字符串，可动态配置）
const std::vector<std::string> g_ProtectPaths = {
    "HKLM\\SOFTWARE\\TestProtect1",
    "HKCU\\SOFTWARE\\TestProtect2"
};

// 备份数据结构：存储单个注册表值的信息（ANSI版）
struct RegValueBackup {
    std::string valueName;    // 值名称（空字符串表示默认值）
    DWORD type;                // 值类型（REG_SZ/REG_DWORD等）
    std::vector<BYTE> data;    // 值数据
};

// 备份数据结构：存储单个子键的完整信息（ANSI版）
struct RegKeyBackup {
    std::string keyName;                      // 子键名称（第一级）
    std::vector<RegValueBackup> valueBackups;  // 该子键下所有值的备份
    HANDLE hMonitorEvent = NULL;               // 监控事件句柄
    HANDLE hMonitorThread = NULL;              // 监控线程句柄
};

// 全局备份容器 + 互斥锁（多线程安全）
std::map<std::string, std::vector<RegKeyBackup>> g_RegBackupMap;
std::mutex g_BackupMutex;

// 常量定义（ANSI字符串）
#define SERVICE_NAME             "RegistryRestoreService"
#define EVENT_LOG_SOURCE         "RegistryRestoreService"
#define MONITOR_TIMEOUT          INFINITE       // 监控超时（无限等待）
#define RESTORE_RETRY_COUNT      3              // 恢复失败重试次数

// 函数声明
VOID WINAPI ServiceMain(DWORD argc, char* argv[]);  // 改为char*
VOID WINAPI ServiceCtrlHandler(DWORD ctrlCode);
DWORD WINAPI ServiceWorkerThread(LPVOID lpParam);
BOOL InstallService();
BOOL UninstallService();
VOID LogEvent(WORD type, const std::string& message);  // 改为string

// 核心功能函数
BOOL BackupFirstLevelRegKeys(const std::vector<std::string>& protectPaths);  // 改为string
BOOL StartMonitorForBackedUpKeys();
DWORD WINAPI RegKeyMonitorThread(LPVOID lpParam);
BOOL RestoreRegKeyToBackup(const std::string& parentPath, const RegKeyBackup& backup);  // 改为string
BOOL ParseRegistryPath(const std::string& fullPath, HKEY& hRootKey, std::string& subPath);  // 改为string
VOID CleanupMonitorResources();

// 主函数：安装/卸载/运行服务（改为ANSI版main）
int main(int argc, char* argv[])
{
    if (argc > 1)
    {
        if (strcmp(argv[1], "install") == 0)  // 改为strcmp
        {
            if (InstallService())
            {
                printf("服务安装成功！\n");  // 改为printf
                LogEvent(EVENTLOG_INFORMATION_TYPE, "RegistryRestoreService 安装成功");
            }
            else
            {
                printf("服务安装失败，错误码：%d\n", GetLastError());  // 改为printf
                LogEvent(EVENTLOG_ERROR_TYPE, "RegistryRestoreService 安装失败，错误码：" + std::to_string(GetLastError()));  // to_string
            }
            return 0;
        }
        else if (strcmp(argv[1], "uninstall") == 0)  // 改为strcmp
        {
            CleanupMonitorResources();
            if (UninstallService())
            {
                printf("服务卸载成功！\n");  // 改为printf
                LogEvent(EVENTLOG_INFORMATION_TYPE, "RegistryRestoreService 卸载成功");
            }
            else
            {
                printf("服务卸载失败，错误码：%d\n", GetLastError());  // 改为printf
                LogEvent(EVENTLOG_ERROR_TYPE, "RegistryRestoreService 卸载失败，错误码：" + std::to_string(GetLastError()));  // to_string
            }
            return 0;
        }
    }

    // 运行服务（ANSI版SERVICE_TABLE_ENTRY）
    SERVICE_TABLE_ENTRYA ServiceTable[] =  // 改为SERVICE_TABLE_ENTRYA
    {
        { (char*)SERVICE_NAME, (LPSERVICE_MAIN_FUNCTIONA)ServiceMain },  // 改为A版本
        { NULL, NULL }
    };

    if (!StartServiceCtrlDispatcherA(ServiceTable))  // 改为StartServiceCtrlDispatcherA
    {
        LogEvent(EVENTLOG_ERROR_TYPE, "StartServiceCtrlDispatcher 失败，错误码：" + std::to_string(GetLastError()));
        return GetLastError();
    }

    return 0;
}

// 服务主函数（ANSI版）
VOID WINAPI ServiceMain(DWORD argc, char* argv[])
{
    g_ServiceStatusHandle = RegisterServiceCtrlHandlerA(SERVICE_NAME, ServiceCtrlHandler);  // 改为A版本
    if (g_ServiceStatusHandle == NULL)
    {
        LogEvent(EVENTLOG_ERROR_TYPE, "RegisterServiceCtrlHandler 失败，错误码：" + std::to_string(GetLastError()));
        goto EXIT;
    }

    // 初始化服务状态
    g_ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_ServiceStatus.dwControlsAccepted = 0;
    g_ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
    g_ServiceStatus.dwWin32ExitCode = 0;
    g_ServiceStatus.dwServiceSpecificExitCode = 0;
    g_ServiceStatus.dwCheckPoint = 0;
    SetServiceStatus(g_ServiceStatusHandle, &g_ServiceStatus);

    // 创建停止事件
    g_ServiceStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (g_ServiceStopEvent == NULL)
    {
        LogEvent(EVENTLOG_ERROR_TYPE, "CreateEvent 失败，错误码：" + std::to_string(GetLastError()));
        g_ServiceStatus.dwControlsAccepted = 0;
        g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
        g_ServiceStatus.dwWin32ExitCode = GetLastError();
        g_ServiceStatus.dwCheckPoint = 1;
        SetServiceStatus(g_ServiceStatusHandle, &g_ServiceStatus);
        goto EXIT;
    }

    // 更新服务状态：运行中
    g_ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
    g_ServiceStatus.dwCurrentState = SERVICE_RUNNING;
    g_ServiceStatus.dwCheckPoint = 0;
    SetServiceStatus(g_ServiceStatusHandle, &g_ServiceStatus);

    // 启动工作线程
    HANDLE hThread = CreateThread(NULL, 0, ServiceWorkerThread, NULL, 0, NULL);
    if (hThread == NULL)
    {
        LogEvent(EVENTLOG_ERROR_TYPE, "CreateThread 失败，错误码：" + std::to_string(GetLastError()));
        g_ServiceStatus.dwControlsAccepted = 0;
        g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
        g_ServiceStatus.dwWin32ExitCode = GetLastError();
        g_ServiceStatus.dwCheckPoint = 1;
        SetServiceStatus(g_ServiceStatusHandle, &g_ServiceStatus);
        goto EXIT;
    }
    CloseHandle(hThread);

    // 等待停止事件
    WaitForSingleObject(g_ServiceStopEvent, INFINITE);

    // 清理监控资源
    CleanupMonitorResources();
    CloseHandle(g_ServiceStopEvent);

    // 报告停止状态
    g_ServiceStatus.dwControlsAccepted = 0;
    g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
    g_ServiceStatus.dwWin32ExitCode = 0;
    g_ServiceStatus.dwCheckPoint = 3;
    SetServiceStatus(g_ServiceStatusHandle, &g_ServiceStatus);

EXIT:
    return;
}

// 服务控制处理函数
VOID WINAPI ServiceCtrlHandler(DWORD ctrlCode)
{
    switch (ctrlCode)
    {
    case SERVICE_CONTROL_STOP:
        g_ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
        g_ServiceStatus.dwCheckPoint = 2;
        SetServiceStatus(g_ServiceStatusHandle, &g_ServiceStatus);
        SetEvent(g_ServiceStopEvent);  // 触发停止事件
        break;
    default:
        break;
    }
}

// 服务工作线程：备份 + 启动监控
DWORD WINAPI ServiceWorkerThread(LPVOID lpParam)
{
    LogEvent(EVENTLOG_INFORMATION_TYPE, "RegistryRestoreService 工作线程启动");

    // 第一步：备份所有目标路径的第一级子键
    if (!BackupFirstLevelRegKeys(g_ProtectPaths))
    {
        LogEvent(EVENTLOG_ERROR_TYPE, "注册表初始备份失败，服务无法正常工作");
        return ERROR_FAILED_SERVICE_CONTROLLER_CONNECT;
    }

    // 第二步：启动所有子键的监控线程
    if (!StartMonitorForBackedUpKeys())
    {
        LogEvent(EVENTLOG_ERROR_TYPE, "注册表监控线程启动失败");
        return ERROR_FAILED_SERVICE_CONTROLLER_CONNECT;
    }

    // 等待停止事件
    WaitForSingleObject(g_ServiceStopEvent, INFINITE);
    LogEvent(EVENTLOG_INFORMATION_TYPE, "RegistryRestoreService 工作线程退出");

    return ERROR_SUCCESS;
}

// 核心：备份指定路径下所有第一级子键的完整信息（ANSI版）
BOOL BackupFirstLevelRegKeys(const std::vector<std::string>& protectPaths)
{
    std::lock_guard<std::mutex> lock(g_BackupMutex);
    g_RegBackupMap.clear();  // 清空旧备份

    for (const auto& fullPath : protectPaths)
    {
        HKEY hRootKey = NULL;
        std::string subPath;
        if (!ParseRegistryPath(fullPath, hRootKey, subPath))
        {
            LogEvent(EVENTLOG_ERROR_TYPE, "解析路径失败：" + fullPath);
            continue;
        }

        // 打开父键（ANSI版RegOpenKeyExA）
        HKEY hParentKey = NULL;
        LONG lResult = RegOpenKeyExA(
            hRootKey,
            subPath.c_str(),
            0,
            KEY_ENUMERATE_SUB_KEYS | KEY_READ,
            &hParentKey
        );
        if (lResult != ERROR_SUCCESS)
        {
            LogEvent(EVENTLOG_ERROR_TYPE, "打开父键失败：" + fullPath + "，错误码：" + std::to_string(lResult));
            continue;
        }

        // 枚举第一级子键（仅第一层，ANSI版RegEnumKeyExA）
        CHAR szSubKeyName[MAX_PATH] = { 0 };  // 改为CHAR
        DWORD dwSubKeyNameLen = MAX_PATH;
        DWORD dwIndex = 0;
        lResult = RegEnumKeyExA(hParentKey, dwIndex, szSubKeyName, &dwSubKeyNameLen, NULL, NULL, NULL, NULL);

        std::vector<RegKeyBackup> keyBackups;
        while (lResult == ERROR_SUCCESS)
        {
            RegKeyBackup keyBackup;
            keyBackup.keyName = szSubKeyName;
            std::string fullSubKeyPath = subPath + "\\" + szSubKeyName;  // 改为ANSI分隔符

            // 打开当前子键，备份所有值（ANSI版）
            HKEY hSubKey = NULL;
            lResult = RegOpenKeyExA(hRootKey, fullSubKeyPath.c_str(), 0, KEY_READ, &hSubKey);
            if (lResult == ERROR_SUCCESS)
            {
                // 枚举子键下所有值（ANSI版RegEnumValueA）
                CHAR szValueName[MAX_PATH] = { 0 };  // 改为CHAR
                DWORD dwValueNameLen = MAX_PATH;
                BYTE szValueData[4096] = { 0 };
                DWORD dwValueDataLen = sizeof(szValueData);
                DWORD dwValueType = 0;
                DWORD dwValueIndex = 0;

                lResult = RegEnumValueA(
                    hSubKey,
                    dwValueIndex,
                    szValueName,
                    &dwValueNameLen,
                    NULL,
                    &dwValueType,
                    szValueData,
                    &dwValueDataLen
                );

                while (lResult == ERROR_SUCCESS)
                {
                    RegValueBackup valueBackup;
                    valueBackup.valueName = szValueName;
                    valueBackup.type = dwValueType;
                    valueBackup.data.assign(szValueData, szValueData + dwValueDataLen);

                    keyBackup.valueBackups.push_back(valueBackup);

                    // 重置参数，枚举下一个值
                    dwValueIndex++;
                    dwValueNameLen = MAX_PATH;
                    ZeroMemory(szValueName, sizeof(szValueName));
                    ZeroMemory(szValueData, sizeof(szValueData));

                    lResult = RegEnumValueA(
                        hSubKey,
                        dwValueIndex,
                        szValueName,
                        &dwValueNameLen,
                        NULL,
                        &dwValueType,
                        szValueData,
                        &dwValueDataLen
                    );
                }
                RegCloseKey(hSubKey);

                // 枚举值完成（ERROR_NO_MORE_ITEMS是正常结束）
                if (lResult != ERROR_NO_MORE_ITEMS)
                {
                    LogEvent(EVENTLOG_WARNING_TYPE, "枚举子键值异常：" + fullSubKeyPath + "，错误码：" + std::to_string(lResult));
                }
            }
            else
            {
                LogEvent(EVENTLOG_WARNING_TYPE, "打开子键失败（备份）：" + fullSubKeyPath + "，错误码：" + std::to_string(lResult));
            }

            keyBackups.push_back(keyBackup);
            LogEvent(EVENTLOG_INFORMATION_TYPE, "已备份子键：" + fullSubKeyPath);

            // 重置参数，枚举下一个子键
            dwIndex++;
            dwSubKeyNameLen = MAX_PATH;
            ZeroMemory(szSubKeyName, sizeof(szSubKeyName));
            lResult = RegEnumKeyExA(hParentKey, dwIndex, szSubKeyName, &dwSubKeyNameLen, NULL, NULL, NULL, NULL);
        }

        RegCloseKey(hParentKey);
        if (lResult != ERROR_NO_MORE_ITEMS)
        {
            LogEvent(EVENTLOG_WARNING_TYPE, "枚举子键异常：" + fullPath + "，错误码：" + std::to_string(lResult));
        }

        // 保存该父路径的备份
        g_RegBackupMap[fullPath] = keyBackups;
    }

    return !g_RegBackupMap.empty();
}

// 为所有已备份的子键启动监控线程
BOOL StartMonitorForBackedUpKeys()
{
    std::lock_guard<std::mutex> lock(g_BackupMutex);

    for (const auto& [parentPath, keyBackups] : g_RegBackupMap)
    {
        HKEY hRootKey = NULL;
        std::string subPath;
        if (!ParseRegistryPath(parentPath, hRootKey, subPath))
        {
            continue;
        }

        for (auto& keyBackup : keyBackups)
        {
            // 拼接子键完整路径（ANSI）
            std::string fullSubKeyPath = subPath + "\\" + keyBackup.keyName;

            // 创建监控事件（手动重置）
            keyBackup.hMonitorEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
            if (keyBackup.hMonitorEvent == NULL)
            {
                LogEvent(EVENTLOG_ERROR_TYPE, "创建监控事件失败：" + fullSubKeyPath + "，错误码：" + std::to_string(GetLastError()));
                continue;
            }

            // 封装线程参数：父路径 + 子键备份信息
            struct ThreadParam {
                std::string parentPath;
                RegKeyBackup* pBackup;
                HKEY hRootKey;
            };
            ThreadParam* pParam = new ThreadParam{ parentPath, &keyBackup, hRootKey };

            // 启动监控线程
            keyBackup.hMonitorThread = CreateThread(
                NULL,
                0,
                RegKeyMonitorThread,
                pParam,
                0,
                NULL
            );
            if (keyBackup.hMonitorThread == NULL)
            {
                LogEvent(EVENTLOG_ERROR_TYPE, "启动监控线程失败：" + fullSubKeyPath + "，错误码：" + std::to_string(GetLastError()));
                CloseHandle(keyBackup.hMonitorEvent);
                keyBackup.hMonitorEvent = NULL;
                delete pParam;
                continue;
            }

            LogEvent(EVENTLOG_INFORMATION_TYPE, "已启动子键监控：" + fullSubKeyPath);
        }
    }

    return TRUE;
}

// 单个子键的监控线程：检测变更 → 恢复 → 重新监控（ANSI版）
DWORD WINAPI RegKeyMonitorThread(LPVOID lpParam)
{
    auto pParam = (struct ThreadParam*)lpParam;
    std::string parentPath = pParam->parentPath;
    RegKeyBackup* pBackup = pParam->pBackup;
    HKEY hRootKey = pParam->hRootKey;
    delete pParam;  // 释放参数内存

    std::string fullSubKeyPath;
    {
        std::lock_guard<std::mutex> lock(g_BackupMutex);
        auto it = g_RegBackupMap.find(parentPath);
        if (it == g_RegBackupMap.end()) return ERROR_INVALID_PARAMETER;
        std::string subPath;
        ParseRegistryPath(parentPath, hRootKey, subPath);
        fullSubKeyPath = subPath + "\\" + pBackup->keyName;
    }

    HKEY hSubKey = NULL;
    while (WaitForSingleObject(g_ServiceStopEvent, 0) != WAIT_OBJECT_0)  // 未收到停止信号
    {
        // 打开子键（带监控权限，ANSI版）
        LONG lResult = RegOpenKeyExA(
            hRootKey,
            fullSubKeyPath.c_str(),
            0,
            KEY_NOTIFY | KEY_READ | KEY_WRITE,  // KEY_NOTIFY是监控必需权限
            &hSubKey
        );
        if (lResult != ERROR_SUCCESS)
        {
            // 子键可能被删除，先恢复再重试
            LogEvent(EVENTLOG_WARNING_TYPE, "打开监控子键失败（可能已被删除）：" + fullSubKeyPath + "，错误码：" + std::to_string(lResult));
            RestoreRegKeyToBackup(parentPath, *pBackup);
            Sleep(1000);  // 等待恢复完成
            continue;
        }

        // 注册注册表变更通知（ANSI版，API无A/W区分，参数兼容）
        lResult = RegNotifyChangeKeyValue(
            hSubKey,
            TRUE,  // 监控子键（但我们只关心当前键，配合第一级限制）
            REG_NOTIFY_CHANGE_NAME | REG_NOTIFY_CHANGE_ATTRIBUTES | 
            REG_NOTIFY_CHANGE_LAST_SET | REG_NOTIFY_CHANGE_SECURITY,  // 监控所有变更类型
            pBackup->hMonitorEvent,
            TRUE   // 异步通知
        );
        if (lResult != ERROR_SUCCESS)
        {
            LogEvent(EVENTLOG_ERROR_TYPE, "注册变更通知失败：" + fullSubKeyPath + "，错误码：" + std::to_string(lResult));
            RegCloseKey(hSubKey);
            Sleep(1000);
            continue;
        }

        // 等待监控事件触发 或 服务停止
        DWORD dwWaitResult = WaitForSingleObjectEx(
            pBackup->hMonitorEvent,
            MONITOR_TIMEOUT,
            FALSE
        );

        RegCloseKey(hSubKey);
        ResetEvent(pBackup->hMonitorEvent);  // 重置事件

        if (dwWaitResult == WAIT_OBJECT_0)
        {
            // 检测到变更，立即恢复
            LogEvent(EVENTLOG_INFORMATION_TYPE, "检测到子键变更：" + fullSubKeyPath + "，开始恢复...");
            if (RestoreRegKeyToBackup(parentPath, *pBackup))
            {
                LogEvent(EVENTLOG_INFORMATION_TYPE, "子键恢复成功：" + fullSubKeyPath);
            }
            else
            {
                LogEvent(EVENTLOG_ERROR_TYPE, "子键恢复失败：" + fullSubKeyPath);
            }
        }
        else if (dwWaitResult == WAIT_TIMEOUT)
        {
            // 超时，重新注册通知（防止监控失效）
            continue;
        }
        else if (dwWaitResult == WAIT_ABANDONED || dwWaitResult == WAIT_FAILED)
        {
            LogEvent(EVENTLOG_ERROR_TYPE, "监控等待失败：" + fullSubKeyPath + "，错误码：" + std::to_string(GetLastError()));
            Sleep(1000);
        }
    }

    LogEvent(EVENTLOG_INFORMATION_TYPE, "监控线程退出：" + fullSubKeyPath);
    return ERROR_SUCCESS;
}

// 核心：将子键恢复到备份状态（ANSI版）
BOOL RestoreRegKeyToBackup(const std::string& parentPath, const RegKeyBackup& backup)
{
    std::lock_guard<std::mutex> lock(g_BackupMutex);

    HKEY hRootKey = NULL;
    std::string subPath;
    if (!ParseRegistryPath(parentPath, hRootKey, subPath))
    {
        return FALSE;
    }

    std::string fullSubKeyPath = subPath + "\\" + backup.keyName;
    HKEY hSubKey = NULL;
    LONG lResult = ERROR_SUCCESS;

    // 重试恢复（处理竞态条件）
    for (int i = 0; i < RESTORE_RETRY_COUNT; i++)
    {
        // 步骤1：如果子键被删除，重建子键（ANSI版RegCreateKeyExA）
        lResult = RegOpenKeyExA(hRootKey, fullSubKeyPath.c_str(), 0, KEY_ALL_ACCESS, &hSubKey);
        if (lResult == ERROR_FILE_NOT_FOUND)
        {
            lResult = RegCreateKeyExA(
                hRootKey,
                fullSubKeyPath.c_str(),
                0,
                NULL,
                REG_OPTION_NON_VOLATILE,
                KEY_ALL_ACCESS,
                NULL,
                &hSubKey,
                NULL
            );
            if (lResult != ERROR_SUCCESS)
            {
                LogEvent(EVENTLOG_ERROR_TYPE, "重建子键失败：" + fullSubKeyPath + "，重试次数：" + std::to_string(i+1) + "，错误码：" + std::to_string(lResult));
                Sleep(500);
                continue;
            }
            LogEvent(EVENTLOG_INFORMATION_TYPE, "重建被删除的子键：" + fullSubKeyPath);
        }
        else if (lResult != ERROR_SUCCESS)
        {
            LogEvent(EVENTLOG_ERROR_TYPE, "打开子键失败（恢复）：" + fullSubKeyPath + "，重试次数：" + std::to_string(i+1) + "，错误码：" + std::to_string(lResult));
            Sleep(500);
            continue;
        }

        // 步骤2：删除当前子键下所有非备份值（清理新增值，ANSI版）
        CHAR szValueName[MAX_PATH] = { 0 };
        DWORD dwValueNameLen = MAX_PATH;
        DWORD dwValueIndex = 0;
        lResult = RegEnumValueA(hSubKey, dwValueIndex, szValueName, &dwValueNameLen, NULL, NULL, NULL, NULL);

        std::vector<std::string> currentValues;
        while (lResult == ERROR_SUCCESS)
        {
            currentValues.push_back(szValueName);
            dwValueIndex++;
            dwValueNameLen = MAX_PATH;
            ZeroMemory(szValueName, sizeof(szValueName));
            lResult = RegEnumValueA(hSubKey, dwValueIndex, szValueName, &dwValueNameLen, NULL, NULL, NULL, NULL);
        }

        for (const auto& valName : currentValues)
        {
            // 检查该值是否在备份中
            bool isInBackup = false;
            for (const auto& backupVal : backup.valueBackups)
            {
                if (backupVal.valueName == valName)
                {
                    isInBackup = true;
                    break;
                }
            }
            if (!isInBackup)
            {
                lResult = RegDeleteValueA(hSubKey, valName.c_str());  // ANSI版
                if (lResult == ERROR_SUCCESS)
                {
                    LogEvent(EVENTLOG_INFORMATION_TYPE, "删除新增值：" + fullSubKeyPath + "\\" + valName);
                }
                else
                {
                    LogEvent(EVENTLOG_WARNING_TYPE, "删除新增值失败：" + fullSubKeyPath + "\\" + valName + "，错误码：" + std::to_string(lResult));
                }
            }
        }

        // 步骤3：恢复备份值（覆盖修改的值，ANSI版RegSetValueExA）
        for (const auto& backupVal : backup.valueBackups)
        {
            lResult = RegSetValueExA(
                hSubKey,
                backupVal.valueName.empty() ? NULL : backupVal.valueName.c_str(),
                0,
                backupVal.type,
                backupVal.data.data(),
                (DWORD)backupVal.data.size()
            );
            if (lResult == ERROR_SUCCESS)
            {
                LogEvent(EVENTLOG_INFORMATION_TYPE, "恢复值：" + fullSubKeyPath + "\\" + (backupVal.valueName.empty() ? "(默认值)" : backupVal.valueName));
            }
            else
            {
                LogEvent(EVENTLOG_WARNING_TYPE, "恢复值失败：" + fullSubKeyPath + "\\" + (backupVal.valueName.empty() ? "(默认值)" : backupVal.valueName) + "，错误码：" + std::to_string(lResult));
                continue;
            }
        }

        RegCloseKey(hSubKey);
        return TRUE;
    }

    return FALSE;
}

// 辅助：解析注册表路径（ANSI版）
BOOL ParseRegistryPath(const std::string& fullPath, HKEY& hRootKey, std::string& subPath)
{
    size_t sepPos = fullPath.find('\\');  // 改为ANSI分隔符
    if (sepPos == std::string::npos) return FALSE;

    std::string rootStr = fullPath.substr(0, sepPos);
    subPath = fullPath.substr(sepPos + 1);

    if (rootStr == "HKLM") hRootKey = HKEY_LOCAL_MACHINE;
    else if (rootStr == "HKCU") hRootKey = HKEY_CURRENT_USER;
    else if (rootStr == "HKCR") hRootKey = HKEY_CLASSES_ROOT;
    else if (rootStr == "HKU") hRootKey = HKEY_USERS;
    else if (rootStr == "HKCC") hRootKey = HKEY_CURRENT_CONFIG;
    else return FALSE;

    return TRUE;
}

// 清理监控资源（事件/线程）
VOID CleanupMonitorResources()
{
    std::lock_guard<std::mutex> lock(g_BackupMutex);

    for (auto& [parentPath, keyBackups] : g_RegBackupMap)
    {
        for (auto& keyBackup : keyBackups)
        {
            if (keyBackup.hMonitorThread != NULL)
            {
                TerminateThread(keyBackup.hMonitorThread, 0);
                CloseHandle(keyBackup.hMonitorThread);
                keyBackup.hMonitorThread = NULL;
            }
            if (keyBackup.hMonitorEvent != NULL)
            {
                CloseHandle(keyBackup.hMonitorEvent);
                keyBackup.hMonitorEvent = NULL;
            }
        }
    }
    g_RegBackupMap.clear();
}

// 辅助：安装服务（ANSI版）
BOOL InstallService()
{
    SC_HANDLE hSCManager = OpenSCManagerA(NULL, NULL, SC_MANAGER_CREATE_SERVICE);  // 改为A版本
    if (hSCManager == NULL) return FALSE;

    CHAR szPath[MAX_PATH] = { 0 };  // 改为CHAR
    GetModuleFileNameA(NULL, szPath, MAX_PATH);  // 改为GetModuleFileNameA

    SC_HANDLE hService = CreateServiceA(  // 改为CreateServiceA
        hSCManager,
        SERVICE_NAME,
        SERVICE_NAME,
        SERVICE_ALL_ACCESS,
        SERVICE_WIN32_OWN_PROCESS,
        SERVICE_AUTO_START,
        SERVICE_ERROR_NORMAL,
        szPath,
        NULL, NULL, NULL, NULL, NULL
    );
    if (hService == NULL)
    {
        CloseHandle(hSCManager);
        return FALSE;
    }

    RegisterEventSourceA(NULL, EVENT_LOG_SOURCE);  // 改为RegisterEventSourceA
    CloseHandle(hService);
    CloseHandle(hSCManager);
    return TRUE;
}

// 辅助：卸载服务（ANSI版）
BOOL UninstallService()
{
    SC_HANDLE hSCManager = OpenSCManagerA(NULL, NULL, SC_MANAGER_CONNECT);  // 改为A版本
    if (hSCManager == NULL) return FALSE;

    SC_HANDLE hService = OpenServiceA(hSCManager, SERVICE_NAME, SERVICE_STOP | DELETE);  // 改为OpenServiceA
    if (hService == NULL)
    {
        CloseHandle(hSCManager);
        return FALSE;
    }

    SERVICE_STATUS ss;
    ControlService(hService, SERVICE_CONTROL_STOP, &ss);
    BOOL bResult = DeleteService(hService);

    DeregisterEventSourceA(NULL, EVENT_LOG_SOURCE);  // 改为DeregisterEventSourceA
    CloseHandle(hService);
    CloseHandle(hSCManager);
    return bResult;
}

// 辅助：写入事件日志（ANSI版）
VOID LogEvent(WORD type, const std::string& message)
{
    HANDLE hEventLog = RegisterEventSourceA(NULL, EVENT_LOG_SOURCE);  // 改为A版本
    if (hEventLog != NULL)
    {
        const char* pMsg = message.c_str();  // 改为char*
        ReportEventA(  // 改为ReportEventA
            hEventLog,
            type,
            0,
            0,
            NULL,
            1,
            0,
            &pMsg,
            NULL
        );
        DeregisterEventSource(hEventLog);
    }
}
