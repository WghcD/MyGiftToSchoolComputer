#include <windows.h>
#include <winternl.h>
#include <iostream>
using namespace std;
typedef NTSTATUS(NTAPI* PNtOpenProcess)(
    _Out_ PHANDLE ProcessHandle,
    _In_ ACCESS_MASK DesiredAccess,
    _In_ POBJECT_ATTRIBUTES ObjectAttributes,
    _In_opt_ PCLIENT_ID ClientId);

HANDLE GetHandle(DWORD pid) {
    // 动态获取未挂钩的NtOpenProcess地址
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    PNtOpenProcess NtOpenProcess = (PNtOpenProcess)GetProcAddress(ntdll, "NtOpenProcess");
    
    if (!NtOpenProcess) {
        std::cout << "Failed to locate NtOpenProcess\n";
        return NULL;
    }

    // 构造参数
    OBJECT_ATTRIBUTES oa;
    InitializeObjectAttributes(&oa, NULL, 0, NULL, NULL);
    
    CLIENT_ID cid = { (HANDLE)pid, NULL };
    HANDLE hProcess = NULL;

    // 调用原始系统调用
    NTSTATUS status = NtOpenProcess(
        &hProcess,
        PROCESS_ALL_ACCESS,
        &oa,
        &cid);

    if (!NT_SUCCESS(status)) {
        std::cout << "NtOpenProcess failed: 0x" << std::hex << status << "\n";
        return NULL;
    }else{cout<<"Get handle by NtOpenProcess Successfully!\n";}

    return hProcess;
}


typedef NTSTATUS(NTAPI* PNtOpenThread)(
    PHANDLE ThreadHandle,
    ACCESS_MASK DesiredAccess,
    POBJECT_ATTRIBUTES ObjectAttributes,
    PCLIENT_ID ClientId
);

HANDLE GetThreadHandle(DWORD dwDesiredAccess, BOOL bInheritHandle, DWORD dwThreadId) {
    // 动态加载NTDLL
    HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
    if (!hNtdll) return NULL;

    // 获取NtOpenThread函数指针
    PNtOpenThread pNtOpenThread = (PNtOpenThread)GetProcAddress(hNtdll, "NtOpenThread");
    if (!pNtOpenThread) return NULL;

    // 初始化参数
    CLIENT_ID cid = { NULL, (HANDLE)dwThreadId };
    OBJECT_ATTRIBUTES oa = { sizeof(oa) };
    oa.Attributes = bInheritHandle ? OBJ_INHERIT : 0;

    // 调用NTAPI
    HANDLE hThread = NULL;
    NTSTATUS status = pNtOpenThread(&hThread, dwDesiredAccess, &oa, &cid);
    if (NT_SUCCESS(status)) {
        return hThread;
    }
    return NULL;
}