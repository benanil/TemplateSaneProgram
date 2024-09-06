
#include "include/Menu.hpp"
#include "include/UI.hpp"
#include "include/Platform.hpp"

#include "../ASTL/Algorithms.hpp"
#include "../ASTL/Math/Color.hpp"

static MenuState menuState = MenuState_Gameplay;
static char logText[32] = {};
static bool wasHovered = false;
static bool isVsyncEnabled = true;
static bool showFPS = true;
static bool showDetails = false; // shows the scene name

static int currentHover = 0;
static bool isAnyHovered = false;
static bool hoveredButtons[3] = {};

static const uint ButtonEffects = uFadeBit | uCenterFadeBit | uFadeInvertBit;

MenuState GetMenuState()
{
    return menuState;
}

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
        buttonOpt |= uButtonOpt_Border | ButtonEffects;
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
        PlayButtonHoverSound();
    }

    if (GetKeyPressed('S') || GetKeyPressed(Key_DOWN)) {
        currentHover = currentHover == 2 ? 0 : currentHover + 1;
        PlayButtonHoverSound();
    }

    if (GetKeyPressed(Key_ENTER)) {
        menuState = targetMenus[currentHover];
        clickedButton = currentHover;
        PlayButtonClickSound();
    }

    // clickedQuit
    if (clickedButton == 2) {
        wRequestQuit();
    }

    uText(logText, Vec2(1750.0f, 920.0f));
}

static void OptionsMenu()
{
    Vector2f bgPos;
    Vector2f bgScale = { 940.0f, 766.0f };
    bgPos.x = (1920.0f / 2.0f) - (bgScale.x / 2.0f);
    bgPos.y = (1080.0f / 2.0f) - (bgScale.y / 2.0f);
    Vector2f pos = bgPos;

    const float textPadding = 13.0f;
    float elementScale = !IsAndroid() ? 0.8f : 1.25f;
    Vector2f zero2 = { 0.0f, 0.0f };
    
    float settingElementWidth = bgScale.x / 1.4f;
    float elementsXOffset = bgScale.x / 2.0f - (settingElementWidth / 2.0f);
    Vector2f textSize = uCalcTextSize("Settings");

    uPushFloat(uf::ContentStart, settingElementWidth);

    uQuad(pos, bgScale, uGetColor(uColor::Quad));
    uBorder(pos, bgScale);

    uPushFloat(uf::TextScale, uGetFloat(uf::TextScale) * 1.2f);
    float settingsXStart = (bgScale.x/2.0f) - (textSize.x/2.0f);
    pos.y += textSize.y + textPadding;
    pos.x += settingsXStart;
    uText("Settings", pos);
    uPopFloat(uf::TextScale);

    float lineLength = bgScale.x * 0.85f;
    float xoffset = (bgScale.x - lineLength) * 0.5f; // where line starts
    pos.x += xoffset;
    pos.y += 20.0f; // line padding
    pos.x -= settingsXStart;

    uPushColor(uColor::Line, uGetColor(uColor::SelectedBorder));
        uLineHorizontal(pos, lineLength, uFadeBit | uCenterFadeBit | uIntenseFadeBit);
    uPopColor(uColor::Line);

    pos.x -= xoffset;
    pos.x += elementsXOffset;
    pos.y += textSize.y + textPadding;

    static int CurrElement = 0;
    const int numElements = 11; // number of options plus back button
    
    uPushFloat(uf::TextScale, elementScale);
    uSetElementFocused(CurrElement == 0);
    if (uCheckBox("Vsync", pos, &isVsyncEnabled, true))
    {
        wSetVSync(isVsyncEnabled);
    }

    textSize.y = uCalcTextSize("V").y;
    uSetElementFocused(CurrElement == 1);
    pos.y += textSize.y + textPadding;
    uCheckBox("Show Fps", pos, &showFPS, true);
    
    pos.y += textSize.y + textPadding;
    uSetElementFocused(CurrElement == 2);
    uCheckBox("Show Details", pos, &showDetails, true);
    
    pos.y += textSize.y + textPadding;
    static char name[128] = {};
    uSetElementFocused(CurrElement == 3);
    if (uTextBox("Name", pos, zero2, name)) {
        CurrElement = 3;
    }

    pos.y += textSize.y + textPadding;
    static float volume = 0.5f;
    uSetElementFocused(CurrElement == 4);
    if (uSlider("Volume", pos, &volume, uGetFloat(uf::TextBoxWidth))) {
        CurrElement = 4;
        SetGlobalVolume(volume);
    }

    const char* graphicsNames[] = { "Low" , "Medium", "High", "Ultra" };
    static int CurrentGraphics = 0;
    pos.y += textSize.y + textPadding;

    uSetElementFocused(CurrElement == 5); // uChoice function does the same thing, but it is not expanding, 
    int selectedGraphics = uDropdown("Graphics", pos, graphicsNames, ArraySize(graphicsNames), CurrentGraphics);
    if (selectedGraphics != CurrentGraphics) { // element changed
        CurrElement = 5;
    }
    CurrentGraphics = selectedGraphics;

    pos.y += textSize.y + textPadding;
    uSetElementFocused(CurrElement == 6);
    static int numFrames = 144;
    static float numFramesHoverTime = 1.0f;
    if (uIntField("Num Frames", pos, &numFrames)) {
        CurrElement = 6;
    }

    numFramesHoverTime = 
    uToolTip("target number of frames that\n"
             "will shown in one second", numFramesHoverTime, uIsHovered());

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
    
    uSetElementFocused(CurrElement == 9);
    pos.y += textSize.y + textPadding;
    static uint color = 0xFFC58A44u;
    if (uColorField("Color", pos, &color)) {
        CurrElement = 9;
    }

    pos = bgPos + bgScale - Vec2(100.0f, 100.0f);
    // draw border only if we selected or it is android
    uSetElementFocused(CurrElement == 10);
    uButtonOptions buttonOpt = uButtonOpt_Border | (CurrElement == 10 ? uButtonOpt_Hovered : 0);
    uPushFloat(uf::TextScale, uGetFloat(uf::TextScale) * 0.8f);
    if (uButton("Back", pos, zero2, buttonOpt)) {
        menuState = MenuState_PauseMenu;
    }
    uPopFloat(uf::TextScale);
    
    uPopFloat(uf::TextScale);
    uPopFloat(uf::ContentStart);
    
    bool tabPressed = GetKeyPressed(Key_TAB) && CurrElement != 8; // if we are at int field we don't want to increase current element instead we want to go next element in vector field
    if (GetKeyPressed(Key_UP))
        CurrElement = CurrElement == 0 ? numElements - 1 : CurrElement - 1, PlayButtonHoverSound();
    else if (GetKeyPressed(Key_DOWN) || tabPressed)
        CurrElement = CurrElement == numElements - 1 ? 0 : CurrElement + 1, PlayButtonHoverSound();
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

    uText(fpsTxt, Vec2(1810.0f, 85.0f));
}

static void TriangleTest()
{
    if (!showDetails)
        return;

    const uint color0 = 0xFF4444FDu, color1 = 0xFF008CFAu, color2 = 0xFF44FD44u;
    static uint8 cutStart = 0, even = 0;
    Vector2f circlePos = Vec2(1520.0f, 540.0f);

    even ^= 1;
    if (even) cutStart++;

    const uint numSegments = 0;
    uint properties = MakeTriProperty(uCutBit, cutStart, numSegments);
    uCircle(circlePos, 25.0f, color0, properties); 
    circlePos.x += 55.0f;

    properties |= uEmptyInsideBit;
    uCircle(circlePos, 25.0f, color0, properties); 
    circlePos.x += 55.0f;
    
    properties |= uFadeInvertBit;
    uCircle(circlePos, 25.0f, color0, properties);
    circlePos.x -= 55.0f * 3.0f;
    
    circlePos.y += 45.0f;
    uCapsule(circlePos, 15.0f, 200.0f, color1, properties);  circlePos.y += 45.0f;

    Vector2f quadPos = circlePos;
    Vector2f quadSize = Vec2(200.0f, 15.0f);
    // test with uquad
    uint color = PackColorToUint(35, 181, 30, 255);
    uRoundedRectangle(quadPos, 50.0f, 50.0f, color, uTriEffect_None);
    quadPos.x += 60.0f;
    uRoundedRectangle(quadPos, 50.0f, 50.0f, color, uFadeBit);
    quadPos.x += 60.0f;
    uRoundedRectangle(quadPos, 50.0f, 50.0f, ~0u, uFadeBit | uFadeInvertBit);
    quadPos.x -= 60.0f * 2.0;
    quadPos.y += 65.0f;
    
    properties &= ~0xFF; // remove tri effect bits
    float width3 = 60 * 3.0f;
    uRoundedRectangle(quadPos, width3, 65.0f, HUEToRGBU32(0.0f), uTriEffect_None);
    
    quadPos.y += 75.0f;
    uRoundedRectangle(quadPos, width3, 65.0f, HUEToRGBU32(0.2f), uFadeBit);
    
    quadPos.y += 100.0f;
    uRoundedRectangle(quadPos, width3, 65.0f, HUEToRGBU32(0.4f), uFadeBit | uFadeInvertBit);
}

bool ShowMenu()
{
    TriangleTest();

    ShowFPS();

    bool pauseMenuOpenned = false;

    if (IsAndroid() && menuState == MenuState_Gameplay) {
        uSetFloat(uf::TextScale, 1.125f);

        Vector2f buttonPos = Vec2(1850.0f, 30.0f);
        if (uButton(nullptr, buttonPos, MakeVec2(40.0f), uButtonOpt_Border))
        {
            menuState = MenuState_PauseMenu;
            pauseMenuOpenned = true;
        }

        // make pause icon ||
        buttonPos += Vec2(10.0f, 7.0f);
        uQuad(buttonPos, Vec2(7.0f, 30.0f), ~0u);
        buttonPos.x += 15.0f;
        uQuad(buttonPos, Vec2(7.0f, 30.0f), ~0u);
    }

    if (showDetails) {
        // write to left bottom side of the screen
        uText("Cratoria: Dubrovnik-Sponza", Vec2(100.0f, 950.0f));
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
        pauseMenuOpenned = true;
    }

    return pauseMenuOpenned;
}