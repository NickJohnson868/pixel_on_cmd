#include <iostream>
#include <filesystem>
#include <windows.h>

#include "image.hpp"

#define IMAGE_FILE_PATH ".\\image\\2.png"
#define VIDEO_FILE_PATH ".\\video\\24fps.mp4"

#define SAFE_DELETE(obj) if (obj) { delete obj; obj = nullptr; }

CVideo* video = nullptr;
CImage* image = nullptr;

void close_player()
{
    SAFE_DELETE(video);
    SAFE_DELETE(image);
}

// 控制处理器函数
BOOL WINAPI ConsoleCtrlHandler(DWORD dwCtrlType) {
    switch (dwCtrlType) {
    case CTRL_C_EVENT:
        system("cls");
        reset_screen(192, 20);
        std::cout << "检测到CTRL+C事件" << std::endl;
        close_player();
        return TRUE;
    case CTRL_BREAK_EVENT:
        std::cout << "检测到CTRL+BREAK事件" << std::endl;
        break;
    case CTRL_CLOSE_EVENT:
        system("cls");
        reset_screen(192, 20);
        std::cout << "检测到控制台关闭事件" << std::endl;
        close_player();
        return TRUE;  // 返回TRUE表示已经处理了该事件
    case CTRL_LOGOFF_EVENT:
        std::cout << "检测到用户注销事件" << std::endl;
        break;
    case CTRL_SHUTDOWN_EVENT:
        std::cout << "检测到系统关机事件" << std::endl;
        break;
    default:
        return FALSE;  // 未处理的事件
    }
    return FALSE;  // 其他事件不处理
}

void play_image(int maxSize = 192, int font = 2)
{
    image = new CImage;
    image->read_img(get_exe_dir().append(IMAGE_FILE_PATH), maxSize);
    system("cls");
    reset_screen(maxSize, font);
    image->display();
    SAFE_DELETE(image);
}

void play_video(int maxSize = 192, int font = 16)
{
    set_cursor_info(false);
    video = new CVideo;
    video->read_video(get_exe_dir().append(VIDEO_FILE_PATH), maxSize);
    system("cls");
    reset_screen(maxSize, font);
    video->display();
    set_cursor_info(true);
    SAFE_DELETE(video);
}

int main()
{
    if (enable_vt_mode() < 0) {
        std::cerr << "终端不支持ANSI转义序列." << std::endl;
        return -1;
    }

    if (!SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE)) {
        std::cerr << "无法设置控制处理器: " << GetLastError() << std::endl;
        return -1;
    }

    cv::setNumThreads(std::thread::hardware_concurrency());

    //for (int i = 1; i <= 8; i *= 2)
    //{
    //    int font = 16;
    //    play_image(192 * i, font / i);
    //    Sleep(1000);
    //}
    
    system("cls");
    reset_screen(192, 20);
    play_video(192 / 2, 32);

    system("pause");
    close_player();

    return 0;
}