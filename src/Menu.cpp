
#include "include/UI.hpp"
#include "../ASTL/Algorithms.hpp"
#include "include/Platform.hpp"

enum MenuState_ {
    MenuState_Gameplay,
    MenuState_PauseMenu,
    MenuState_Options
};
typedef int MenuState;

static MenuState menuState = MenuState_Gameplay;
static char logText[32] = {};
static bool wasHovered = false;
static bool isVsyncEnabled = true;
static bool showFPS = true;
static bool showLocation = true; // shows the scene name

static int currentHover = 0;
static bool isAnyHovered = false;
static bool hoveredButtons[3] = {};

static inline void SetLogText(const char* txt, int size)
{
    MemsetZero(logText, sizeof(logText));
    SmallMemCpy(logText, txt, size);
}

inline void HoverEvents(bool* wasHovered, void(*HoverIn)(), void(*HoverOut)())
{
    if (!*wasHovered && uIsHovered()) 
        if (HoverIn != nullptr) HoverIn();

    if (*wasHovered && !uIsHovered()) 
        if (HoverOut != nullptr) HoverOut();
    
    *wasHovered = uIsHovered();
}

static void SetAnyHoveredTrue()  { isAnyHovered = true; }
static void SetAnyHoveredFalse() { isAnyHovered = false; }

static void PauseMenu()
{
    Vector2f buttonSize = {340.0f, 70.0f};
    Vector2f buttonPosition;
    buttonPosition.x = (1920.0f / 2.0f) - (buttonSize.x / 2.0f);
    buttonPosition.y = 500.0f;

    uButtonOptions buttonOpt;
    const char* buttonNames[] = { "Play", "Options", "Quit" };
    const MenuState targetMenus[] = { MenuState_Gameplay, MenuState_Options, MenuState_PauseMenu };
    
    const int numButtons = 3;
    float buttonYPadding = 10.0f;
    int clickedButton = -1;

    for (int i = 0; i < numButtons; i++)
    {
        buttonOpt = !isAnyHovered && currentHover == i ? uButtonOpt_Hovered : 0;
        buttonOpt |= uButtonOpt_Border;
        if (uButton(buttonNames[i], buttonPosition, buttonSize, buttonOpt))
        {
            menuState = targetMenus[i];
            clickedButton = i;
        }
        HoverEvents(hoveredButtons + i, SetAnyHoveredTrue, SetAnyHoveredFalse);
        buttonPosition.y += buttonSize.y + buttonYPadding;
    }

    if (GetKeyPressed('W') || GetKeyPressed(Key_UP)) {
        currentHover = currentHover == 0 ? 2 : currentHover-1;
    }

    if (GetKeyPressed('S') || GetKeyPressed(Key_DOWN)) {
        currentHover = currentHover == 2 ? 0 : currentHover + 1;
    }

    if (GetKeyPressed(Key_ENTER)) {
        menuState = targetMenus[currentHover];
        clickedButton = currentHover;
    }

    // clickedQuit
    if (clickedButton == 2) {
        wRequestQuit();
    }

    uText(logText, MakeVec2(1750.0f, 920.0f));
}

static void OptionsMenu()
{
    Vector2f bgPos;
    Vector2f bgScale = { 1000.0f, 666.0f };
    bgPos.x = (1920.0f / 2.0f) - (bgScale.x / 2.0f);
    bgPos.y = (1080.0f / 2.0f) - (bgScale.y / 2.0f);
    Vector2f pos = bgPos;

    const float textPadding      = 13.0f;
    const float settingsXStart   = 18.0f;
    float elementScale = !IsAndroid() ? 0.8f : 1.25f;
    Vector2f zero2 = { 0.0f, 0.0f };
    
    float settingElementWidth = bgScale.x / 1.25f;
    float elementsXOffset = bgScale.x / 2.0f - (settingElementWidth / 2.0f);

    uPushFloat(ufContentStart, settingElementWidth);

    uQuad(pos, bgScale, uGetColor(uColorQuad));
    uBorder(pos, bgScale);

    uPushFloat(ufTextScale, uGetFloat(ufTextScale) * 1.2f);
    Vector2f textSize = uCalcTextSize("Settings");
    pos.y += textSize.y + textPadding;
    pos.x += settingsXStart;
    uText("Settings", pos);
    uPopFloat(ufTextScale);

    float lineLength = bgScale.x * 0.85f;
    float xoffset = (bgScale.x - lineLength) * 0.5f; // where line starts
    pos.x += xoffset;
    pos.y += 20.0f; // line padding
    pos.x -= settingsXStart;

    uPushColor(uColorLine, uGetColor(uColorSelectedBorder));
    uPushFloat(ufLineThickness, uGetFloat(ufLineThickness) * 0.62f);
        uLineHorizontal(pos, lineLength);
    uPopFloat(ufLineThickness);
    uPopColor(uColorLine);

    pos.x -= xoffset;
    pos.x += elementsXOffset;
    pos.y += textSize.y + textPadding;

    static int CurrElement = 0;
    const int numElements = 10; // number of options plus back button
    
    uPushFloat(ufTextScale, elementScale);
    uSetElementFocused(CurrElement == 0);
    if (uCheckBox("Vsync", &isVsyncEnabled, pos, true))
    {
        wSetVSync(isVsyncEnabled);
    }

    textSize.y = uCalcTextSize("V").y;
    uSetElementFocused(CurrElement == 1);
    pos.y += textSize.y + textPadding;
    uCheckBox("Show Fps", &showFPS, pos, true);
    
    pos.y += textSize.y + textPadding;
    uSetElementFocused(CurrElement == 2);
    uCheckBox("Show Location", &showLocation, pos, true);
    
    pos.y += textSize.y + textPadding;
    static char name[128] = {};
    uSetElementFocused(CurrElement == 3);
    if (uTextBox("Name", pos, zero2, name)) {
        CurrElement = 3;
    }

    pos.y += textSize.y + textPadding;
    static float volume = 0.5f;
    uSetElementFocused(CurrElement == 4);
    if (uSlider("Volume", pos, &volume, uGetFloat(ufTextBoxWidth))) {
        CurrElement = 4;
    }

    const char* graphicsNames[] = { "Low" , "Medium", "High", "Ultra" };
    static int CurrentGraphics = 0;
    pos.y += textSize.y + textPadding;

    uSetElementFocused(CurrElement == 5);
    int selectedGraphics = uChoice("Graphics", pos, graphicsNames, ArraySize(graphicsNames), CurrentGraphics);
    if (selectedGraphics != CurrentGraphics) { // element changed
        CurrElement = 5;
    }
    CurrentGraphics = selectedGraphics;

    pos.y += textSize.y + textPadding;
    uSetElementFocused(CurrElement == 6);
    static int numFrames = 144;
    if (uIntField("Num Frames", pos, &numFrames)) {
        CurrElement = 6;
    }

    pos.y += textSize.y + textPadding;
    uSetElementFocused(CurrElement == 7);
    static float senstivity = 1.0f;
    if (uFloatField("Senstivity", pos, &senstivity, -16.0f, 128.0f, 0.05f)) {
        CurrElement = 7;
    }

    static Vector2i Resolution = { 1920, 1080 };
    static int vecIndex = 0;
    pos.y += textSize.y + textPadding;
    uSetElementFocused(CurrElement == 8);
    if (uIntVecField("Resolution", pos, Resolution.arr, 2, &vecIndex)) {
        CurrElement = 8;
    }
    if (vecIndex == 2) CurrElement = 9; // pressed tab

    pos = bgPos + bgScale - MakeVec2(100.0f, 100.0f);
    // draw border only if we selected or it is android
    uSetElementFocused(CurrElement == 9);
    uButtonOptions buttonOpt = uButtonOpt_Border | (CurrElement == 9 ? uButtonOpt_Hovered : 0);
    uPushFloat(ufTextScale, uGetFloat(ufTextScale) * 0.8f);
    if (uButton("Back", pos, zero2, buttonOpt)) {
        menuState = MenuState_PauseMenu;
    }
    uPopFloat(ufTextScale);
    
    uPopFloat(ufTextScale);
    uPopFloat(ufContentStart);
    
    bool tabPressed = GetKeyPressed(Key_TAB) && CurrElement != 8; // if we are at int field we don't want to increase current element instead we want to go next element in vector field
    if (GetKeyPressed(Key_UP))
        CurrElement = CurrElement == 0 ? numElements - 1 : CurrElement - 1;
    else if (GetKeyPressed(Key_DOWN) || tabPressed)
        CurrElement = CurrElement == numElements - 1 ? 0 : CurrElement + 1;
}

static void ShowFPS()
{
    if (!showFPS) return;

    static int fps = 60;
    static char fpsTxt[16] = {'6', '0'};

    double timeSinceStart = TimeSinceStartup();
    if (timeSinceStart - (float)int(timeSinceStart) < 0.1)
    {
        double dt = GetDeltaTime();
        fps = (int)(1.0 / dt);
        IntToString(fpsTxt, fps);
    }

    uText(fpsTxt, MakeVec2(15.0f, 85.0f));
}

void ShowMenu()
{
    uBegin();

    ShowFPS();

    if (IsAndroid() && menuState == MenuState_Gameplay) {
        uSetFloat(ufTextScale, 1.125f);
        if (uButton(IC_PAUSE, MakeVec2(1880.0f, 30.0f), Vector2f::Zero(), uButtonOpt_Border))
        {
            menuState = MenuState_PauseMenu;
        }
    }

    if (showLocation) {
        // write to left bottom side of the screen
        uText("Cratoria: Dubrovnik-Sponza", MakeVec2(100.0f, 950.0f));
    }

    switch (menuState)
    {
        case MenuState_Options: OptionsMenu(); break;
        case MenuState_PauseMenu: PauseMenu(); break;
    };

    if (GetKeyPressed(Key_ESCAPE))
    {
        switch (menuState)
        {
            case MenuState_Options:   menuState = MenuState_PauseMenu; break;
            case MenuState_PauseMenu: menuState = MenuState_Gameplay;  break;
            case MenuState_Gameplay:  menuState = MenuState_PauseMenu; break;
        };
        currentHover = 0;
    }

    uRender();
}