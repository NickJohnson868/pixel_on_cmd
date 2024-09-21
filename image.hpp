#pragma once
#include <filesystem>
#include <opencv2/opencv.hpp>
#include <stdexcept>
#include <sstream>
#include <string>
#include <vector>
#include <windows.h>
#include <mmsystem.h>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
}

#pragma comment(lib, "avformat.lib")
#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "swresample.lib")
#pragma comment(lib, "winmm.lib")

#pragma comment (lib, "opencv_world4100d.lib")

#define CHARACTER "#"
#define CHECK_TEMP_STRING	std::is_same<StringT, std::string>::value
#define CHECK_TEMP_WSTRING	std::is_same<StringT, std::wstring>::value

void print_progress_bar(int row, int total);
void set_cursor_info(bool visible);
void reset_screen(int max_size = 192, int font_size = 8);
int enable_vt_mode();
std::filesystem::path get_exe_path();
std::filesystem::path get_exe_dir();
std::string generate_uuid(bool b_upper=false, bool b_delimiter=false);
template <typename StringT>
static StringT replace_all(StringT str, const StringT& target, const StringT& dest)
{
    static_assert(CHECK_TEMP_STRING || CHECK_TEMP_WSTRING,
        "Invalid type for replace_all, only std::string and std::wstring are supported");

    size_t start_pos = 0;
    while ((start_pos = str.find(target, start_pos)) != StringT::npos) {
        str.replace(start_pos, target.length(), dest);
        start_pos += dest.length();
    }
    return str;
}

struct Pixel {
    unsigned char R, G, B;
};

class CImage {
public:
    CImage() : m_width(0), m_height(0) {}

    CImage(const std::filesystem::path& filePath) : m_width(0), m_height(0)
    {
        read_img(filePath);
    }

    CImage(cv::Mat& image) : m_width(0), m_height(0)
    {
        read_img(image);
    }

    virtual ~CImage()
    {

    }

    // 读取图像并处理
    void read_img(const std::filesystem::path& filePath, int maxSize = 192) {
        cv::Mat image = cv::imread(filePath.string(), cv::IMREAD_COLOR);
        if (image.empty()) {
            throw std::runtime_error("文件打开失败！");
        }
        read_img(image, maxSize);
    }

    void read_img(cv::Mat& image, int maxSize = 192) {
        compress_bitmap(image, maxSize, 1.0, 0.5);

        m_width = image.cols;
        m_height = image.rows;
        pixels.resize(m_width * m_height);

        for (int y = 0; y < m_height; ++y) {
            for (int x = 0; x < m_width; ++x) {
                cv::Vec3b color = image.at<cv::Vec3b>(y, x);
                pixels[x + y * m_width] = { color[2], color[1], color[0] };
            }
        }
    }

    void display() const 
    {
        if (pixels.size() != m_height * m_width) return;
        printf("\033[H");
        //setvbuf(stdout, NULL, _IOFBF, m_width * m_height * 3);
        std::string ss;
        for (int y = 0; y < m_height; ++y) {
            for (int x = 0; x < m_width; ++x) {
                const Pixel& pixel = pixels[x + y * m_width];
                ss += "\033[38;2;";
                ss += std::to_string(static_cast<int>(pixel.R));
                ss += ";";
                ss += std::to_string(static_cast<int>(pixel.G));
                ss += ";";
                ss += std::to_string(static_cast<int>(pixel.B));
                ss += "m" + std::string(CHARACTER);
            }
            ss += '\n';
        }
        ss += "\033[0m";
        printf("%s", ss.c_str());
        fflush(stdout);
    }

private:
    int m_width, m_height;
    std::vector<Pixel> pixels;

    void compress_bitmap(cv::Mat& image, const int maxSize, const double xScale, const double yScale) {
        if (image.empty()) return;

        int newWidth = static_cast<int>(image.cols * xScale);
        int newHeight = static_cast<int>(image.rows * yScale);

        if (newWidth > maxSize || newHeight > maxSize) {
            double ratio = std::max(newWidth, newHeight) / static_cast<double>(maxSize);
            newWidth = static_cast<int>(newWidth / ratio);
            newHeight = static_cast<int>(newHeight / ratio);
        }

        cv::resize(image, image, cv::Size(newWidth, newHeight));
    }
};

class CVideo {
public:
    CVideo() : m_frameWidth(0), m_frameHeight(0), m_fps(0) {}

    CVideo(const std::filesystem::path& filePath) : m_frameWidth(0), m_frameHeight(0), m_fps(0) {
        read_video(filePath);
    }

    virtual ~CVideo()
    {
        if (std::filesystem::exists(m_audio_path)) 
        {
            std::filesystem::remove(m_audio_path);
        }
    }

    void display()
    {
        play_sound();
        play_video();
    }

    void read_video(const std::filesystem::path& filePath, int maxSize = 192)
    {
        cv::VideoCapture cap(filePath.string());
        if (!cap.isOpened()) {
            throw std::runtime_error("视频打开失败！");
        }

        m_maxFrames = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_COUNT));
        m_frameWidth = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH));
        m_frameHeight = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT));
        m_fps = cap.get(cv::CAP_PROP_FPS);

        cv::Mat frame;
        int frameCount = 0;
        while (cap.read(frame)) {
            if (frame.empty()) {
                continue;
            }

            print_progress_bar(frameCount, m_maxFrames);
            CImage image;
            image.read_img(frame, maxSize);
            m_frames.push_back(image);

            frameCount++;
            if (frameCount >= m_maxFrames) {
                break;
            }
        }
        cap.release();

        m_audio_path = extract_audio(filePath);
    }

private:
    std::vector<CImage> m_frames;
    int m_frameWidth, m_frameHeight, m_maxFrames;
    double m_fps;
    std::filesystem::path m_audio_path;

    void play_video()
    {
        LARGE_INTEGER cpuFrequency = { 0 };  //CPU时钟频率
        QueryPerformanceFrequency(&cpuFrequency);  //获取CPU时钟频率
        LARGE_INTEGER start_time, time1, time2;
        QueryPerformanceCounter(&start_time); // 获取当前CPU时钟计数

        for (int i = 0; i < m_maxFrames; )
        {
            QueryPerformanceCounter(&time1);
            m_frames[i].display();
            QueryPerformanceCounter(&time2);
            double frame_time = ((double)time2.QuadPart - (double)time1.QuadPart) / (double)cpuFrequency.QuadPart;

            if (i % (int)m_fps == 0)
                printf("\033[0mfps: %3.2f,  frame: %d/%d", 1 / frame_time, i, m_maxFrames);

            double time_since_start = ((double)time2.QuadPart - (double)start_time.QuadPart) / (double)cpuFrequency.QuadPart;
            i = (int)(m_fps * time_since_start);
        }

        system("cls");
    }

    std::filesystem::path extract_audio(const std::filesystem::path& inputFile)
    {
        std::filesystem::path exe_dir = get_exe_dir();
        std::string uuid = exe_dir.string() + "\\" + generate_uuid() + ".mp3";
        std::string command = exe_dir.string() + "\\ffmpeg.exe -i " + inputFile.string() + " -q:a 0 -map a " + uuid;
        system(command.c_str());
        return uuid;
    }

    void play_sound()
    {
        std::wstring openCommand = L"open \"" + m_audio_path.wstring() + L"\" type mpegvideo alias myMP3";
        if (mciSendString(openCommand.c_str(), NULL, 0, NULL) != 0) {
            std::cerr << "Failed to open the MP3 file." << std::endl;
            return;
        }

        if (mciSendString(L"play myMP3", NULL, 0, NULL) != 0) {
            std::cerr << "Failed to play the MP3 file." << std::endl;
            mciSendString(L"close myMP3", NULL, 0, NULL);
            return;
        }

        // 停止并关闭音频文件
        //mciSendString(L"stop myMP3", NULL, 0, NULL);
        //mciSendString(L"close myMP3", NULL, 0, NULL);
    }
};

int enable_vt_mode() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return -1;

    DWORD dwMode = 0;
    if (!GetConsoleMode(hOut, &dwMode)) return -2;

    dwMode |= 0x0004;
    if (!SetConsoleMode(hOut, dwMode)) return -3;
    return 1;
}

void reset_screen(int max_size, int font_size) {
    HANDLE hConsoleOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hConsoleOutput == INVALID_HANDLE_VALUE) {
        std::cerr << "无法获取标准输出句柄" << std::endl;
        return;
    }

    CONSOLE_FONT_INFOEX fontInfo;
    fontInfo.cbSize = sizeof(CONSOLE_FONT_INFOEX);
    if (!GetCurrentConsoleFontEx(hConsoleOutput, FALSE, &fontInfo)) {
        std::cerr << "无法获取当前控制台字体信息" << std::endl;
        return;
    }

    fontInfo.dwFontSize.X = 0;
    fontInfo.dwFontSize.Y = font_size;

    if (!SetCurrentConsoleFontEx(hConsoleOutput, FALSE, &fontInfo)) {
        std::cerr << "无法设置新的控制台字体" << std::endl;
        return;
    }

    HWND consoleWindow = GetConsoleWindow();
    ShowWindow(consoleWindow, SW_MAXIMIZE);

    std::string s = "mode con cols=" + std::to_string(max_size) + " lines=" + std::to_string(max_size);
    system(s.data());
}

void set_cursor_info(bool visible) {
    HANDLE hConsoleOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO cursorInfo;
    cursorInfo.dwSize = 10;
    cursorInfo.bVisible = visible;
    SetConsoleCursorInfo(hConsoleOutput, &cursorInfo);
}

void print_progress_bar(int row, int total) {
    const int bar_width = 50;
    double progress = static_cast<double>(row) / total;
    printf("请稍等, 当前进度 [");
    int pos = static_cast<int>(bar_width * progress);

    for (int i = 0; i < bar_width; ++i) {
        if (i < pos) std::cout << "=";
        else if (i == pos) std::cout << ">";
        else std::cout << " ";
    }

    progress *= 100.0;
    if (progress - 100.0 > 0.001) progress = 100;
    printf("] %.2f%%\r", progress);
    fflush(stdout);
}

std::filesystem::path get_exe_path()
{
    wchar_t buffer[MAX_PATH];
    GetModuleFileNameW(NULL, buffer, MAX_PATH);
    std::filesystem::path path_ = std::wstring(buffer);
    return path_;
}

std::filesystem::path get_exe_dir()
{
    return get_exe_path().parent_path();
}

std::string generate_uuid(bool b_upper, bool b_delimiter)
{
    char buffer[64] = { 0 };
    GUID guid;
    CoCreateGuid(&guid);
    std::string format = "%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X";
    if (!b_upper) {
        format = replace_all<std::string>(format, "X", "x");
    }
    if (!b_delimiter) {
        format = replace_all<std::string>(format, "-", "");
    }
    _snprintf_s(buffer, sizeof(buffer),
        format.data(),
        guid.Data1, guid.Data2, guid.Data3,
        guid.Data4[0], guid.Data4[1], guid.Data4[2],
        guid.Data4[3], guid.Data4[4], guid.Data4[5],
        guid.Data4[6], guid.Data4[7]);
    return buffer;
}