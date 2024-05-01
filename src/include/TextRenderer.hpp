
#pragma once

typedef int FontAtlasHandle;

enum {
    InvalidFontHandle = -1   
};


FontAtlasHandle LoadFontAtlas(const char* file);

// text is an utf8 string
void DrawText(const char* text, float xPos, float yPos, float scale);

void TextRendererInitialize();

void DestroyTextRenderer();

// usefull icons. Usage:  DrawText(IC_ALARM": 12:00pm, "IC_CIRCLE) // c string concatanation
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

