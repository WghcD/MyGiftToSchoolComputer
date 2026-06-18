#pragma once
#include <Windows.h>
  
/**  
 * 创建并配置 Windows 服务  
 *  
 * @param lpServiceName      服务名称（唯一标识）  
 * @param lpDisplayName      服务显示名称  
 * @param lpDescription      服务描述信息  
 * @param lpBinaryPath       服务程序完整路径  
 * @param dwStartType        启动类型（如 SERVICE_AUTO_START）  
 * @return 成功返回 true，失败返回 false  
 */  
bool CreateWindowsService(  
    LPCSTR lpServiceName,  
    LPCSTR lpDisplayName,  
    LPCSTR lpDescription,  
    LPCSTR lpBinaryPath,  
    DWORD dwStartType  
) {  
    SC_HANDLE hSCManager = OpenSCManagerA(  
        NULL,                   // 本地计算机  
        NULL,                   // 默认数据库  
        SC_MANAGER_CREATE_SERVICE // 创建服务权限  
    );  
    if (!hSCManager) return false;  
  
    // 创建服务核心逻辑  
    SC_HANDLE hService = CreateServiceA(  
        hSCManager,  
        lpServiceName,  
        lpDisplayName,  
        SERVICE_ALL_ACCESS,      // 完全控制权限  
        SERVICE_WIN32_OWN_PROCESS, // 独立进程类型  
        dwStartType,             // 启动类型  
        SERVICE_ERROR_NORMAL,    // 错误处理级别  
        lpBinaryPath,            // 可执行文件路径  
        NULL, NULL, NULL, NULL, NULL  
    );  
    if (!hService) {  
        CloseServiceHandle(hSCManager);  
        return false;  
    }  
  
    // 设置服务描述信息  
    SERVICE_DESCRIPTIONA sd = { const_cast<LPSTR>(lpDescription) };  
    ChangeServiceConfig2A(hService, SERVICE_CONFIG_DESCRIPTION, &sd);  
  
    // 清理资源  
    CloseServiceHandle(hService);  
    CloseServiceHandle(hSCManager);  
    return true;  
}  
  
/* 使用示例 */  
/*
int main() {  
    bool success = CreateWindowsService(  
        "MyCustomService",          // 服务名  
        "高级数据同步服务",          // 显示名  
        "负责实时同步企业级数据库",  // 描述  
        "C:\\MyApp\\service.exe",    // 程序路径  
        SERVICE_AUTO_START           // 自动启动  
    );  
  
    if (success) {  
        MessageBoxA(NULL, "服务创建成功", "操作完成", MB_OK);  
    } else {  
        MessageBoxA(NULL, "服务创建失败", "错误", MB_ICONERROR);  
    }  
    return 0;  
}  
*/