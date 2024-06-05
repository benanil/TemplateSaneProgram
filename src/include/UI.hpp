
#pragma once

#include "../../ASTL/Math/Vector.hpp"

// usefull icons. Usage:  DrawText(IC_ALARM ": 12:00pm, " IC_CIRCLE) // c string concatanation
#define IC_LEFT_TRIANGLE "\xE2\x8F\xB4"
#define IC_RIGHT_TRIANGLE "\xE2\x8F\xB5"
#define IC_UP_TRIANGLE "\xE2\x8F\xB6"
#define IC_DOWN_TRIANGLE "\xE2\x8F\xB7"
#define IC_PAUSE "\xE2\x8F\xB8"
#define IC_SQUARE "\xE2\x8F\xB9" 
#define IC_CIRCLE "\xE2\x8F\xBA"
#define IC_RESTART "\xE2\x86\xBA"
#define IC_HOUR_GLASS "\xE2\x8F\xB3"
#define IC_ALARM "\xE2\x8F\xB0"
#define IC_CHECK_MARK "\xE2\x9C\x94"
#define IC_HEART "\xE2\x9D\xA4"
#define IC_STAR "\xE2\x98\x85"


enum {
    InvalidFontHandle = -1,
    uOptNone = 0
};

enum uColor_{
    uColorText, 
    uColorQuad, 
    uColorHovered, 
    uColorLine, 
    uColorBorder,
    uColorCheckboxBG,
    uColorTextBoxBG,
    uColorSliderInside,
    uColorTextBoxCursor
};

enum uButtonOpt_ { // bitmask
    uButtonOpt_Hovered = 1,
    uButtonOpt_Border  = 2
};

enum uFloat_ {
    ufLineThickness,
    // if set to zero it will start at the end of the text
    // Content Start is: Vsync On _________ [X]  the space between label and content
    ufContentStart,
    ufButtonSpace , 
    ufTextScale   , // 1.0 default
    ufTextBoxWidth,
    ufSliderHeight,
    ufDepth       , // < between [0.0, 1.0] lower depth will shown on top
    ufDragSpeed   , // drag speed of int or float fields default value 1.0
};

typedef int FontHandle;
typedef int uColor;
typedef int uButtonOptions;
typedef int uFloat;
//------------------------------------------------------------------------
// also sets the current font to the loaded font
FontHandle uLoadFont(const char* file);

void uInitialize();

void uWindowResizeCallback(int width, int height);
void uKeyPressCallback(unsigned unicode);

//------------------------------------------------------------------------
void uSetElementFocused(bool val); // next element that will drawn is going to be focused
void uSetFont(FontHandle font);
void uSetColor(uColor what, uint color); // set color of the buttons, texts etc.
void uSetTheme(uint* colors);
void uSetFloat(uFloat color, float val);

void uPushColor(uColor color, uint val);
void uPushFloat(uFloat what, float val);

void uPopColor(uColor color);
void uPopFloat(uFloat what);

uint uGetColor(uColor color);
float uGetFloat(uFloat what);
bool uIsHovered(); // last button was hovered?

//------------------------------------------------------------------------

// text is an utf8 string
void uText(const char* text, Vector2f position);

// returns true if clicked, make scale [0.0,0.0] if you want to scale the button according to text
bool uButton(const char* text, Vector2f pos, Vector2f scale, uButtonOptions opt = 0);

// quad shaped 
void uQuad(Vector2f position, Vector2f scale, uint color);

// returns true if changed
bool uCheckBox(const char* text, bool* isEnabled, Vector2f pos);

// val should be between 0 and 1
bool uSlider(const char* label, Vector2f pos, float* val, float scale); 

bool uIntField(const char* label, Vector2f pos, int* val);

bool uFloatField(const char* label, Vector2f pos, float* val);

// it will look like this: <  option  >
// current is the current index of elements.
// returns new index if value changed.
// usage:
//      const char* graphicsNames[] = { "Low" , "Medium", "High", "Ultra" };
//      static int CurrentGraphics = 0;
//      CurrentGraphics = uChoice("Graphics", pos, graphicsNames, ArraySize(graphicsNames), CurrentGraphics)
int uChoice(const char* label, Vector2f pos, const char** elements, int numElements, int current);

bool uTextBox(const char* label, Vector2f pos, Vector2f size, char* text);

void uLineVertical(Vector2f begin, float size);

void uLineHorizontal(Vector2f begin, float size);

void uBorder(Vector2f begin, Vector2f scale);

//------------------------------------------------------------------------
Vector2f uCalcTextSize(const char* text);

void uBegin();

void uRender();

void uDestroy();
