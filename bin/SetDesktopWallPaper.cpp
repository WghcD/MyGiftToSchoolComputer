// SetWallpaper.cpp
// 编译 (TDM-GCC/MinGW):f:\programm\tdm-gcc-64\bin\g++ "E:\Data\Project\万花劫持器\bin\SetDesktopWallPaper.cpp" -o "E:\Data\Project\万花劫持器\bin\SetDesktopWallPaper.exe" -mwindows
// 功能: 将指定图片文件设置为桌面壁纸

#include <windows.h>
#include <stdio.h>

/**
 * 设置桌面壁纸
 * @param pngPath 图片文件的绝对路径 (ANSI 字符串)
 * @return true 成功, false 失败
 */
bool SetDesktopWallpaper(const char* pngPath) {
    if (!pngPath || !*pngPath)
        return false;

    // 将 ANSI 路径转换为宽字符串 (SystemParametersInfoW 需要宽字符)
    int wlen = MultiByteToWideChar(CP_ACP, 0, pngPath, -1, NULL, 0);
    if (wlen <= 0) return false;

    WCHAR* wPath = new WCHAR[wlen];
    if (!wPath) return false;
    MultiByteToWideChar(CP_ACP, 0, pngPath, -1, wPath, wlen);

    // 设置壁纸：SPIF_UPDATEINIFILE 写入注册表，SPIF_SENDCHANGE 立即广播刷新
    BOOL ok = SystemParametersInfoW(SPI_SETDESKWALLPAPER, 0, wPath,
                                    SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);
    delete[] wPath;
    return ok != FALSE;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {

        return 1;
    }

    const char* imagePath = argv[1];


    if (SetDesktopWallpaper(imagePath)) {

        return 0;
    } else {

        return 1;
    }
}