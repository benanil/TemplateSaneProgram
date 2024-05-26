
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
typedef int FontHandle;

enum uColor_{
  uColorText, uColorQuad, uColorHovered  
};
typedef int uColor;

enum uButtonOptions_ { // bitmask
    uButtonOptions_Hovered = 1
};
typedef int uButtonOptions;


// also sets the current font to the loaded font
FontHandle uLoadFont(const char* file);

void uInitialize();

void uSetFont(FontHandle font);
void uSetDepth(char depth);
void uSetColor(uColor what, uint color); // set color of the buttons, texts etc.
void uSetTheme(uint* colors);

uint uGetColor(uColor color);
bool uIsHovered(); // last button was hovered?

// text is an utf8 string
void uText(const char* text, Vector2f position, float scale);
bool uButton(const char* text, Vector2f pos, Vector2f scale, uButtonOptions opt = 0); // returns true if clicked
void uQuad(Vector2f position, Vector2f scale, uint color);
bool uCheckBox(const char* text, bool* isEnabled, Vector2f pos, float scale);
bool uSlider(Vector2f pos, float* val, float scale); // val should be between 0 and 1

Vector2f uCalcTextSize(const char* text, float scale);

void uRender();

void uWindowResizeCallback(int width, int height);

void uDestroy();
