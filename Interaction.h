#pragma once
#include <Windows.h>
/**
 * 连续发送ANSI字符串
 * 
 * @param str   要发送的ANSI字符串
 * @param times 重复发送次数
 */
void SendString(const char* str, int times) {
    if (!str || times <= 0) return;
    
    // 1. 计算需要的事件数量（每个字符按下+释放 * 次数）
    size_t len = strlen(str);
    size_t totalEvents = len * times * 2;
    if (totalEvents == 0) return;
    
    // 2. 创建输入事件数组
    INPUT* inputs = new INPUT[totalEvents];
    memset(inputs, 0, sizeof(INPUT) * totalEvents);
    
    // 3. 填充输入事件
    for (int t = 0; t < times; t++) {
        for (size_t i = 0; i < len; i++) {
            size_t index = (t * len * 2) + (i * 2);
            
            // 按键按下事件
            inputs[index].type = INPUT_KEYBOARD;
            inputs[index].ki.wScan = str[i];  // 直接使用字符值
            inputs[index].ki.dwFlags = KEYEVENTF_UNICODE;
            
            // 按键释放事件
            inputs[index + 1].type = INPUT_KEYBOARD;
            inputs[index + 1].ki.wScan = str[i];
            inputs[index + 1].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
        }
    }
    
    // 4. 发送所有输入事件
    SendInput(static_cast<UINT>(totalEvents), inputs, sizeof(INPUT));
    
    // 5. 清理资源
    delete[] inputs;
}

/**
 * @brief 发送全局按键事件
 * @param vkCode 虚拟键码（如VK_RETURN表示回车键）
 * 
 * 功能：向系统全局发送按键事件，无论哪个窗口处于焦点状态
 */
void SendKey(WORD vkCode) {
    // 创建按键事件序列
    INPUT inputs[2] = {};
    
    // 按键按下事件
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = vkCode;         // 指定虚拟键码
    inputs[0].ki.dwFlags = 0;          // 按下标志
    
    // 按键释放事件
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = vkCode;
    inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;  // 释放标志

    // 发送全局输入事件
    SendInput(2, inputs, sizeof(INPUT));
}