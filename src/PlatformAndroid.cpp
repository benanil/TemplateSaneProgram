
/********************************************************************
*    Purpose: Touch Input, Surface Creation, Main Loop              *
*    Author : Anilcan Gulkaya 2023 anilcangulkaya7@gmail.com        *
********************************************************************/
#if defined(__ANDROID__)

#include "include/Platform.hpp"
#include "include/Renderer.hpp"

#include "../ASTL/Array.hpp"
#include "../ASTL/Memory.hpp"
#include "../ASTL/Algorithms.hpp" // IntToString
#include "../ASTL/IO.hpp" // IntToString

#include <EGL/egl.h>
#include <GLES3/gl32.h>

#include <time.h> // gettime
#include <jni.h>
#include <game-activity/GameActivity.cpp>
#include <game-text-input/gametextinput.cpp>

#define MINIAUDIO_IMPLEMENTATION
#define MA_ENABLE_ONLY_SPECIFIC_BACKENDS
#define MA_NO_ENCODING /* read audio files only, for now */
#define MA_NO_GENERATION
#define MA_ENABLE_AAUDIO

#include "../External/miniaudio.h"
#include "../External/swappyGL.h"

constexpr int NumTouch = 4;

struct PlatformContextAndroid
{
    // Callbacks
    void(*WindowResizeCallback)(int  , int) ;
    void(*MouseMoveCallback)   (float, float);
    void(*KeyPressCallback)    (unsigned);
    void(*FocusChangedCallback)(bool);

    EGLDisplay Display;
    EGLSurface Surface;
    EGLContext Context;
    EGLConfig Config;

    // Window
    int WindowWidth;
    int WindowHeight;

    uint64 StartTime;
    double DeltaTime;

    bool VSyncActive;
    bool RendererInitialized;
    bool ShouldRender;
    bool ShouldClose;

    Touch Fingers[NumTouch];

    // bitmasks for finger states(touch)
    int FingerDown;
    int FingerReleased;
    int FingerPressed;
} PlatformCtx={};

StackArray<ma_sound, 16> mSounds={};
JNIEnv* mAppJniEnv = nullptr;

void wSetFocusChangedCallback(void(*callback)(bool focused)) { PlatformCtx.FocusChangedCallback = callback; }
void wSetWindowResizeCallback(void(*callback)(int, int))     { PlatformCtx.WindowResizeCallback = callback;}
void wSetKeyPressCallback(void(*callback)(unsigned))         { PlatformCtx.KeyPressCallback     = callback; }
void wSetMouseMoveCallback(void(*callback)(float, float))    { PlatformCtx.MouseMoveCallback    = callback;}

void wGetWindowSize(int* x, int* y)           { *x = PlatformCtx.WindowWidth;  *y = PlatformCtx.WindowHeight; }
void wGetMonitorSize(int* width, int* height) { *width = PlatformCtx.WindowWidth; *height = PlatformCtx.WindowHeight; }

const char* wGetClipboardString() {
    return nullptr;
}

bool wSetClipboardString(const char* string) {
    return false;
}

bool AnyNumberPressed() {
    return 0;
}

int GetPressedNumber() {
    return -1;
}

void wSetVSync(bool active)
{
    PlatformCtx.VSyncActive = active;
}

void wOpenURL(const char* url)
{
    JNIEnv* env = mAppJniEnv;
    jstring urlString = (env)->NewStringUTF(url);
    jclass uriClass = (env)->FindClass("android/net/Uri");
    jmethodID uriParse = (env)->GetStaticMethodID(uriClass, "parse", "(Ljava/lang/String;)Landroid/net/Uri;");
    jobject uri = (env)->CallStaticObjectMethod(uriClass, uriParse, urlString);

    jclass intentClass = (env)->FindClass("android/content/Intent");
    jfieldID actionViewId = (env)->GetStaticFieldID(intentClass, "ACTION_VIEW", "Ljava/lang/String;");
    jobject actionView = (env)->GetStaticObjectField(intentClass, actionViewId);
    jmethodID newIntent = (env)->GetMethodID(intentClass, "<init>", "(Ljava/lang/String;Landroid/net/Uri;)V");
    jobject intent = (env)->AllocObject(intentClass);

    (env)->CallVoidMethod(intent, newIntent, actionView, uri);
    jclass activityClass = (env)->FindClass("android/app/Activity");
    jmethodID startActivity = (env)->GetMethodID(activityClass, "startActivity", "(Landroid/content/Intent;)V");
    jobject mainActivityClass = g_android_app->activity->javaGameActivity;
    (env)->CallVoidMethod(mainActivityClass, startActivity, intent);
}

void wVibrate(long miliseconds)
{
    JNIEnv* env = mAppJniEnv;
    // Get the class object for MainActivity
    jclass mainActivityClass = env->GetObjectClass(g_android_app->activity->javaGameActivity);
    jmethodID vibrateMethod = env->GetMethodID(mainActivityClass, "vibrate", "(J)V");
    if (vibrateMethod == nullptr) return;

    jobject javaGameActivity = g_android_app->activity->javaGameActivity;
    env->CallVoidMethod(javaGameActivity, vibrateMethod, miliseconds);
}

purefn bool SwapBuffers(EGLDisplay display, EGLSurface surface)
{
    if (PlatformCtx.VSyncActive) return SwappyGL_swap(display, surface);
    else /* no vsync */          return eglSwapBuffers(display, surface);
}

//------------------------------------------------------------------------
// Audio
ma_engine maEngine;

ma_engine* GetMAEngine() {
    return &maEngine;
}

int LoadSound(const char* path)
{
    if (!FileExist(path)) AX_WARN("sound file is not exist!");
    mSounds.AddUninitialized(1);
    uint soundFlag = MA_SOUND_FLAG_NO_SPATIALIZATION | MA_SOUND_FLAG_NO_PITCH;
    // in order to work with miniaudio, we have to use fopen,
    // that's why  we have to extract audio files from aasset manager to internal path
    const char* internalDataPath = g_android_app->activity->internalDataPath;
    char internalPath[512] = {};
    int internalPathSize = StringLength(internalDataPath);
    int pathSize = StringLength(path);
    SmallMemCpy(internalPath, internalDataPath, internalPathSize);
    internalPath[internalPathSize++] = '/';
    SmallMemCpy(internalPath + internalPathSize, path, pathSize);

    ScopedPtr<char> soundFile = ReadAllFile(path);
    FILE* file = fopen(internalPath, "wb");
    fwrite(soundFile.ptr, 1, FileSize(path), file);
    fclose(file);
    [[maybe_unused]] int result = ma_sound_init_from_file(GetMAEngine(), internalPath, soundFlag, nullptr, nullptr, mSounds.Data() + (mSounds.Size()-1));
    return mSounds.Size()-1;
}

void SetGlobalVolume(float v)
{
    for (int i = 0; i < mSounds.Size(); i++)
        ma_sound_set_volume(&mSounds[i], v);
}

void SoundPlay(ASound sound)
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

void UpdateRenderArea()
{
    EGLint width, height;
    eglQuerySurface(PlatformCtx.Display, PlatformCtx.Surface, EGL_WIDTH, &width);
    eglQuerySurface(PlatformCtx.Display, PlatformCtx.Surface, EGL_HEIGHT, &height);
    PlatformCtx.WindowWidth = width;
    PlatformCtx.WindowHeight = height;

    if (PlatformCtx.WindowResizeCallback)
        PlatformCtx.WindowResizeCallback(width, height);
}

/****               Keyboard and Touch                ****/
bool AnyKeyDown() { return PlatformCtx.FingerDown > 0; } // todo: maybe add physical buttons too
bool GetKeyDown(char c)     { return false; }
bool GetKeyPressed(char c)  { return false; }
bool GetKeyReleased(char c) { return false; }

Touch GetTouch(int index) { return PlatformCtx.Fingers[index]; }

int NumTouchPressing() { return PopCount32(PlatformCtx.FingerDown); }

bool AnyMouseKeyDown()  { return PlatformCtx.FingerDown > 0; }
bool GetMouseDown(MouseButton button)     { return !!(PlatformCtx.FingerDown & button); }
bool GetMouseReleased(MouseButton button) { return !!(PlatformCtx.FingerReleased & button); }
bool GetMousePressed(MouseButton button)  { return !!(PlatformCtx.FingerPressed & button); }

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
extern void AXLoop(bool shouldRender);
extern void AXExit();


extern "C" {

#include <game-activity/native_app_glue/android_native_app_glue.c>

android_app* g_android_app = nullptr;


static void CreateSurface()
{    // create the proper window surface
    EGLint format;
    eglGetConfigAttrib(PlatformCtx.Display, PlatformCtx.Config, EGL_NATIVE_VISUAL_ID, &format);
    PlatformCtx.Surface = eglCreateWindowSurface(PlatformCtx.Display, PlatformCtx.Config, g_android_app->window, nullptr);
}

static void DestroySurface()
{
    eglMakeCurrent(PlatformCtx.Display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroySurface(PlatformCtx.Display, PlatformCtx.Surface);
    PlatformCtx.Surface = EGL_NO_SURFACE;
}

static void SetContext()
{
    EGLBoolean madeCurrent = eglMakeCurrent(PlatformCtx.Display, PlatformCtx.Surface, PlatformCtx.Surface, PlatformCtx.Context);
    if (madeCurrent == false) AX_ERROR("make current failed!");
}

static void InitWindow()
{
    // The default display is probably what you want on Android
    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    PlatformCtx.Display = display;
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
    EGLConfig supportedConfigs[32]={};
    eglChooseConfig(display, attribs, supportedConfigs, numConfigs, &numConfigs);

    // Find a config we like. Could likely just grab the first if we don't care about anything else in the config.
    EGLConfig config = nullptr;
    {
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
        PlatformCtx.Config = config;
        AX_LOG("Found %i configs\n", numConfigs);
    }
    // Create a GLES 3 context
    EGLint contextAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
    PlatformCtx.Context = eglCreateContext(display, config, nullptr, contextAttribs);

    CreateSurface();
    SetContext();
    UpdateRenderArea();
}

static void TerminateWindow()
{
    DestroySurface();
    eglDestroyContext(PlatformCtx.Display, PlatformCtx.Context);
    eglTerminate(PlatformCtx.Display);
    PlatformCtx.Display = EGL_NO_DISPLAY;
    PlatformCtx.Context = EGL_NO_CONTEXT;
}

void HandleCMD(android_app *pApp, int32_t cmd)
{
    switch (cmd) {
        case APP_CMD_INIT_WINDOW:
            pApp->userData = (void*)(~0ull);
            g_android_app = pApp;
            if (!PlatformCtx.RendererInitialized)
            {
                PlatformCtx.RendererInitialized = true;
                AXInit();
                InitWindow();
                rInitRenderer();

                if (AXStart() == 0)
                    return; // user defined startup failed
            }
            else
            {
                CreateSurface();
                SetContext();
                UpdateRenderArea();
            }

            if (pApp->window != NULL)
                SwappyGL_setWindow(pApp->window);

            PlatformCtx.ShouldRender = true;
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
            DestroySurface();
            PlatformCtx.ShouldRender = false;
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

void GameTextInputGetStateCB(void *ctx, const GameTextInputState* state) {
    
    static GameTextInputState* lastState = nullptr;
    static int32_t lastSize = 0;
    static unsigned int lastCharacter = 0;
    if (!state) return;
    
    if (state != lastState) {
        lastSize = 0, lastCharacter = 0;
    }

    lastState = (GameTextInputState*)(size_t)state;
    int32_t text_length = state->text_length;
    if (text_length > lastSize)
    {
        unsigned int character = 0;
        int32_t unicodeSize = MAX(text_length - lastSize, 0);
        SmallMemCpy(&character, state->text_UTF8 + lastSize, unicodeSize);
        if (unicodeSize == 2) {
            // swap first two bytes
            character = (character >> 8u) | (0xFFFFu & (character << 8u));
        }
        else if (unicodeSize > 2) {
            #ifdef DEBUG
            AX_WARN("unicode Key contains more than 2 byte, we only handle 2byte unicodes for now");
            #endif
            return;
        }

        if (PlatformCtx.KeyPressCallback && character != lastCharacter)
            PlatformCtx.KeyPressCallback(character), lastCharacter = character;
    }
    lastSize = text_length;
    LOGI("UserInputText: %s", state->text_UTF8);
}


/* This the main entry point for a native activity */
void android_main(android_app *pApp)
{
    if (ma_engine_init(NULL, &maEngine) != MA_SUCCESS) {
        AX_ERROR("mini audio init failed!");
        return;
    }

    // Register an event handler for Android events
    pApp->onAppCmd = HandleCMD;
    // Set input event filters (set it to NULL if the app wants to process all inputs).
    // Note that for key inputs, this example uses the default default_key_filter() implemented in android_native_app_glue.c.
    android_app_set_motion_event_filter(pApp, motion_event_filter_func);

    // This sets up a typical game/event loop. It will run until the app is destroyed.
    int events;
    android_poll_source *pSource;
    pApp->activity->vm->AttachCurrentThread(&mAppJniEnv, NULL);

    // use swappy to avoid tearing and lag. https://developer.android.com/games/sdk/frame-pacing
    [[maybe_unused]] bool swappyFine = SwappyGL_init(mAppJniEnv, pApp->activity->javaGameActivity);
    ASSERT(swappyFine);
    // SwappyGL_setSwapIntervalNS(1000000000L / PreferredFrameRateInHz);
    uint64 currentTime    = PerformanceCounter();
    uint64 prevTime       = currentTime;
    PlatformCtx.StartTime = currentTime;
    PlatformCtx.ShouldClose = false;

    while (pApp->destroyRequested || !PlatformCtx.ShouldClose)
    {
        // Process all pending events before running game logic.
        while (ALooper_pollAll(0, nullptr, &events, (void **) &pSource) >= 0)
        {
            if (pSource) {
                pSource->process(pApp, pSource);
            }

            if (pApp->destroyRequested) {
                goto end_loop;
            }
        }
        if (pApp->textInputState) {
            GameActivity_getTextInputState(g_android_app->activity, GameTextInputGetStateCB, nullptr);
        }

        // Check if any user data is associated. This is assigned in handle_cmd
        if (pApp->userData)
        {
            HandleInput();

            currentTime = PerformanceCounter();
            PlatformCtx.DeltaTime = Clamp01((double)(currentTime - prevTime) / NS_PER_SECOND);
            PlatformCtx.DeltaTime = MIN(PlatformCtx.DeltaTime, 0.1);
            prevTime = currentTime;

            AXLoop(PlatformCtx.ShouldRender);

            if (PlatformCtx.ShouldRender)
            {
                [[maybe_unused]] bool swapped = SwapBuffers(PlatformCtx.Display, PlatformCtx.Surface);
                ASSERT(swapped);
                glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // | GL_STENCIL_BUFFER_BIT
            }

            PlatformCtx.FingerReleased = 0;
            PlatformCtx.FingerPressed = 0;
        }
    } // < main loop

    end_loop:
    {
        AXExit();
        TerminateWindow();
        SwappyGL_destroy();
    };
}

inline uint MapAndroidKeyToWindowsKey(uint key)
{
    if (key >= AKEYCODE_A && key <= AKEYCODE_Z)
        return key - AKEYCODE_A + 'A';
    
    if (key >= AKEYCODE_0 && key <= AKEYCODE_9)
        return key - AKEYCODE_0 + '0';

    switch (key) {
        case AKEYCODE_DEL:    return Key_BACK;
        case AKEYCODE_ENTER:  return Key_ENTER;
        case AKEYCODE_ESCAPE: return Key_ESCAPE;
        case AKEYCODE_TAB:    return Key_TAB;
        case AKEYCODE_DPAD_LEFT:  return Key_LEFT;
        case AKEYCODE_DPAD_RIGHT: return Key_RIGHT;
        case AKEYCODE_DPAD_UP:    return Key_UP;
        case AKEYCODE_DPAD_DOWN:  return Key_DOWN;
        case AKEYCODE_AT: return '@';
        case AKEYCODE_PERIOD: return '.';
    }

    if (key >= AKEYCODE_NUMPAD_0 && key <= AKEYCODE_NUMPAD_9)
        return key - AKEYCODE_NUMPAD_0 + Key_NUMPAD0;

    if (key >= AKEYCODE_F1 && key <= AKEYCODE_12)
        return key - AKEYCODE_F1 + Key_F1;
    #ifdef DEBUG
    AX_WARN("Key is not recognised");
    #endif
    return 0;
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

    // handle input key events.
    for (auto i = 0; i < inputBuffer->keyEventsCount; i++)
    {
        GameActivityKeyEvent& keyEvent = inputBuffer->keyEvents[i];
        AX_LOG("Key: %i ", keyEvent.keyCode);
        uint key = 0;
        switch (keyEvent.action)
        {
            case AKEY_EVENT_ACTION_DOWN:
                AX_LOG("Key Down %i\n", keyEvent.action);
                break;
            case AKEY_EVENT_ACTION_UP:
                AX_LOG("Key Up %i\n", keyEvent.action);
                key = MapAndroidKeyToWindowsKey(keyEvent.keyCode);

                if (PlatformCtx.KeyPressCallback && key != 0) // enter etc. comes here
                    PlatformCtx.KeyPressCallback(key);
                break;
            case AKEY_EVENT_ACTION_MULTIPLE:
                // Deprecated since Android API level 29.
                AX_LOG("Multiple Key Actions %i\n", keyEvent.action);
                break;
            default:
                AX_LOG("Unknown KeyEvent Action: %i \n", keyEvent.action);
        }
    }

    // clear the key input count too.
    android_app_clear_key_events(inputBuffer);
}

} // extern C end

void wRequestQuit() {
    PlatformCtx.ShouldClose = true;
    GameActivity_finish(g_android_app->activity);
}

void wShowKeyboard(bool value)
{
    GameActivity_showSoftInput(g_android_app->activity, 0);
}

#endif // ifdef __ANDROID__
