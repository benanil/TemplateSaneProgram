
/********************************************************************
*    Purpose: Touch Input, Surface Creation, Main Loop              *
*    Author : Anilcan Gulkaya 2023 anilcangulkaya7@gmail.com        *
********************************************************************/
#if defined(__ANDROID__) 

#include "Platform.hpp"
#include "Renderer.hpp"
#include "../ASTL/Memory.hpp"
#include "../ASTL/Algorithms.hpp" // IntToString

#include <EGL/egl.h>
#include <GLES3/gl32.h>

#include <time.h> // gettime
#include <jni.h>
#include <game-activity/GameActivity.cpp>
#include <game-text-input/gametextinput.cpp>

constexpr int NumTouch = 4;

struct PlatformContextAndroid
{
    // Callbacks
    void(*WindowResizeCallback)(int  , int)   = nullptr;
    void(*MouseMoveCallback)   (float, float) = nullptr;
    void(*KeyPressCallback)    (wchar_t)      = nullptr;
    void(*FocusChangedCallback)(bool)         = nullptr;
    
    EGLDisplay Display = EGL_NO_DISPLAY;
    EGLSurface Surface = EGL_NO_SURFACE;
    EGLContext Context = EGL_NO_CONTEXT;
    
    // Window
    int WindowWidth  = 0;
    int WindowHeight = 0;

    uint64 StartTime;
    double DeltaTime;

    bool VSyncActive = true;

    Touch Fingers[NumTouch]{};
    
    // bitmasks for finger states(touch)
    int FingerDown = 0;
    int FingerReleased = 0;
    int FingerPressed = 0;

} PlatformCtx{};

void wSetFocusChangedCallback(void(*callback)(bool focused)) { PlatformCtx.FocusChangedCallback = callback; }
void wSetWindowResizeCallback(void(*callback)(int, int))     { PlatformCtx.WindowResizeCallback = callback;}
void SetKeyPressCallback(void(*callback)(wchar_t))          { PlatformCtx.KeyPressCallback     = callback; }
void SetMouseMoveCallback(void(*callback)(float, float))    { PlatformCtx.MouseMoveCallback    = callback;}

void wGetWindowSize(int* x, int* y)           { *x = PlatformCtx.WindowWidth;  *y = PlatformCtx.WindowHeight; }
void wGetMonitorSize(int* width, int* height) { *width = PlatformCtx.WindowWidth; *height = PlatformCtx.WindowHeight; }

void UpdateRenderArea()
{
    EGLint width, height;
    eglQuerySurface(PlatformCtx.Display, PlatformCtx.Surface, EGL_WIDTH, &width);
    eglQuerySurface(PlatformCtx.Display, PlatformCtx.Surface, EGL_HEIGHT, &height);
    PlatformCtx.WindowWidth  = width;
    PlatformCtx.WindowHeight = height;
    if (PlatformCtx.WindowResizeCallback)
        PlatformCtx.WindowResizeCallback(width, height);
}

/****               Keyboard and Touch                ****/
bool GetKeyDown(char c)     { return false; }
bool GetKeyPressed(char c)  { return false; }
bool GetKeyReleased(char c) { return false; }

Touch GetTouch(int index) { return PlatformCtx.Fingers[index]; }

int NumTouchPressed() { return PopCount(PlatformCtx.FingerDown); }

bool GetMouseDown(MouseButton button)     { return !!(PlatformCtx.FingerDown & 0b11); }
bool GetMouseReleased(MouseButton button) { return !!(PlatformCtx.FingerReleased & 0b11); }
bool GetMousePressed(MouseButton button)  { return !!(PlatformCtx.FingerPressed & 0b11); }

void GetMousePos(float* x, float* y) { *x = PlatformCtx.Fingers[0].positionX; *y = PlatformCtx.Fingers[0].positionY; }
void GetMouseWindowPos(float* x, float* y) { *x = PlatformCtx.Fingers[0].positionX; *y = PlatformCtx.Fingers[0].positionY; }
float GetMouseWheelDelta() { return 0.0f; }

/****                     Time                        ****/

constexpr long long NS_PER_SECOND = 1000000000LL;

static uint64 PerformanceCounter()
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return now.tv_sec * NS_PER_SECOND  + now.tv_nsec;
}

double TimeSinceStartup()
{
    return (double)(PerformanceCounter() - PlatformCtx.StartTime) / NS_PER_SECOND;
}

double GetDeltaTime()
{
    return PlatformCtx.DeltaTime;
}

extern void AXInit();

extern int  AXStart();
extern void AXLoop();
extern void AXExit();


extern "C" {

#include <game-activity/native_app_glue/android_native_app_glue.c>

android_app* g_android_app = nullptr;

static void InitWindow()
{
    // The default display is probably what you want on Android
    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    EGLint majorVersion = 0, minorVersion = 0;
    eglInitialize(display, &majorVersion, &minorVersion);

    const EGLint attribs[] = { EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
                               EGL_SURFACE_TYPE   , EGL_WINDOW_BIT,
                               EGL_BLUE_SIZE      , 8,
                               EGL_GREEN_SIZE     , 8,
                               EGL_RED_SIZE       , 8,
                               EGL_DEPTH_SIZE     , 24,
                               EGL_SAMPLE_BUFFERS , 1, // enable msaa
                               EGL_SAMPLES        , 4, // 4x msaa
                               EGL_NONE };
    // figure out how many configs there are
    EGLint numConfigs;
    eglChooseConfig(display, attribs, nullptr, 0, &numConfigs);
    // get the list of configurations
    EGLConfig supportedConfigs[32]{};
    eglChooseConfig(display, attribs, supportedConfigs, numConfigs, &numConfigs);

    // Find a config we like.
    // Could likely just grab the first if we don't care about anything else in the config.
    // Otherwise hook in your own heuristic
    EGLConfig config = nullptr;
    for (int i = 0; i < numConfigs; i++)
    {
        EGLint red, green, blue, depth;

        config = supportedConfigs[i];
        if (eglGetConfigAttrib(display, config, EGL_RED_SIZE, &red)
            && eglGetConfigAttrib(display, config, EGL_GREEN_SIZE, &green)
            && eglGetConfigAttrib(display, config, EGL_BLUE_SIZE, &blue)
            && eglGetConfigAttrib(display, config, EGL_DEPTH_SIZE, &depth))
        if (red == 8 && green == 8 && blue == 8 && depth == 24)
        break;
    }
    AX_LOG("Found %i configs\n", numConfigs);

    // create the proper window surface
    EGLint format;
    eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &format);
    EGLSurface surface = eglCreateWindowSurface(display, config, g_android_app->window, nullptr);

    // Create a GLES 3 context
    EGLint contextAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
    EGLContext context = eglCreateContext(display, config, nullptr, contextAttribs);

    // get some window metrics
    EGLBoolean madeCurrent = eglMakeCurrent(display, surface, surface, context);
    if (madeCurrent == false) AX_ERROR("make current failed!");

    PlatformCtx.Display = display;
    PlatformCtx.Surface = surface;
    PlatformCtx.Context = context;

    UpdateRenderArea();
}

static void TerminateWindow()
{
    eglMakeCurrent(PlatformCtx.Display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroyContext(PlatformCtx.Display, PlatformCtx.Context);
    eglDestroySurface(PlatformCtx.Display, PlatformCtx.Surface);
    eglTerminate(PlatformCtx.Display);
    PlatformCtx.Display = EGL_NO_DISPLAY;
    PlatformCtx.Surface = EGL_NO_SURFACE;
    PlatformCtx.Context = EGL_NO_CONTEXT;
}

void HandleCMD(android_app *pApp, int32_t cmd)
{
    switch (cmd) {
        case APP_CMD_INIT_WINDOW:
            pApp->userData = (void*)(~0ull);
            g_android_app = pApp;
            AXInit();
            InitWindow();
            rInitRenderer();
            if (AXStart() == 0)
                return; // user defined startup failed
            break;
        case APP_CMD_GAINED_FOCUS:
        case APP_CMD_LOST_FOCUS:
            if (PlatformCtx.FocusChangedCallback)
                PlatformCtx.FocusChangedCallback(cmd == APP_CMD_GAINED_FOCUS);
            break;
        case APP_CMD_WINDOW_RESIZED:
            UpdateRenderArea();
            break;
        case APP_CMD_TERM_WINDOW:
            rDestroyRenderer();
            pApp->userData = 0;
            g_android_app  = nullptr;
        break;
        default:
            break;
    }
}

/* Enable the motion events you want to handle; not handled events are,                   *
 * passed back to OS for further processing. For this example case, only pointer enabled. */
bool motion_event_filter_func(const GameActivityMotionEvent *motionEvent)
{
    int sourceClass = motionEvent->source & AINPUT_SOURCE_CLASS_MASK;
    return sourceClass == AINPUT_SOURCE_CLASS_POINTER;
}

static void HandleInput();

/* This the main entry point for a native activity */
void android_main(android_app *pApp)
{
    // Register an event handler for Android events
    pApp->onAppCmd = HandleCMD;
    // Set input event filters (set it to NULL if the app wants to process all inputs).
    // Note that for key inputs, this example uses the default default_key_filter() implemented in android_native_app_glue.c.
    android_app_set_motion_event_filter(pApp, motion_event_filter_func);
    
    // This sets up a typical game/event loop. It will run until the app is destroyed.
    int events;
    android_poll_source *pSource;

    uint64 currentTime    = PerformanceCounter();
    uint64 prevTime       = currentTime;
    PlatformCtx.StartTime = currentTime;

    do 
    {
        // Process all pending events before running game logic.
        if (ALooper_pollAll(0, nullptr, &events, (void **) &pSource) >= 0) 
        {
            if (pSource) {
                pSource->process(pApp, pSource);
            }
        }

        // Check if any user data is associated. This is assigned in handle_cmd
        if (pApp->userData)
        {
            HandleInput();

            currentTime = PerformanceCounter();
            PlatformCtx.DeltaTime = (double)(currentTime - prevTime) / NS_PER_SECOND;
            prevTime = currentTime;
            AXLoop();
            
            EGLBoolean swapResult = eglSwapBuffers(PlatformCtx.Display, PlatformCtx.Surface);
            ASSERT(swapResult);

            glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // | GL_STENCIL_BUFFER_BIT
            
            PlatformCtx.FingerReleased = 0;
            PlatformCtx.FingerPressed = 0;
        }
    } while (!pApp->destroyRequested);

    AXExit();
    TerminateWindow();
}

static void HandleInput()
{
    // handle all queued inputs
    android_input_buffer* inputBuffer = android_app_swap_input_buffers(g_android_app);
    if (!inputBuffer) 
        return; // no inputs yet.

    // handle motion events (motionEventsCounts can be 0).
    for (auto i = 0; i < inputBuffer->motionEventsCount; i++)
    {
        GameActivityMotionEvent& motionEvent = inputBuffer->motionEvents[i];
        int32_t action = motionEvent.action;

        // Find the pointer index, mask and bitshift to turn it into a readable value.
        int32_t pointerIndex = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;

        // get the x and y position of this event if it is not ACTION_MOVE.
        GameActivityPointerAxes& pointer = motionEvent.pointers[pointerIndex];
        float x = GameActivityPointerAxes_getX(&pointer);
        float y = GameActivityPointerAxes_getY(&pointer);

        if (pointer.id >= 4)
            continue;

        // determine the action type and process the event accordingly.
        switch (action & AMOTION_EVENT_ACTION_MASK)
        {
            case AMOTION_EVENT_ACTION_DOWN:
            case AMOTION_EVENT_ACTION_POINTER_DOWN:
            {
                PlatformCtx.FingerPressed |= 1 << pointer.id;
                PlatformCtx.FingerDown |= 1 << pointer.id;
                PlatformCtx.Fingers[pointer.id].positionX = x;
                PlatformCtx.Fingers[pointer.id].positionY = y;
                break;
            }
            case AMOTION_EVENT_ACTION_UP:
            case AMOTION_EVENT_ACTION_POINTER_UP:
            {
                PlatformCtx.FingerReleased |= 1 << pointer.id;
                PlatformCtx.FingerDown &= ~(1 << pointer.id);
                break;
            }
            case AMOTION_EVENT_ACTION_MOVE:
            {
                // There is no pointer index for ACTION_MOVE, only a snapshot of
                // all active pointers; app needs to cache previous active pointers
                // to figure out which ones are actually moved.
                for (auto index = 0; index < (motionEvent.pointerCount & 7); index++) // &7 because max 4 button
                {
                    pointer = motionEvent.pointers[index];
                    PlatformCtx.Fingers[pointer.id].positionX = GameActivityPointerAxes_getX(&pointer);
                    PlatformCtx.Fingers[pointer.id].positionY = GameActivityPointerAxes_getY(&pointer);
                }
                break;
            }
            default:
                AX_LOG("Unknown MotionEvent Action: %i", action);
        }
    }
    // clear the motion input count in this buffer for main thread to re-use.
    android_app_clear_motion_events(inputBuffer);

    /*
    // handle input key events.
    for (auto i = 0; i < inputBuffer->keyEventsCount; i++)
    {
        auto& keyEvent = inputBuffer->keyEvents[i];
        AX_LOG("Key: %i ", keyEvent.keyCode);
        switch (keyEvent.action)
        {
            case AKEY_EVENT_ACTION_DOWN: AX_LOG("Key Down %i\n", keyEvent.action); break;
            case AKEY_EVENT_ACTION_UP:   AX_LOG("Key Up %i\n", keyEvent.action); break;
            case AKEY_EVENT_ACTION_MULTIPLE:
                // Deprecated since Android API level 29.
                AX_LOG("Multiple Key Actions %i\n", keyEvent.action);
                break;
            default:
                AX_LOG("Unknown KeyEvent Action: %i \n", keyEvent.action);
        }
    }
    */

    // clear the key input count too.
    android_app_clear_key_events(inputBuffer);
}

} // extern C end

#endif // ifdef __ANDROID__