
/********************************************************************
*    Purpose: Creating Window, Keyboard and Mouse input, Main Loop  *
*             Touch Input(Android).                                 *
*             Functions are implemented in Platform.cpp             *
*    Author : Anilcan Gulkaya 2023 anilcangulkaya7@gmail.com        *
********************************************************************/

#pragma once

#if defined(_DEBUG) || defined(DEBUG)
#ifdef __ANDROID__
    #include <android/log.h>
    #define AX_ERROR(format, ...) { __android_log_print(ANDROID_LOG_FATAL, "AX-FATAL", "%s -line:%i " format, GetFileName(__FILE__), __LINE__, ##__VA_ARGS__); ASSERT(0);}
    #define AX_LOG(format, ...)    __android_log_print(ANDROID_LOG_INFO, "AX-INFO", "%s -line:%i " format, GetFileName(__FILE__), __LINE__, ##__VA_ARGS__)
    #define AX_WARN(format, ...)   __android_log_print(ANDROID_LOG_WARN, "AX-WARN", "%s -line:%i " format, GetFileName(__FILE__), __LINE__, ##__VA_ARGS__)
#else
    void FatalError(const char* format, ...); // defined in PlatformBla.cpp

    #define AX_LOG(format, ...)  
    #define AX_WARN(format, ...) 

    #if !(defined(__GNUC__) || defined(__GNUG__))
    #   define AX_ERROR(format, ...) FatalError("%s -line:%i " format, GetFileName(__FILE__), __LINE__, __VA_ARGS__)
    #else                                                             
    #   define AX_ERROR(format, ...) FatalError("%s -line:%i " format, GetFileName(__FILE__), __LINE__,##__VA_ARGS__)
    #endif
#endif
#else
    #define AX_ERROR(format, ...)
    #define AX_LOG(format, ...)  
    #define AX_WARN(format, ...) 
#endif


inline __constexpr const char* GetFileName(const char* path)
{
    int length = 0;
    while (path[length]) length++;

    while (path[length-1] != '\\' && path[length-1] != '/') 
        length--;
    return path + length;
}

////////                Window               ////////

#ifndef __ANDROID__
void SetWindowSize(int width, int height);
void SetWindowPosition(int x, int y);
void SetWindowMoveCallback(void(*callback)(int, int));
void SetWindowName(const char* name);
void GetWindowPos(int* x, int* y);

// Sets Window as Full Screen, with given resolution, this might improve performance but pixel density drops down
// You can set the resolution using GetMonitorSize or following resolutions: 
// 2560x1440, 1920x1080, 1280×720 etc.
bool EnterFullscreen(int fullscreenWidth, int fullscreenHeight);

// Go back to full screen Mode
bool ExitFullscreen(int windowX, int windowY, int windowedWidth, int windowedHeight);

void SetVSync(bool active);
#endif

void SetFocusChangedCallback(void(*callback)(bool focused));
void SetWindowResizeCallback(void(*callback)(int, int));
void GetWindowSize(int* x, int* y);
void GetMonitorSize(int* width, int* height);

////////                Keyboard             ////////

// use enum KeyboardKey or asci char 'X'
bool GetKeyDown(char c);

bool GetKeyPressed(char c);

bool GetKeyReleased(char c);

void SetKeyPressCallback(void(*callback)(wchar_t));

////////                Mouse                ////////
// Mouse is finger in Android, and MouseButton is finger id.

enum MouseButton_
{
    MouseButton_Left   = 1,
    MouseButton_Right  = 2,
    MouseButton_Middle = 4,

    MouseButton_Touch0 = 1,
    MouseButton_Touch1 = 2,
    MouseButton_Touch2 = 4
};
typedef int MouseButton;

bool GetMouseDown(MouseButton button);
bool GetMouseReleased(MouseButton button);
bool GetMousePressed(MouseButton button);

void GetMousePos(float* x, float* y);
void GetMouseWindowPos(float* x, float* y);
void SetMouseMoveCallback(void(*callback)(float, float));
float GetMouseWheelDelta();

#ifndef __ANDROID__
void SetMousePos(float x, float y);
void SetMouseWindowPos(float x, float y);
#else
struct Touch
{
    float positionX;
    float positionY;
};

Touch GetTouch(int index);

int NumTouchPressed();
#endif
////////                TIME                 ////////

double GetDeltaTime();
double TimeSinceStartup();

/*
* KeyboardKey_0 - KeyboardKey_9 are the same as ASCII '0' - '9' (0x30 - 0x39)
* KeyboardKey_A - KeyboardKey_Z are the same as ASCII 'A' - 'Z' (0x41 - 0x5A)
*/

// Todo: we will need different enum values for each platform

enum KeyboardKey_
{
    Key_BACK       = 0x08,   Key_MODECHANGE     = 0x1F,
    Key_TAB        = 0x09,   Key_SPACE          = 0x20,
    Key_CLEAR      = 0x0C,   Key_PRIOR          = 0x21,
    Key_RETURN     = 0x0D,   Key_NEXT           = 0x22,
    Key_SHIFT      = 0x10,   Key_END            = 0x23,
    Key_CONTROL    = 0x11,   Key_HOME           = 0x24,
    Key_MENU       = 0x12,   Key_LEFT           = 0x25,
    Key_PAUSE      = 0x13,   Key_UP             = 0x26,
    Key_CAPITAL    = 0x14,   Key_RIGHT          = 0x27,
    Key_ESCAPE     = 0x1B,   Key_DOWN           = 0x28,
    Key_CONVERT    = 0x1C,   Key_SELECT         = 0x29,
    Key_NONCONVERT = 0x1D,   Key_PRINT          = 0x2A,
    Key_ACCEPT     = 0x1E,   Key_EXECUTE        = 0x2B,
    Key_SNAPSHOT   = 0x2C,   Key_MULTIPLY       = 0x6A,
    Key_INSERT     = 0x2D,   Key_ADD            = 0x6B,
    Key_DELETE     = 0x2E,   Key_SEPARATOR      = 0x6C,
    Key_HELP       = 0x2F,   Key_SUBTRACT       = 0x6D,
    Key_LWIN       = 0x5B,   Key_DECIMAL        = 0x6E,
    Key_RWIN       = 0x5C,   Key_DIVIDE         = 0x6F,
    Key_APPS       = 0x5D,   Key_F1             = 0x70,
    Key_SLEEP      = 0x5F,   Key_F2             = 0x71,
    Key_NUMPAD0    = 0x60,   Key_F3             = 0x72,
    Key_NUMPAD1    = 0x61,   Key_F4             = 0x73,
    Key_NUMPAD2    = 0x62,   Key_F5             = 0x74,
    Key_NUMPAD3    = 0x63,   Key_F6             = 0x75,
    Key_NUMPAD4    = 0x64,   Key_F7             = 0x76,
    Key_NUMPAD5    = 0x65,   Key_F8             = 0x77,
    Key_NUMPAD6    = 0x66,   Key_F9             = 0x78,
    Key_NUMPAD7    = 0x67,   Key_F10            = 0x79,
    Key_NUMPAD8    = 0x68,   Key_F11            = 0x7A,
    Key_NUMPAD9    = 0x69,   Key_F12            = 0x7B
};
typedef int KeyboardKey;

// These functions are not used in android code, we are inlining here
// this way compiler will not use this functions
#ifdef __ANDROID__

inline void SetWindowSize(int width, int height) {}
inline void SetWindowPosition(int x, int y) {}
inline void SetWindowMoveCallback(void(*callback)(int, int)) {}
inline void SetWindowName(const char* name) {}
inline void GetWindowPos(int* x, int* y) { *x=0; *y=0;}

inline bool EnterFullscreen(int fullscreenWidth, int fullscreenHeight) { return false; }

inline bool ExitFullscreen(int windowX, int windowY, int windowedWidth, int windowedHeight) { return false; }

inline void SetVSync(bool active) {}

inline void SetMousePos(float x, float y) {}

inline void SetMouseWindowPos(float x, float y) {}
#endif