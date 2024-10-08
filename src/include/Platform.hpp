
/********************************************************************
*    Purpose: Creating Window, Keyboard and Mouse input, Main Loop  *
*             Touch Input(Android).                                 *
*             Functions are implemented in PlatformX.cpp            *
*    Author : Anilcan Gulkaya 2023 anilcangulkaya7@gmail.com        *
********************************************************************/

#pragma once

// enables logging no matter what
#define AX_ENABLE_LOGGING

#if defined(AX_ENABLE_LOGGING) || defined(_DEBUG) || defined(DEBUG) || defined(Debug)
#ifdef __ANDROID__
    #include <android/log.h>
    #define AX_ERROR(format, ...) { __android_log_print(ANDROID_LOG_FATAL, "AX-FATAL", "%s -line:%i " format, GetFileName(__FILE__), __LINE__, ##__VA_ARGS__); ASSERT(0);}
    #define AX_LOG(format, ...)    __android_log_print(ANDROID_LOG_INFO, "AX-INFO", "%s -line:%i " format, GetFileName(__FILE__), __LINE__, ##__VA_ARGS__)
    #define AX_WARN(format, ...)   __android_log_print(ANDROID_LOG_WARN, "AX-WARN", "%s -line:%i " format, GetFileName(__FILE__), __LINE__, ##__VA_ARGS__)
#else
    void FatalError(const char* format, ...); // defined in PlatformBla.cpp
    void DebugLog(const char* format, ...); 

    #define AX_LOG(format, ...)  DebugLog("axInfo: %s -line:%i " format, GetFileName(__FILE__), __LINE__, __VA_ARGS__)
    #define AX_WARN(format, ...) DebugLog("axWarn: %s -line:%i " format, GetFileName(__FILE__), __LINE__, __VA_ARGS__)

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

//------------------------------------------------------------------------
//  Window

enum wCursor_
{
    wCursor_Arrow     ,
    wCursor_TextInput , // < | for text
    wCursor_ResizeAll , // < +  
    wCursor_ResizeEW  , // < - 
    wCursor_ResizeNS  , // < | 
    wCursor_ResizeNESW, // < /
    wCursor_ResizeNWSE, 
    wCursor_Hand      , // < hand
    wCursor_NotAllowed, // < not allowed
    wCursor_None        // < remove the cursor
};
typedef unsigned int wCursor;

#ifndef __ANDROID__
void wSetWindowSize(int width, int height);
void wSetWindowPosition(int x, int y);
void wSetWindowMoveCallback(void(*callback)(int, int));
void wSetWindowName(const char* name);
void wGetWindowPos(int* x, int* y);

// Sets Window as Full Screen, with given resolution, this might improve performance but pixel density drops down
// You can set the resolution using GetMonitorSize or following resolutions: 
// 2560x1440, 1920x1080, 1280×720 etc.
bool wEnterFullscreen(int fullscreenWidth, int fullscreenHeight);

// Go back to full screen Mode
bool wExitFullscreen(int windowX, int windowY, int windowedWidth, int windowedHeight);

bool wOpenFolder(const char* folderPath);

bool wOpenFile(const char* filePath);

void wSetCursor(wCursor cursor);

// android only functions
inline void wShowKeyboard(bool value) { }
inline void wVibrate(long miliseconds){ }

#else
// These functions are not used in android code, we are inlining here
// this way compiler will not use this functions
// desktop device only functions
inline void wSetWindowSize(int width, int height) {}
inline void wSetWindowPosition(int x, int y) {}
inline void wSetWindowMoveCallback(void(*callback)(int, int)) {}
inline void wSetWindowName(const char* name) {}
inline void wGetWindowPos(int* x, int* y) { *x=0; *y=0;}
inline bool wEnterFullscreen(int fullscreenWidth, int fullscreenHeight) { return false; }
inline bool wExitFullscreen(int windowX, int windowY, int windowedWidth, int windowedHeight) { return false; }
inline void SetMousePos(float x, float y) {}
inline void SetMouseWindowPos(float x, float y) {}
inline bool wOpenFolder(const char* folderPath) { return false; } // < not implemented
bool wOpenFile(const char* filePath){}
inline void wSetCursor(wCursor cursor) { }

void wShowKeyboard(bool value); // shows keyboard on android devices
void wVibrate(long miliseconds);
#endif

void wOpenURL(const char* url);
void wSetVSync(bool active);

void wSetFocusChangedCallback(void(*callback)(bool focused));
void wSetWindowResizeCallback(void(*callback)(int, int));
void wSetKeyPressCallback(void(*callback)(unsigned));
void wSetMouseMoveCallback(void(*callback)(float, float));

void wGetWindowSize(int* x, int* y);
void wGetMonitorSize(int* width, int* height);
void wRequestQuit();

const char* wGetClipboardString();
bool wSetClipboardString(const char* str); // returns true if success

//------------------------------------------------------------------------
//  Audio

typedef int ASound;
int LoadSound(const char* path);
void SetGlobalVolume(float v);
void SoundPlay(ASound sound);
void SoundRewind(ASound sound); // seeks to beginning of the sound
void SoundSetVolume(ASound sound, float volume);
void SoundDestroy(ASound sound);

//------------------------------------------------------------------------
//  Keyboard 

// use enum Key_... or asci char 'X'
bool GetKeyDown(char c);

bool GetKeyPressed(char c);

bool GetKeyReleased(char c);

bool AnyKeyDown();

//------------------------------------------------------------------------
//  Mouse

//  Mouse is finger in Android, and MouseButton is finger id.
enum MouseButton_
{
    MouseButton_Left    = 1, /* same -> */ MouseButton_Touch0 = 1,
    MouseButton_Right   = 2, /* same -> */ MouseButton_Touch1 = 2,
    MouseButton_Middle  = 4, /* same -> */ MouseButton_Touch2 = 4,
    MouseButton_Forward  = 8, 
    MouseButton_Backward = 16, 
};
typedef int MouseButton;

bool AnyMouseKeyDown();
bool IsDoubleClick();
bool AnyNumberPressed();
int GetPressedNumber();

bool GetMouseDown(MouseButton button);
bool GetMouseReleased(MouseButton button);
bool GetMousePressed(MouseButton button);

void GetMousePos(float* x, float* y);
void GetMouseWindowPos(float* x, float* y);
float GetMouseWheelDelta();

struct Touch
{
    float positionX;
    float positionY;
};

#ifdef __ANDROID__
Touch GetTouch(int index);
int NumTouchPressing();
#else
// with android these two functions belove are inlined at the end of this file
void SetMousePos(float x, float y);
void SetMouseWindowPos(float x, float y);

inline Touch GetTouch(int index) 
{
    Touch touch;
    GetMousePos(&touch.positionX, &touch.positionY);
    return touch; 
}

inline int NumTouchPressing() { 
    return (int)GetMouseDown(0) + (int)GetMouseDown(1); 
}
#endif

//------------------------------------------------------------------------
//  TIME

double GetDeltaTime();
double TimeSinceStartup();

/*
* keys are from vk_key in windows
* KeyboardKey_0 - KeyboardKey_9 are the same as ASCII '0' - '9' (0x30 - 0x39)
* KeyboardKey_A - KeyboardKey_Z are the same as ASCII 'A' - 'Z' (0x41 - 0x5A)
*/

// Todo: we will need different enum values for each platform

enum KeyboardKey_
{
    Key_MouseBack  = 0x05, 
    Key_BACK       = 0x08,   
    Key_TAB        = 0x09,   
    Key_CLEAR      = 0x0C,   
    Key_RETURN     = 0x0D, Key_ENTER = 0x0D,
    Key_SHIFT      = 0x10,   
    Key_CONTROL    = 0x11,   
    Key_MENU       = 0x12, // alt key
    Key_PAUSE      = 0x13,   
    Key_CAPITAL    = 0x14,   
    Key_ESCAPE     = 0x1B,   
    Key_CONVERT    = 0x1C,   
    Key_NONCONVERT = 0x1D,   
    Key_ACCEPT     = 0x1E,   
    Key_SNAPSHOT   = 0x2C,   
    Key_INSERT     = 0x2D,   
    Key_DELETE     = 0x2E,   
    Key_HELP       = 0x2F,   
    Key_LWIN       = 0x5B,   
    Key_RWIN       = 0x5C,   
    Key_APPS       = 0x5D,   
    Key_SLEEP      = 0x5F,   
    
    Key_NUMPAD0    = 0x60,   
    Key_NUMPAD1    = 0x61,   
    Key_NUMPAD2    = 0x62,   
    Key_NUMPAD3    = 0x63,   
    Key_NUMPAD4    = 0x64,   
    Key_NUMPAD5    = 0x65,   
    Key_NUMPAD6    = 0x66,   
    Key_NUMPAD7    = 0x67,   
    Key_NUMPAD8    = 0x68,   
    Key_NUMPAD9    = 0x69,

    Key_MODECHANGE = 0x1F,
    Key_SPACE      = 0x20,
    Key_PRIOR      = 0x21,
    Key_NEXT       = 0x22,
    Key_END        = 0x23,
    Key_HOME       = 0x24,
    Key_LEFT       = 0x25,
    Key_UP         = 0x26,
    Key_RIGHT      = 0x27,
    Key_DOWN       = 0x28,
    Key_SELECT     = 0x29,
    Key_PRINT      = 0x2A,
    Key_EXECUTE    = 0x2B,
    Key_MULTIPLY   = 0x6A,
    Key_ADD        = 0x6B,
    Key_SEPARATOR  = 0x6C,
    Key_SUBTRACT   = 0x6D,
    Key_DECIMAL    = 0x6E,
    Key_DIVIDE     = 0x6F,

    Key_F1         = 0x70,
    Key_F2         = 0x71,
    Key_F3         = 0x72,
    Key_F4         = 0x73,
    Key_F5         = 0x74,
    Key_F6         = 0x75,
    Key_F7         = 0x76,
    Key_F8         = 0x77,
    Key_F9         = 0x78,
    Key_F10        = 0x79,
    Key_F11        = 0x7A,
    Key_F12        = 0x7B
};
typedef int KeyboardKey;

