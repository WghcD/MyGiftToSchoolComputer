#include <windows.h>
#include <wbemidl.h>
#include <string>
#include <iostream>
//f:\programm\tdm-gcc-64\bin\g++ "E:\Data\Project\万花劫持器\bin\WMIProtect.cpp" -o "E:\Data\Project\ 万花劫持器\bin\WMIProtect.exe" -lwbemuuid -lole32 -loleaut32
#pragma comment(lib, "wbemuuid.lib")

// 辅助函数：将 wstring 转换为 BSTR
inline BSTR MakeBSTR(const std::wstring& str) {
    return SysAllocString(str.c_str());
}

// 安全释放 COM 接口
#define SAFE_RELEASE(p) { if (p) { (p)->Release(); (p) = nullptr; } }

/**
 * 创建 WMI 永久事件订阅，保护指定进程（被杀后自动重启）
 *
 * @param processName 进程名（如 L"notepad.exe"）
 * @param processPath 完整可执行路径（如 L"C:\\Windows\\System32\\notepad.exe"）
 * @return true 成功，false 失败
 */
bool CreateProcessProtectionSubscription(const std::wstring& processName,
                                         const std::wstring& processPath) {
    HRESULT hres;
    IWbemLocator* pLoc = nullptr;
    IWbemServices* pSvc = nullptr;
    IWbemClassObject* pFilterClass = nullptr;
    IWbemClassObject* pFilterInst = nullptr;
    IWbemClassObject* pConsumerClass = nullptr;
    IWbemClassObject* pConsumerInst = nullptr;
    IWbemClassObject* pBindingClass = nullptr;
    IWbemClassObject* pBindingInst = nullptr;

    // 1. 初始化 COM
    hres = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (FAILED(hres)) {
        std::cerr << "CoInitializeEx failed. Error: 0x" << std::hex << hres << std::endl;
        return false;
    }

    // 2. 初始化 COM 安全
    hres = CoInitializeSecurity(
        nullptr, -1, nullptr, nullptr,
        RPC_C_AUTHN_LEVEL_DEFAULT,
        RPC_C_IMP_LEVEL_IMPERSONATE,
        nullptr, EOAC_NONE, nullptr
    );
    if (FAILED(hres) && hres != RPC_E_TOO_LATE) {
        std::cerr << "CoInitializeSecurity failed. Error: 0x" << std::hex << hres << std::endl;
        CoUninitialize();
        return false;
    }

    // 3. 创建 WMI 定位器
    hres = CoCreateInstance(CLSID_WbemLocator, 0,
                            CLSCTX_INPROC_SERVER,
                            IID_IWbemLocator,
                            (LPVOID*)&pLoc);
    if (FAILED(hres)) {
        std::cerr << "CoCreateInstance failed. Error: 0x" << std::hex << hres << std::endl;
        CoUninitialize();
        return false;
    }

    // 4. 连接到 root\subscription
    hres = pLoc->ConnectServer(
        MakeBSTR(L"root\\subscription"),
        nullptr, nullptr, nullptr,
        0, nullptr, nullptr,
        &pSvc
    );
    if (FAILED(hres)) {
        std::cerr << "ConnectServer failed. Error: 0x" << std::hex << hres << std::endl;
        SAFE_RELEASE(pLoc);
        CoUninitialize();
        return false;
    }

    // 5. 设置代理安全
    hres = CoSetProxyBlanket(
        pSvc,
        RPC_C_AUTHN_WINNT,
        RPC_C_AUTHZ_NONE,
        nullptr,
        RPC_C_AUTHN_LEVEL_CALL,
        RPC_C_IMP_LEVEL_IMPERSONATE,
        nullptr,
        EOAC_NONE
    );
    if (FAILED(hres)) {
        std::cerr << "CoSetProxyBlanket failed. Error: 0x" << std::hex << hres << std::endl;
        SAFE_RELEASE(pSvc);
        SAFE_RELEASE(pLoc);
        CoUninitialize();
        return false;
    }

    // ================= 创建事件过滤器 =================
    std::wstring query = L"SELECT * FROM __InstanceDeletionEvent WITHIN 5 "
                         L"WHERE TargetInstance ISA 'Win32_Process' "
                         L"AND TargetInstance.Name = '";
    query += processName + L"'";

    hres = pSvc->GetObject(MakeBSTR(L"__EventFilter"), 0, nullptr, &pFilterClass, nullptr);
    if (FAILED(hres)) {
        std::cerr << "GetObject __EventFilter failed. Error: 0x" << std::hex << hres << std::endl;
        SAFE_RELEASE(pSvc);
        SAFE_RELEASE(pLoc);
        CoUninitialize();
        return false;
    }

    hres = pFilterClass->SpawnInstance(0, &pFilterInst);
    if (FAILED(hres)) {
        std::cerr << "SpawnInstance filter failed. Error: 0x" << std::hex << hres << std::endl;
        SAFE_RELEASE(pFilterClass);
        SAFE_RELEASE(pSvc);
        SAFE_RELEASE(pLoc);
        CoUninitialize();
        return false;
    }

    VARIANT var;
    VariantInit(&var);

    var.vt = VT_BSTR;
    var.bstrVal = MakeBSTR(L"Windows_System_Event_Filter");
    pFilterInst->Put(L"Name", 0, &var, 0);

    VariantClear(&var);
    var.vt = VT_BSTR;
    var.bstrVal = MakeBSTR(query);
    pFilterInst->Put(L"Query", 0, &var, 0);

    VariantClear(&var);
    var.vt = VT_BSTR;
    var.bstrVal = MakeBSTR(L"WQL");
    pFilterInst->Put(L"QueryLanguage", 0, &var, 0);

    hres = pSvc->PutInstance(pFilterInst, WBEM_FLAG_CREATE_OR_UPDATE, nullptr, nullptr);
    if (FAILED(hres)) {
        std::cerr << "PutInstance filter failed. Error: 0x" << std::hex << hres << std::endl;
        VariantClear(&var);
        SAFE_RELEASE(pFilterInst);
        SAFE_RELEASE(pFilterClass);
        SAFE_RELEASE(pSvc);
        SAFE_RELEASE(pLoc);
        CoUninitialize();
        return false;
    }
    VariantClear(&var);

    // ================= 创建事件消费者 =================
    hres = pSvc->GetObject(MakeBSTR(L"CommandLineEventConsumer"), 0, nullptr, &pConsumerClass, nullptr);
    if (FAILED(hres)) {
        std::cerr << "GetObject CommandLineEventConsumer failed. Error: 0x" << std::hex << hres << std::endl;
        SAFE_RELEASE(pFilterInst);
        SAFE_RELEASE(pFilterClass);
        SAFE_RELEASE(pSvc);
        SAFE_RELEASE(pLoc);
        CoUninitialize();
        return false;
    }

    hres = pConsumerClass->SpawnInstance(0, &pConsumerInst);
    if (FAILED(hres)) {
        std::cerr << "SpawnInstance consumer failed. Error: 0x" << std::hex << hres << std::endl;
        SAFE_RELEASE(pConsumerClass);
        SAFE_RELEASE(pFilterInst);
        SAFE_RELEASE(pFilterClass);
        SAFE_RELEASE(pSvc);
        SAFE_RELEASE(pLoc);
        CoUninitialize();
        return false;
    }

    VariantInit(&var);
    var.vt = VT_BSTR;
    var.bstrVal = MakeBSTR(L"Windows_System_Restart_Consumer");
    pConsumerInst->Put(L"Name", 0, &var, 0);

    VariantClear(&var);
    var.vt = VT_BSTR;
    var.bstrVal = MakeBSTR(processPath);
    pConsumerInst->Put(L"CommandLineTemplate", 0, &var, 0);

    hres = pSvc->PutInstance(pConsumerInst, WBEM_FLAG_CREATE_OR_UPDATE, nullptr, nullptr);
    if (FAILED(hres)) {
        std::cerr << "PutInstance consumer failed. Error: 0x" << std::hex << hres << std::endl;
        VariantClear(&var);
        SAFE_RELEASE(pConsumerInst);
        SAFE_RELEASE(pConsumerClass);
        SAFE_RELEASE(pFilterInst);
        SAFE_RELEASE(pFilterClass);
        SAFE_RELEASE(pSvc);
        SAFE_RELEASE(pLoc);
        CoUninitialize();
        return false;
    }
    VariantClear(&var);

    // ================= 创建绑定 =================
    hres = pSvc->GetObject(MakeBSTR(L"__FilterToConsumerBinding"), 0, nullptr, &pBindingClass, nullptr);
    if (FAILED(hres)) {
        std::cerr << "GetObject __FilterToConsumerBinding failed. Error: 0x" << std::hex << hres << std::endl;
        SAFE_RELEASE(pConsumerInst);
        SAFE_RELEASE(pConsumerClass);
        SAFE_RELEASE(pFilterInst);
        SAFE_RELEASE(pFilterClass);
        SAFE_RELEASE(pSvc);
        SAFE_RELEASE(pLoc);
        CoUninitialize();
        return false;
    }

    hres = pBindingClass->SpawnInstance(0, &pBindingInst);
    if (FAILED(hres)) {
        std::cerr << "SpawnInstance binding failed. Error: 0x" << std::hex << hres << std::endl;
        SAFE_RELEASE(pBindingClass);
        SAFE_RELEASE(pConsumerInst);
        SAFE_RELEASE(pConsumerClass);
        SAFE_RELEASE(pFilterInst);
        SAFE_RELEASE(pFilterClass);
        SAFE_RELEASE(pSvc);
        SAFE_RELEASE(pLoc);
        CoUninitialize();
        return false;
    }

    // ---------- 关键修复：直接构造路径，不依赖 Get(__PATH) ----------
    std::wstring filterPath = L"\\\\.\\root\\subscription:__EventFilter.Name=\"Windows_System_Event_Filter\"";
    std::wstring consumerPath = L"\\\\.\\root\\subscription:CommandLineEventConsumer.Name=\"Windows_System_Restart_Consumer\"";

    VariantInit(&var);
    var.vt = VT_BSTR;
    var.bstrVal = SysAllocString(filterPath.c_str());
    pBindingInst->Put(L"Filter", 0, &var, 0);
    VariantClear(&var);

    var.vt = VT_BSTR;
    var.bstrVal = SysAllocString(consumerPath.c_str());
    pBindingInst->Put(L"Consumer", 0, &var, 0);
    VariantClear(&var);
    // ----------------------------------------------------------------

    // 保存绑定实例
    hres = pSvc->PutInstance(pBindingInst, WBEM_FLAG_CREATE_OR_UPDATE, nullptr, nullptr);
    if (FAILED(hres)) {
        std::cerr << "PutInstance binding failed. Error: 0x" << std::hex << hres << std::endl;
        SAFE_RELEASE(pBindingInst);
        SAFE_RELEASE(pBindingClass);
        SAFE_RELEASE(pConsumerInst);
        SAFE_RELEASE(pConsumerClass);
        SAFE_RELEASE(pFilterInst);
        SAFE_RELEASE(pFilterClass);
        SAFE_RELEASE(pSvc);
        SAFE_RELEASE(pLoc);
        CoUninitialize();
        return false;
    }

    // 清理
    SAFE_RELEASE(pBindingInst);
    SAFE_RELEASE(pBindingClass);
    SAFE_RELEASE(pConsumerInst);
    SAFE_RELEASE(pConsumerClass);
    SAFE_RELEASE(pFilterInst);
    SAFE_RELEASE(pFilterClass);
    SAFE_RELEASE(pSvc);
    SAFE_RELEASE(pLoc);
    CoUninitialize();

    std::wcout << L"Protection subscription created successfully for: " << processName << std::endl;
    return true;
}

// 使用示例
int main() {
    if (CreateProcessProtectionSubscription(L"notepad.exe", L"C:\\Windows\\System32\\notepad.exe")) {
        std::wcout << L"Process is now protected." << std::endl;
    } else {
        std::wcout << L"Failed to protect process." << std::endl;
    }
    return 0;
}