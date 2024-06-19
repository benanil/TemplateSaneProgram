
/********************************************************************
*    Purpose: Creating Window, Keyboard and Mouse input, Main Loop  *
*    Author : Anilcan Gulkaya 2023 anilcangulkaya7@gmail.com        *
********************************************************************/

#ifdef _WIN32

#ifndef NOMINMAX
#  define NOMINMAX
#  define WIN32_LEAN_AND_MEAN 
#  define VC_EXTRALEAN
#endif

#define MINIAUDIO_IMPLEMENTATION
#define MA_ENABLE_ONLY_SPECIFIC_BACKENDS
#define MA_NO_ENCODING /* read audio files only, for now */
#define MA_NO_GENERATION
#define MA_ENABLE_WASAPI
#include "../External/miniaudio.h"

#include <Windows.h>
#include <bitset>

#include "../ASTL/Array.hpp"
#include "../ASTL/Common.hpp"
#include "../ASTL/Algorithms.hpp"
#include "../External/glad.hpp"
#include "include/Platform.hpp"
// #include "../External/VMem.h"

#pragma comment (lib, "gdi32.lib")
#pragma comment (lib, "user32.lib")
#pragma comment (lib, "opengl32.lib")
#pragma comment (lib, "Shell32.lib")

struct PlatformContextWin
{
    // Callbacks
    void(*WindowMoveCallback)  (int, int);
    void(*WindowResizeCallback)(int, int);
    void(*MouseMoveCallback)   (float, float);
    void(*KeyPressCallback)    (unsigned);
    void(*FocusChangedCallback)(bool);
    
    // Window
    int WindowPosX;
    int WindowPosY;
    int WindowWidth;
    int WindowHeight;
    HWND hwnd;

    // Input Code, 128 bit bitmasks for key states.
    std::bitset<128> DownKeys;
    std::bitset<128> LastKeys;
    std::bitset<128> PressedKeys;
    std::bitset<128> ReleasedKeys;
    // Mouse
    int    MouseDown, MouseLast, MousePressed, MouseReleased;
    float  MousePosX, MousePosY;
    float  MouseWheelDelta;

    LONGLONG StartupTime;
    LONGLONG Frequency;
    double   DeltaTime;

    bool VSyncActive;
    bool ShouldClose;
    char* ClipboardString;
} PlatformCtx;  

StackArray<ma_sound, 16> mSounds={};
static char WindowName[64] = { 'S', 'a', 'n', 'e', 'E', 'n', 'g', 'i', 'n', 'e' };

void wSetFocusChangedCallback(void(*callback)(bool))         { PlatformCtx.FocusChangedCallback = callback; }
void wSetKeyPressCallback    (void(*callback)(unsigned))     { PlatformCtx.KeyPressCallback     = callback; }
void wSetMouseMoveCallback   (void(*callback)(float, float)) { PlatformCtx.MouseMoveCallback    = callback; }
void wSetWindowResizeCallback(void(*callback)(int, int))     { PlatformCtx.WindowResizeCallback = callback; }
void wSetWindowMoveCallback  (void(*callback)(int, int))     { PlatformCtx.WindowMoveCallback   = callback; }

void wRequestQuit() {
    PlatformCtx.ShouldClose = true;
}

void wGetWindowSize(int* x, int* y) { *x = PlatformCtx.WindowWidth;  *y = PlatformCtx.WindowHeight; }
void wGetWindowPos (int* x, int* y) { *x = PlatformCtx.WindowPosX;   *y = PlatformCtx.WindowPosY;   }

void wSetWindowSize(int width, int height)
{
    PlatformCtx.WindowWidth = width; PlatformCtx.WindowHeight = height;
    if (!PlatformCtx.hwnd) return;
    SetWindowPos(PlatformCtx.hwnd, nullptr, PlatformCtx.WindowPosX, PlatformCtx.WindowPosY, width, height, 0);
}

void wSetWindowPosition(int x, int y)
{
    PlatformCtx.WindowPosX = x; PlatformCtx.WindowPosY = y;
    if (!PlatformCtx.hwnd) return;
    SetWindowPos(PlatformCtx.hwnd, nullptr, x, y, PlatformCtx.WindowWidth, PlatformCtx.WindowHeight, 0);
}

void wSetWindowName(const char* name)
{
    SmallMemSet(WindowName, 0, sizeof(WindowName));
    
    for (int i = 0; *name && i < 64; i++)
    {
        WindowName[i] = *name++;
    }
    if (PlatformCtx.hwnd)
    {
        SetWindowText(PlatformCtx.hwnd, WindowName);
    }
}

void wGetMonitorSize(int* width, int* height)
{
    *width  = GetSystemMetrics(SM_CXSCREEN);
    *height = GetSystemMetrics(SM_CYSCREEN);
}

void wSetVSync(bool active)
{
    PlatformCtx.VSyncActive = active; 
}

// Forward declaration of ShellExecuteA function
extern "C" {
    typedef struct HWND__* HWND;
    typedef const char* LPCSTR;
    typedef int INT;

    __declspec(dllimport) HINSTANCE __stdcall ShellExecuteA(
        HWND hwnd,
        LPCSTR lpOperation,
        LPCSTR lpFile,
        LPCSTR lpParameters,
        LPCSTR lpDirectory,
        INT nShowCmd
    );
}

void wOpenURL(const char* url)
{
    ShellExecuteA(NULL, "open", url, NULL, NULL, SW_SHOWNORMAL);
}

bool wOpenFolder(const char* folderPath) 
{
    if ((size_t)ShellExecuteA(nullptr, "open", folderPath, nullptr, nullptr, SW_SHOWNORMAL) <= 32) 
        return false;
    return true;
}

//------------------------------------------------------------------------
// Audio

ma_engine maEngine;
ma_engine* GetMAEngine() {
    return &maEngine; 
}
int LoadSound(const char* path)
{
    mSounds.AddUninitialized(1);
    uint soundFlag = MA_SOUND_FLAG_NO_SPATIALIZATION | MA_SOUND_FLAG_NO_PITCH;
    ma_sound_init_from_file(GetMAEngine(), path, soundFlag, nullptr, nullptr, &mSounds.Back());
    return mSounds.Size()-1;
}

void SoundPlay(int sound)
{
    ma_sound_start(&mSounds[sound]);
}

void SoundRewind(ASound sound)
{
    ma_sound* soundPtr = &mSounds[sound];
    if (ma_sound_is_playing(soundPtr))
        ma_sound_seek_to_pcm_frame(soundPtr, 0);
}

void SoundSetVolume(ASound sound, float volume)
{
    ma_sound_set_volume(&mSounds[sound], volume);
}

void SoundDestroy(ASound sound)
{
    ma_sound_uninit(&mSounds[sound]);
}

/********************************************************************************/
/*                          Keyboard and Mouse Input                            */
/********************************************************************************/

inline bool GetBit128(const std::bitset<128>& bits, int idx) { return bits[idx]; }
inline void SetBit128(std::bitset<128>& bits, int idx)   { bits.set(idx); }
inline void ResetBit128(std::bitset<128>& bits, int idx) { bits.reset(idx); }

bool AnyKeyDown() { return (PlatformCtx.DownKeys.count()) > 0; }

bool GetKeyDown(char c)     { return GetBit128(PlatformCtx.DownKeys, c); }

bool GetKeyReleased(char c) { return GetBit128(PlatformCtx.ReleasedKeys, c); }

bool GetKeyPressed(char c)  { return GetBit128(PlatformCtx.PressedKeys, c); }

static void SetPressedAndReleasedKeys()
{
    PlatformCtx.ReleasedKeys = PlatformCtx.LastKeys & ~PlatformCtx.DownKeys;
    PlatformCtx.PressedKeys  = ~PlatformCtx.LastKeys & PlatformCtx.DownKeys;
    // Mouse
    PlatformCtx.MouseReleased = PlatformCtx.MouseLast & ~PlatformCtx.MouseDown;
    PlatformCtx.MousePressed  = ~PlatformCtx.MouseLast & PlatformCtx.MouseDown;
}

static void RecordLastKeys() {
    PlatformCtx.LastKeys = PlatformCtx.DownKeys;
    PlatformCtx.MouseLast = PlatformCtx.MouseDown;
}

bool AnyMouseKeyDown()                    { return PlatformCtx.MouseDown > 0; }
bool GetMouseDown(MouseButton button)     { return !!(PlatformCtx.MouseDown     & button); }
bool GetMouseReleased(MouseButton button) { return !!(PlatformCtx.MouseReleased & button); }
bool GetMousePressed(MouseButton button)  { return !!(PlatformCtx.MousePressed  & button); }

int GetPressedNumber() {
    for (int i = '0'; i <= '9'; i++)
        if (PlatformCtx.ReleasedKeys[i])
            return i - '0';
    return -1;
}

bool AnyNumberPressed() {
    return GetPressedNumber() != -1;
}

void GetMousePos(float* x, float* y)
{
    ASSERT((uint64_t)x & (uint64_t)y); // shouldn't be nullptr
    POINT point;
    GetCursorPos(&point);
    *x = (float)point.x;
    *y = (float)point.y;
}

void SetMousePos(float x, float y)
{
    SetCursorPos((int)x, (int)y);
}

void GetMouseWindowPos(float* x, float* y)
{
    *x = PlatformCtx.MousePosX; *y = PlatformCtx.MousePosY;
}

void SetMouseWindowPos(float x, float y)
{
    SetMousePos(PlatformCtx.WindowPosX + x, PlatformCtx.WindowPosY + y);
}

float GetMouseWheelDelta() 
{
    return PlatformCtx.MouseWheelDelta; 
}

static LRESULT CALLBACK WindowCallback(HWND window, UINT msg, WPARAM wparam, LPARAM lparam)
{
    LRESULT result = 0;
    wchar_t wch = 0;

    switch (msg)
    {
        case WM_MOUSEMOVE:
            PlatformCtx.MousePosX = (float)LOWORD(lparam); 
            PlatformCtx.MousePosY = (float)HIWORD(lparam); 
            if (PlatformCtx.MouseMoveCallback) 
                PlatformCtx.MouseMoveCallback(PlatformCtx.MousePosX, PlatformCtx.MousePosY);
            break;
        case WM_MOUSEWHEEL:
            PlatformCtx.MouseWheelDelta = (float)GET_WHEEL_DELTA_WPARAM(wparam) / (float)WHEEL_DELTA;
            break;
        case WM_LBUTTONDOWN: PlatformCtx.MouseDown |= MouseButton_Left;    break;
        case WM_RBUTTONDOWN: PlatformCtx.MouseDown |= MouseButton_Right;   break;
        case WM_MBUTTONDOWN: PlatformCtx.MouseDown |= MouseButton_Middle;  break;
        case WM_LBUTTONUP:   PlatformCtx.MouseDown &= ~MouseButton_Left;   break;
        case WM_RBUTTONUP:   PlatformCtx.MouseDown &= ~MouseButton_Right;  break;
        case WM_MBUTTONUP:   PlatformCtx.MouseDown &= ~MouseButton_Middle; break;
        case WM_XBUTTONUP:   PlatformCtx.MouseDown &= ~MouseButton_Middle; break;
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
        {
            if (wparam > 127 || wparam < 0) break;
            SetBit128(PlatformCtx.DownKeys, (int)wparam);
            break;
        }
        case WM_SETFOCUS:
        case WM_KILLFOCUS:
            if (PlatformCtx.FocusChangedCallback)
                PlatformCtx.FocusChangedCallback(msg == WM_SETFOCUS);
            break;
        case WM_KEYUP:
        case WM_SYSKEYUP:
        {
            if (wparam > 127 || wparam < 0) break;
            ResetBit128(PlatformCtx.DownKeys, (int)wparam);
            break;
        }
        case WM_CHAR:
            if (PlatformCtx.KeyPressCallback) 
                PlatformCtx.KeyPressCallback((uint)wparam);
            break;
        case WM_SIZE:
            PlatformCtx.WindowWidth  = LOWORD(lparam);
            PlatformCtx.WindowHeight = HIWORD(lparam);
            if (PlatformCtx.WindowResizeCallback)
                PlatformCtx.WindowResizeCallback(PlatformCtx.WindowWidth, PlatformCtx.WindowHeight);
            break;
        case WM_MOVE:
            PlatformCtx.WindowPosX = LOWORD(lparam);
            PlatformCtx.WindowPosY = HIWORD(lparam);
            break;
        case WM_CLOSE:
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        default:
            result = DefWindowProcA(window, msg, wparam, lparam);
            break;
    }
    return result;
}

/********************************************************************************/
/*                       OpenGL, WGL Initialization                             */
/********************************************************************************/

// See https://www.khronos.org/registry/OpenGL/extensions/ARB/WGL_ARB_create_context.txt for all values
// See https://www.khronos.org/registry/OpenGL/extensions/ARB/WGL_ARB_pixel_format.txt for all values
// See https://gist.github.com/nickrolfe/1127313ed1dbf80254b614a721b3ee9c
typedef HGLRC WINAPI wglCreateContextAttribsARB_type(HDC hdc, HGLRC hShareContext, const int* attribList);
typedef BOOL WINAPI wglChoosePixelFormatARB_type(HDC hdc, const int* piAttribIList, const FLOAT* pfAttribFList, UINT nMaxFormats, int* piFormats, UINT* nNumFormats);

wglCreateContextAttribsARB_type* wglCreateContextAttribsARB;
wglChoosePixelFormatARB_type* wglChoosePixelFormatARB = nullptr;

BOOL(WINAPI* wglSwapIntervalEXT)(int) = nullptr;

#include <stdio.h>

void FatalError(const char* format, ...)
{
    char buffer[1024]; // Adjust the size according to your needs
    // Format the error message
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    printf(buffer);
    OutputDebugString(buffer);
    // Display the message box
    MessageBoxA(NULL, buffer, "Fatal Error", MB_ICONERROR | MB_OK);
    ASSERT(0);
}

void DebugLog(const char* format, ...)
{
    char buffer[1024]; // Adjust the size according to your needs
    // Format the error message
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    printf(buffer);
    OutputDebugString(buffer);
}

// https://gist.github.com/mmozeiko/6825cb94d393cb4032d250b8e7cc9d14
static void GetWglFunctions(void)
{
    // to get WGL functions we need valid GL context, so create dummy window for dummy GL contetx
    HWND dummy = CreateWindowExW(
        0, L"STATIC", L"DummyWindow", WS_OVERLAPPED,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        NULL, NULL, NULL, NULL);
    ASSERT(dummy && "Failed to create dummy window");

    HDC dc = GetDC(dummy);
    ASSERT(dc && "Failed to get device context for dummy window");

    PIXELFORMATDESCRIPTOR desc = {};
    desc.nSize = sizeof(desc);
    desc.nVersion = 1;
    desc.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    desc.iPixelType = PFD_TYPE_RGBA;
    desc.cColorBits = 24;

    int format = ChoosePixelFormat(dc, &desc);
    if (!format)
        FatalError("Cannot choose OpenGL pixel format for dummy window!");

    int ok = DescribePixelFormat(dc, format, sizeof(desc), &desc);
    ASSERT(ok && "Failed to describe OpenGL pixel format");

    // reason to create dummy window is that SetPixelFormat can be called only once for the window
    if (!SetPixelFormat(dc, format, &desc))
        FatalError("Cannot set OpenGL pixel format for dummy window!");

    HGLRC rc = wglCreateContext(dc);
    ASSERT(rc && "Failed to create OpenGL context for dummy window");

    ok = wglMakeCurrent(dc, rc);
    ASSERT(ok && "Failed to make current OpenGL context for dummy window");

    wglCreateContextAttribsARB = (wglCreateContextAttribsARB_type*)wglGetProcAddress("wglCreateContextAttribsARB");
    wglSwapIntervalEXT = (BOOL(WINAPI*)(int))wglGetProcAddress("wglSwapIntervalEXT");
    wglChoosePixelFormatARB = (wglChoosePixelFormatARB_type*)wglGetProcAddress("wglChoosePixelFormatARB");

    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(rc);
    ReleaseDC(dummy, dc);
    DestroyWindow(dummy);
}

static HWND WindowCreate(HINSTANCE instance)
{
    GetWglFunctions();
    // Now we can choose a pixel format the modern way, using wglChoosePixelFormatARB.
    int pixel_format_attribs[] = {
        0x2001,          1, // WGL_DRAW_TO_WINDOW_ARB
        0x2010,          1, // WGL_SUPPORT_OPENGL_ARB
        0x2011,          1, // WGL_DOUBLE_BUFFER_ARB
        0x2003,     0x2027, // WGL_ACCELERATION_ARB, WGL_FULL_ACCELERATION_ARB
        0x2013,     0x202B, // WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_ARB
        0x2014,         32, // WGL_COLOR_BITS_ARB
        0x2022,         24, // WGL_DEPTH_BITS_ARB
        0x2023,          8, // WGL_STENCIL_BITS_ARB
        0x20A9,          1, // WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB <- SRGB support
        0x2041,          1, // WGL_SAMPLE_BUFFERS_ARB           <- enable MSAA
        0x2042,          8, // WGL_SAMPLES_ARB                  <- 4x MSAA
        0
    };
    // register window class to have custom WindowProc callback
    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(wc),
    wc.lpfnWndProc = WindowCallback,
    wc.hInstance = instance,
    wc.hIcon     = LoadIconA(instance, "icon"),
    wc.hCursor   = LoadCursor(NULL, IDC_ARROW),
    wc.lpszClassName = WindowName;
    ATOM atom = RegisterClassEx(&wc);
    ASSERT(atom && "Failed to register window class");

    if (PlatformCtx.WindowWidth == 0 || PlatformCtx.WindowHeight == 0)
    {
        wGetMonitorSize(&PlatformCtx.WindowWidth, &PlatformCtx.WindowHeight);
    }

    // window properties - width, height and style
    int width = PlatformCtx.WindowWidth;
    int height = PlatformCtx.WindowHeight;
    DWORD exstyle = WS_EX_APPWINDOW;
    DWORD style = WS_OVERLAPPEDWINDOW;

    // VERY IMPORTANT: all windows sharing same OpenGL context must have same pixel format
    // this is mentioned in https://www.khronos.org/registry/OpenGL/extensions/ARB/WGL_ARB_create_context.txt
    int format = 0;
    PIXELFORMATDESCRIPTOR desc = {};
    desc.nSize = sizeof(desc);

    // create window
    HWND window = CreateWindowEx(
        exstyle, wc.lpszClassName, WindowName, style,
        CW_USEDEFAULT, CW_USEDEFAULT, width, height,
        NULL, NULL, wc.hInstance, NULL);
    ASSERT(window && "Failed to create window");

    HDC dc = GetDC(window);
    ASSERT(dc && "Failed to window device context");

    // figure out pixel format
    int attrib[] = {
        0x2001, GL_TRUE, // WGL_DRAW_TO_WINDOW_ARB
        0x2010, GL_TRUE, // WGL_SUPPORT_OPENGL_ARB
        0x2011, GL_TRUE, // WGL_DOUBLE_BUFFER_ARB
        0x2013, 0x202B, // WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_ARB
        0x2014, 24, // WGL_COLOR_BITS_ARB
        0x2022, 24, // WGL_DEPTH_BITS_ARB
        0x2023,  8, // WGL_STENCIL_BITS_ARB
        // uncomment for sRGB framebuffer, from WGL_ARB_framebuffer_sRGB extension
        // https://www.khronos.org/registry/OpenGL/extensions/ARB/ARB_framebuffer_sRGB.txt
        //WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB, GL_TRUE,
        // uncomment for multisampeld framebuffer, from WGL_ARB_multisample extension
        // https://www.khronos.org/registry/OpenGL/extensions/ARB/ARB_multisample.txt
        0x2041, 1, // WGL_SAMPLE_BUFFERS_ARB
        0x2042, 4, // 4x MSAA WGL_SAMPLES_ARB
        0,
    };
    
    UINT formats;
    if (!wglChoosePixelFormatARB(dc, attrib, NULL, 1, &format, &formats) || formats == 0)
        FatalError("OpenGL does not support required pixel format!");
    
    int ok = DescribePixelFormat(dc, format, sizeof(desc), &desc);
    ASSERT(ok && "Failed to describe OpenGL pixel format");

    // always set pixel format, same for all windows
    if (!SetPixelFormat(dc, format, &desc))
        FatalError("Cannot set OpenGL selected pixel format!");

    return window;
}

HGLRC InitOpenGL(HDC dc)
{
    // now create modern OpenGL context, can do it after pixel format is set
    int attrib[] =
    {
        0x2091, 4, // WGL_CONTEXT_MAJOR_VERSION_ARB
        0x2092, 3, // WGL_CONTEXT_MINOR_VERSION_ARB
        0x9126,  0x00000001, // WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB
        #ifdef DEBUG
        // ask for debug context for non "Release" builds
        // this is so we can enable debug callback
        0x2094, 0x00000001, // WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_DEBUG_BIT_ARB
        #endif
        0,
    };

    // we'll use only one OpenGL context for simplicity, no need to worry about resource sharing
    HGLRC rc = wglCreateContextAttribsARB(dc, NULL, attrib);
    if (!rc)
    {
        FatalError("Cannot create modern OpenGL context! OpenGL version 4.5 not supported?");
    }

    BOOL ok = wglMakeCurrent(dc, rc);
    ASSERT(ok && "Failed to make current OpenGL context");
    return rc;
}

/********************************************************************************/
/*                                     TIME                                     */
/********************************************************************************/

double GetDeltaTime() 
{ 
    return PlatformCtx.DeltaTime; 
}

double TimeSinceStartup()
{
    LARGE_INTEGER currentTime;
    QueryPerformanceCounter(&currentTime);
    return (double)(currentTime.QuadPart - PlatformCtx.StartupTime) / PlatformCtx.Frequency;
}

// forom SaneProgram.cpp
extern void AXInit();
extern int  AXStart();
extern void AXLoop(bool shouldRender);
extern void AXExit();
// forom Renderer.cpp
extern void rDestroyRenderer();
extern void rInitRenderer();
extern void rSetViewportSize(int x, int y);

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmd_line, int show)
{
    // VMem::Initialise();
    if (ma_engine_init(NULL, &maEngine) != MA_SUCCESS) {
        FatalError("mini audio init failed!");
        return 1;
    }
    MemsetZero(&PlatformCtx, sizeof(PlatformContextWin));
    AXInit();
    
    PlatformCtx.hwnd = WindowCreate(inst);
    HDC   dc         = GetDC(PlatformCtx.hwnd);
    HGLRC rc         = InitOpenGL(dc);
    
    gladLoaderLoadGL();
    
    ShowWindow(PlatformCtx.hwnd, show);
    UpdateWindow(PlatformCtx.hwnd);
    // first thing that we will see is going to be black color instead of white
    // if we clear before starting the engine
    SwapBuffers(dc);
    rInitRenderer();

    // init time
    LARGE_INTEGER frequency, prevTime, currentTime;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&prevTime);

    currentTime = prevTime;
    PlatformCtx.StartupTime = currentTime.QuadPart;
    PlatformCtx.Frequency   = frequency.QuadPart;

    if (AXStart() == 0)
        return 1; // user defined startup failed

    while (!PlatformCtx.ShouldClose)
    {   
        MSG msg;
        while (PeekMessageA(&msg, 0, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT) 
                goto end_infinite_loop;
         
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }

        SetPressedAndReleasedKeys();
        
        QueryPerformanceCounter(&currentTime);
        PlatformCtx.DeltaTime = (double)(currentTime.QuadPart - prevTime.QuadPart) / frequency.QuadPart;
        prevTime = currentTime;
        
        if (GetKeyDown(Key_MENU) && GetKeyDown(Key_F4)) // alt f4 check
            goto end_infinite_loop;

        // char fps[10]={};
        // IntToString(fps, (int)(1.0 / PlatformCtx.DeltaTime));
        // wSetWindowName(fps);

        // Do OpenGL rendering here
        AXLoop(true); // should render true
        wglSwapIntervalEXT(PlatformCtx.VSyncActive); // vsync
        SwapBuffers(dc);

        RecordLastKeys();
        PlatformCtx.MouseWheelDelta = 0.0f;

        glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // | GL_STENCIL_BUFFER_BIT
    }
    end_infinite_loop:
    {
        AXExit();
        rDestroyRenderer();
        wglMakeCurrent(dc, 0);
        ReleaseDC(PlatformCtx.hwnd, dc);
        wglDeleteContext(rc);
        DestroyWindow(PlatformCtx.hwnd);
        if (PlatformCtx.ClipboardString)
            delete[] PlatformCtx.ClipboardString;
    }
    // VMem::Destroy();
    ma_engine_uninit(&maEngine);

    return 0;
}

// https://github.com/glfw/glfw/blob/master/src/win32_init.c#L467
static char* CreateUTF8FromWideStringWin32(const WCHAR* source)
{
    char* target;
    int size;

    size = WideCharToMultiByte(CP_UTF8, 0, source, -1, NULL, 0, NULL, NULL);
    if (!size) {
        DebugLog("Win32: Failed to convert string to UTF-8");
        return nullptr;
    }

    target = new char[size];

    if (!WideCharToMultiByte(CP_UTF8, 0, source, -1, target, size, NULL, NULL))
    {
        DebugLog("Win32: Failed to convert string to UTF-8");
        delete[] target;
        return nullptr;
    }
    return target;
}

const char* wGetClipboardString()
{
    HANDLE object;
    WCHAR* buffer;
    int tries = 0;

    // NOTE: Retry clipboard opening a few times as some other application may have it
    //       open and also the Windows Clipboard History reads it after each update
    while (!OpenClipboard(PlatformCtx.hwnd))
    {
        Sleep(1);
        tries++;

        if (tries == 3)
        {
            DebugLog("Win32: Failed to open clipboard");
            return nullptr;
        }
    }

    object = GetClipboardData(CF_UNICODETEXT);
    if (!object)
    {
        DebugLog("Win32: Failed to convert clipboard to string");
        CloseClipboard();
        return nullptr;
    }

    buffer = (WCHAR*)GlobalLock(object);
    if (!buffer)
    {
        DebugLog("Win32: Failed to lock global handle");
        CloseClipboard();
        return nullptr;
    }

    if (PlatformCtx.ClipboardString)
        delete[] PlatformCtx.ClipboardString;
    PlatformCtx.ClipboardString = CreateUTF8FromWideStringWin32(buffer);

    GlobalUnlock(object);
    CloseClipboard();

    return PlatformCtx.ClipboardString;
}

bool wSetClipboardText(const char* string) 
{
    int characterCount, tries = 0;
    HANDLE object;
    WCHAR* buffer;

    characterCount = MultiByteToWideChar(CP_UTF8, 0, string, -1, NULL, 0);
    if (!characterCount)
        return false;

    object = GlobalAlloc(GMEM_MOVEABLE, characterCount * sizeof(WCHAR));
    if (!object)
    {
        DebugLog("Win32: Failed to allocate global handle for clipboard");
        return false;
    }

    buffer = (WCHAR*)GlobalLock(object);
    if (!buffer)
    {
        DebugLog("Win32: Failed to lock global handle");
        GlobalFree(object);
        return false;
    }

    MultiByteToWideChar(CP_UTF8, 0, string, -1, buffer, characterCount);
    GlobalUnlock(object);

    // NOTE: Retry clipboard opening a few times as some other application may have it
    //       open and also the Windows Clipboard History reads it after each update
    while (!OpenClipboard(PlatformCtx.hwnd))
    {
        Sleep(1);
        tries++;

        if (tries == 3)
        {
            DebugLog("Win32: Failed to open clipboard");
            GlobalFree(object);
            return false;
        }
    }

    EmptyClipboard();
    SetClipboardData(CF_UNICODETEXT, object);
    CloseClipboard();
    return true;
}


// https://stackoverflow.com/questions/2382464/win32-full-screen-and-hiding-taskbar
bool wEnterFullscreen(int fullscreenWidth, int fullscreenHeight) 
{
    DEVMODE fullscreenSettings;
    EnumDisplaySettings(NULL, 0, &fullscreenSettings);
    PlatformCtx.WindowWidth  = fullscreenSettings.dmPelsWidth  = fullscreenWidth;
    PlatformCtx.WindowHeight = fullscreenSettings.dmPelsHeight = fullscreenHeight;
    fullscreenSettings.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT;

    SetWindowLongPtr(PlatformCtx.hwnd, GWL_EXSTYLE, WS_EX_APPWINDOW | WS_EX_TOPMOST);
    SetWindowLongPtr(PlatformCtx.hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
    SetWindowPos(PlatformCtx.hwnd, HWND_TOPMOST, 0, 0, fullscreenWidth, fullscreenHeight, SWP_SHOWWINDOW);
    bool success = ChangeDisplaySettings(&fullscreenSettings, CDS_FULLSCREEN) == DISP_CHANGE_SUCCESSFUL;
    ASSERT(success && "unable to make full screen");
    ShowWindow(PlatformCtx.hwnd, SW_MAXIMIZE);
    
    if (success && PlatformCtx.WindowResizeCallback)
    {
        PlatformCtx.WindowResizeCallback(fullscreenWidth, fullscreenHeight); 
        rSetViewportSize(fullscreenWidth, fullscreenHeight);
    }
    return success;
}

bool wExitFullscreen(int windowX, int windowY, int windowedWidth, int windowedHeight) 
{
    SetWindowLongPtr(PlatformCtx.hwnd, GWL_EXSTYLE, WS_EX_LEFT);
    SetWindowLongPtr(PlatformCtx.hwnd, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE);
    bool success = ChangeDisplaySettings(NULL, CDS_RESET) == DISP_CHANGE_SUCCESSFUL;
    ASSERT(success && "unable to make windowed");
    PlatformCtx.WindowWidth = windowedWidth; PlatformCtx.WindowHeight = windowedHeight;
    SetWindowPos(PlatformCtx.hwnd, HWND_NOTOPMOST, windowX, windowY, windowedWidth, windowedHeight, SWP_SHOWWINDOW);
    ShowWindow(PlatformCtx.hwnd, SW_RESTORE);

    if (success && PlatformCtx.WindowResizeCallback)
    {
        PlatformCtx.WindowResizeCallback(windowedWidth, windowedHeight); 
        rSetViewportSize(windowedWidth, windowedHeight);
    }
    return success;
}


#endif // defined _WIN32