
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
#include <Windows.h>

#include "ASTL/Common.hpp"
#include "ASTL/Algorithms.hpp"
#include "Platform.hpp"
#include "External/glad.hpp"

#define AX_LOG(...) printf(__VA_ARGS__)

#pragma comment (lib, "gdi32.lib")
#pragma comment (lib, "user32.lib")
#pragma comment (lib, "opengl32.lib")


struct PlatformContextWin
{
    // Callbacks
    void(*WindowMoveCallback)  (int  , int)   = nullptr;
    void(*WindowResizeCallback)(int  , int)   = nullptr;
    void(*MouseMoveCallback)   (float, float) = nullptr;
    void(*KeyPressCallback)    (wchar_t)      = nullptr;
    void(*FocusChangedCallback)(bool)         = nullptr;
    
    // Window
    int WindowPosX   = 0;
    int WindowPosY   = 0;
    int WindowWidth  = 0;
    int WindowHeight = 0;
    HWND hwnd       = nullptr;

    // Input Code, 128 bit bitmasks for key states.
    unsigned long DownKeys[2]{};
    unsigned long LastKeys[2]{};
    unsigned long PressedKeys[2]{};
    unsigned long ReleasedKeys[2]{};
    // Mouse
    int    MouseDown, MouseLast, MousePressed, MouseReleased;
    float  MousePosX, MousePosY;
    float  MouseWheelDelta;

    LONGLONG StartupTime;
    LONGLONG Frequency;
    double   DeltaTime;

    bool VSyncActive;
} PlatformCtx{}; 

static char WindowName[64]{ 'A', 'S', 'T', 'L' };


void SetFocusChangedCallback(void(*callback)(bool))         { PlatformCtx.FocusChangedCallback = callback; }
void SetKeyPressCallback    (void(*callback)(wchar_t))      { PlatformCtx.KeyPressCallback     = callback; }
void SetMouseMoveCallback   (void(*callback)(float, float)) { PlatformCtx.MouseMoveCallback    = callback; }
void SetWindowResizeCallback(void(*callback)(int, int))     { PlatformCtx.WindowResizeCallback = callback; }
void SetWindowMoveCallback  (void(*callback)(int, int))     { PlatformCtx.WindowMoveCallback   = callback; }


void GetWindowSize(int* x, int* y) { *x = PlatformCtx.WindowWidth;  *y = PlatformCtx.WindowHeight; }
void GetWindowPos (int* x, int* y) { *x = PlatformCtx.WindowPosX;   *y = PlatformCtx.WindowPosY;   }

void SetWindowSize(int width, int height)
{
    PlatformCtx.WindowWidth = width; PlatformCtx.WindowHeight = height;
    if (!PlatformCtx.hwnd) return;
    SetWindowPos(PlatformCtx.hwnd, nullptr, PlatformCtx.WindowPosX, PlatformCtx.WindowPosY, width, height, 0);
}

void SetWindowPosition(int x, int y)
{
    PlatformCtx.WindowPosX = x; PlatformCtx.WindowPosY = y;
    if (!PlatformCtx.hwnd) return;
    SetWindowPos(PlatformCtx.hwnd, nullptr, x, y, PlatformCtx.WindowWidth, PlatformCtx.WindowHeight, 0);
}

void SetWindowName(const char* name)
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

void GetMonitorSize(int* width, int* height)
{
    *width  = GetSystemMetrics(SM_CXSCREEN);
    *height = GetSystemMetrics(SM_CYSCREEN);
}

void SetVSync(bool active)
{
    PlatformCtx.VSyncActive = active; 
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
    // Display the message box
    MessageBoxA(NULL, buffer, "Fatal Error", MB_ICONERROR | MB_OK);
    OutputDebugString(buffer);
}

static void InitOpenGLExtensions(void)
{
    // Before we can load extensions, we need a dummy OpenGL context, created using a dummy window.
    // We use a dummy window because you can only set the pixel format for a window once.
    WNDCLASSA window_class{};
    window_class.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    window_class.lpfnWndProc   = DefWindowProcA;
    window_class.hInstance     = GetModuleHandle(0);
    window_class.lpszClassName = "Dummy_WGL_StagingWindow";
    
    if (!RegisterClass(&window_class)) 
        FatalError("Failed to register dummy OpenGL window.");
    
    HWND dummy_window = CreateWindowExA(0, window_class.lpszClassName, "ASTL Window",
                                        0, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 0,
                                        0, window_class.hInstance, 0);
    
    if (!dummy_window) FatalError("Failed to create dummy OpenGL window.");
    
    HDC dummy_dc = GetDC(dummy_window);
    PIXELFORMATDESCRIPTOR pfd{};
    pfd.nSize        = sizeof(pfd);
    pfd.nVersion     = 1;
    pfd.iPixelType   = PFD_TYPE_RGBA;
    pfd.dwFlags      = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.cColorBits   = 32;
    pfd.cAlphaBits   = 8;
    pfd.iLayerType   = PFD_MAIN_PLANE;
    pfd.cDepthBits   = 24;
    pfd.cStencilBits = 8;
    
    int pixel_format = ChoosePixelFormat(dummy_dc, &pfd);
    if (!pixel_format) FatalError("Failed to find a suitable pixel format.");
    
    if (!SetPixelFormat(dummy_dc, pixel_format, &pfd)) FatalError("Failed to set the pixel format.");
    
    HGLRC dummy_context = wglCreateContext(dummy_dc);
    if (!dummy_context) FatalError("Failed to create a dummy OpenGL rendering context.");
    
    if (!wglMakeCurrent(dummy_dc, dummy_context)) FatalError("Failed to activate dummy OpenGL rendering context.");
    
    wglCreateContextAttribsARB = (wglCreateContextAttribsARB_type*)wglGetProcAddress("wglCreateContextAttribsARB");
    wglChoosePixelFormatARB    = (wglChoosePixelFormatARB_type*)wglGetProcAddress("wglChoosePixelFormatARB");
    wglSwapIntervalEXT         = (BOOL(WINAPI*)(int)) wglGetProcAddress("wglSwapIntervalEXT");

    wglMakeCurrent(dummy_dc, 0);
    wglDeleteContext(dummy_context);
    ReleaseDC(dummy_window, dummy_dc);
    DestroyWindow(dummy_window);
}

static HGLRC InitOpenGL(HDC real_dc)
{
    InitOpenGLExtensions();
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
                                 0x2042,          4, // WGL_SAMPLES_ARB                  <- 4x MSAA
                                 0
    };
    
    int pixel_format;
    UINT num_formats;
    wglChoosePixelFormatARB(real_dc, pixel_format_attribs, 0, 1, &pixel_format, &num_formats);
    if (!num_formats) 
    FatalError("Failed to set the OpenGL 3.3 pixel format.");
    
    PIXELFORMATDESCRIPTOR pfd;
    DescribePixelFormat(real_dc, pixel_format, sizeof(pfd), &pfd);
    if (!SetPixelFormat(real_dc, pixel_format, &pfd)) 
    FatalError("Failed to set the OpenGL 3.3 pixel format.");
    
    // Specify that we want to create an OpenGL 3.2 core profile context
    int gl32_attribs[] = {
                         0x2091, 3, // WGL_CONTEXT_MAJOR_VERSION_ARB
                         0x2092, 2, // WGL_CONTEXT_MINOR_VERSION_ARB
                         0x9126,  0x00000001, // WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB
                         0,
    };
    
    HGLRC gl32_context = wglCreateContextAttribsARB(real_dc, 0, gl32_attribs);
    if (!gl32_context) 
    FatalError("Failed to create OpenGL 3.2 context.");
    
    if (!wglMakeCurrent(real_dc, gl32_context)) 
    FatalError("Failed to activate OpenGL 3.2 rendering context.");
    
    return gl32_context;
}

/********************************************************************************/
/*                          Keyboard and Mouse Input                            */
/********************************************************************************/

inline bool GetBit128(unsigned long bits[2], int idx)   { return !!(bits[idx > 63] & (1ul << (idx & 63ul))); }
inline void SetBit128(unsigned long bits[2], int idx)   { bits[idx > 63] |= 1ul << (idx & 63); }
inline void ResetBit128(unsigned long bits[2], int idx) { bits[idx > 63] &= ~(1ul << (idx & 63)); }

bool GetKeyDown(char c)     { return GetBit128(PlatformCtx.DownKeys, c); }

bool GetKeyReleased(char c) { return GetBit128(PlatformCtx.ReleasedKeys, c); }

bool GetKeyPressed(char c)  { return GetBit128(PlatformCtx.PressedKeys, c); }

static void SetPressedAndReleasedKeys()
{
    PlatformCtx.ReleasedKeys[0] = PlatformCtx.LastKeys[0] & ~PlatformCtx.DownKeys[0];
    PlatformCtx.ReleasedKeys[1] = PlatformCtx.LastKeys[1] & ~PlatformCtx.DownKeys[1];
    PlatformCtx.PressedKeys[0]  = ~PlatformCtx.LastKeys[0] & PlatformCtx.DownKeys[0];
    PlatformCtx.PressedKeys[1]  = ~PlatformCtx.LastKeys[1] & PlatformCtx.DownKeys[1];
    // Mouse
    PlatformCtx.MouseReleased = PlatformCtx.MouseLast & ~PlatformCtx.MouseDown;
    PlatformCtx.MousePressed  = ~PlatformCtx.MouseLast & PlatformCtx.MouseDown;
}

static void RecordLastKeys() {
    PlatformCtx.LastKeys[0] = PlatformCtx.DownKeys[0];
    PlatformCtx.LastKeys[1] = PlatformCtx.DownKeys[1];
    PlatformCtx.MouseLast = PlatformCtx.MouseDown;
}

bool GetMouseDown(MouseButton button)     { return !!(PlatformCtx.MouseDown     & button); }
bool GetMouseReleased(MouseButton button) { return !!(PlatformCtx.MouseReleased & button); }
bool GetMousePressed(MouseButton button)  { return !!(PlatformCtx.MousePressed  & button); }

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

static void UpdateRenderArea()
{
    glViewport(0, 0, PlatformCtx.WindowWidth, PlatformCtx.WindowHeight);
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
            if (wparam > 127) break;
            SetBit128(PlatformCtx.DownKeys, wparam);
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
            if (wparam > 127) break;
            ResetBit128(PlatformCtx.DownKeys, wparam);
            break;
        }
        case WM_CHAR:
            ::MultiByteToWideChar(CP_UTF8, MB_PRECOMPOSED, (char*)&wparam, 1, &wch, 1);
            if (PlatformCtx.KeyPressCallback) 
                PlatformCtx.KeyPressCallback(wch);
            break;
        case WM_SIZE:
            PlatformCtx.WindowWidth  = LOWORD(lparam);
            PlatformCtx.WindowHeight = HIWORD(lparam);
            UpdateRenderArea();
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

static HWND WindowCreate(HINSTANCE inst)
{
    WNDCLASSA window_class{};
    window_class.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    window_class.lpfnWndProc   = WindowCallback;
    window_class.hInstance     = inst;
    window_class.hCursor       = LoadCursor(0, IDC_ARROW);
    window_class.hbrBackground = 0;
    window_class.lpszClassName = "ASTLWindow";
    window_class.hIcon         = LoadIconA(inst, "icon");
    
    if (!RegisterClassA(&window_class))
    FatalError("Failed to register window.");
    
    // Specify a desired width and height, then adjust the rect so the window's client area will be that size.
    RECT rect{};
    rect.right  = PlatformCtx.WindowWidth;
    rect.bottom = PlatformCtx.WindowHeight;
    const DWORD window_style = WS_OVERLAPPEDWINDOW;

    AdjustWindowRect(&rect, window_style, false);
    
    HWND window = CreateWindowExA(0,
                                  window_class.lpszClassName,
                                  WindowName, window_style,
                                  CW_USEDEFAULT, CW_USEDEFAULT,
                                  PlatformCtx.WindowWidth, PlatformCtx.WindowHeight,
                                  0, 0, inst, 0);
    
    if (!window) FatalError("Failed to create window.");
    return window;
}

/**** Time ****/

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
extern void AXLoop();
extern void AXExit();
// forom Renderer.cpp
extern void DestroyRenderer();
extern void InitRenderer();

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmd_line, int show)
{
    AXInit();
    
    PlatformCtx.hwnd = WindowCreate(inst);
    HDC   dc         = GetDC(PlatformCtx.hwnd);
    HGLRC rc         = InitOpenGL(dc);
    
    gladLoaderLoadGL();
    
    ShowWindow(PlatformCtx.hwnd, show);
    UpdateWindow(PlatformCtx.hwnd);
    InitRenderer();
    // first thing that we will see is going to be black color instead of white
    // if we clear before starting the engine
    SwapBuffers(dc);

    if (AXStart() == 0) return 1; // user defined startup failed
    InitRenderer();

    LARGE_INTEGER frequency, prevTime, currentTime;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&prevTime);

    currentTime = prevTime;
    PlatformCtx.StartupTime = currentTime.QuadPart;
    PlatformCtx.Frequency   = frequency.QuadPart;

    while (true)
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
        
        // char fps[9]{};
        // IntToString(fps, (int)(1.0 / PlatformCtx.DeltaTime));
        // SetWindowName(fps);

        // Do OpenGL rendering here
        AXLoop();
        wglSwapIntervalEXT(PlatformCtx.VSyncActive); // vsync
        SwapBuffers(dc);

        RecordLastKeys();
        PlatformCtx.MouseWheelDelta = 0.0f;

        glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    }
    end_infinite_loop:
    {
        AXExit();
        DestroyRenderer();
        wglMakeCurrent(dc, 0);
        ReleaseDC(PlatformCtx.hwnd, dc);
        wglDeleteContext(rc);
        DestroyWindow(PlatformCtx.hwnd);
    }
    return 0;
}

// https://stackoverflow.com/questions/2382464/win32-full-screen-and-hiding-taskbar
bool EnterFullscreen(int fullscreenWidth, int fullscreenHeight) 
{
    DEVMODE fullscreenSettings;
    EnumDisplaySettings(NULL, 0, &fullscreenSettings);
    PlatformCtx.WindowWidth  = fullscreenSettings.dmPelsWidth  = fullscreenWidth;
    PlatformCtx.WindowHeight = fullscreenSettings.dmPelsHeight = fullscreenHeight;
    fullscreenSettings.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT;

    SetWindowLongPtr(PlatformCtx.hwnd, GWL_EXSTYLE, WS_EX_APPWINDOW | WS_EX_TOPMOST);
    SetWindowLongPtr(PlatformCtx.hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
    SetWindowPos(PlatformCtx.hwnd, HWND_TOPMOST, 0, 0, fullscreenWidth, fullscreenHeight, SWP_SHOWWINDOW);
    bool success = ChangeDisplaySettings(&fullscreenSettings, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL;
    ASSERT(success && "unable to make full screen");
    ShowWindow(PlatformCtx.hwnd, SW_MAXIMIZE);
    
    if (success && PlatformCtx.WindowResizeCallback)
        PlatformCtx.WindowResizeCallback(fullscreenWidth, fullscreenHeight), UpdateRenderArea();
    return success;
}

bool ExitFullscreen(int windowX, int windowY, int windowedWidth, int windowedHeight) 
{
    SetWindowLongPtr(PlatformCtx.hwnd, GWL_EXSTYLE, WS_EX_LEFT);
    SetWindowLongPtr(PlatformCtx.hwnd, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE);
    bool success = ChangeDisplaySettings(NULL, CDS_RESET) == DISP_CHANGE_SUCCESSFUL;
    ASSERT(success && "unable to make windowed");
    PlatformCtx.WindowWidth = windowedWidth; PlatformCtx.WindowHeight = windowedHeight;
    SetWindowPos(PlatformCtx.hwnd, HWND_NOTOPMOST, windowX, windowY, windowedWidth, windowedHeight, SWP_SHOWWINDOW);
    ShowWindow(PlatformCtx.hwnd, SW_RESTORE);

    if (success && PlatformCtx.WindowResizeCallback)
        PlatformCtx.WindowResizeCallback(windowedWidth, windowedHeight), UpdateRenderArea();
    
    return success;
}


#endif // defined _WIN32
