
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
    uColorText          , 
    uColorQuad          , 
    uColorHovered       , // < button hovered color
    uColorLine          ,
    uColorBorder        ,
    uColorCheckboxBG    ,
    uColorTextBoxBG     ,
    uColorSliderInside  ,
    uColorTextBoxCursor ,
    uColorSelectedBorder, // < selected field, slider, textbox, checkbox...
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
    ufButtonSpace , // Space between button text and button quad start
    ufTextScale   , // 1.0 default
    ufTextBoxWidth,
    ufSliderHeight,
    ufDepth       , // < between [0.0, 1.0] lower depth will shown on top
    ufFieldWidth  , // < width of float or int fields
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

// 0xFF000000 is black and alpha is 1.0 0x00FF0000 is blue so ABGR when writing with hex
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
// if cubeCheckMark is true, selected checkbox will look like square instead of checkmark
bool uCheckBox(const char* text, bool* isEnabled, Vector2f pos, bool cubeCheckMark = false);

// val should be between 0 and 1
bool uSlider(const char* label, Vector2f pos, float* val, float scale); 

enum FieldRes_ { FieldRes_Changed = 1, FieldRes_Clicked = 2 };
typedef int FieldRes;

FieldRes uIntField(const char* label, Vector2f pos, int* val, int minVal = 0, int maxVal = INT32_MAX, float dragSpeed = 1.0f);

FieldRes uFloatField(const char* label, Vector2f pos, float* val, float minVal = 0.0f, float maxVal = 1.0f, float dragSpeed = 0.1f);

bool uIntVecField(const char* label,
                  Vector2f pos, 
                  int* val, 
                  int N, // number of vec elements
                  int* index = nullptr, // holds the current selected element index
                  int minVal = 0, 
                  int maxVal = INT32_MAX, 
                  float dragSpeed = 1.0f);

bool uFloatVecField(const char* label, 
                    Vector2f pos, 
                    float* valArr, 
                    int N, // number of vec elements
                    int* index = nullptr, // holds the current selected element index
                    float minVal = 0,
                    float maxVal = 99999.0f, 
                    float dragSpeed = 1.0f);

bool uColorField(const char* label, Vector2f pos, uint* color);

bool uColorField3(const char* label, Vector2f pos, float* color3Ptr); // rgb32f color

bool uColorField4(const char* label, Vector2f pos, float* color4Ptr); // rgba32f color

// basically draws the texture to the specified position and scale
// can be used for debugging. (easy and useful)
// each uSprite call makes an draw call so
// if you want to make an menu with thumbnails preferably you should resize all of the textures to 64x64 
// and use sprite atlas like in font rendering
// be aware that this works after scene rendering done, whenever you call this function, it will show the content when it is rendered(uRender function)
void uSprite(Vector2f pos, Vector2f scale, struct Texture* texturePtr);

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
// Triangle Tendering
enum uTriEffect_
{
    uTriEffect_Fade = 1, 
    uTriEffect_Cut  = 2
};
typedef int uTriEffect;

void uSetTriangleEffect(uTriEffect effect);
void uSetCutStart(uint8 cutStart);

// ads a vertex for triangle drawing
void uVertex(Vector2f pos, uint8 fade, uint color = 0);

void uTriangle(Vector2f pos0, Vector2f pos1, Vector2f pos2, uint color);

void uTriangleQuad(Vector2f pos, Vector2f scale, uint color);

// axis is -1 or 1, you may want to scale it as well (ie: 2x)
void uHorizontalTriangle(Vector2f pos, float size, float axis, uint color);

// axis is -1 or 1, you may want to scale it as well (ie: 2x)
void uVerticalTriangle(Vector2f pos, float size, float axis, uint color);

// num segments are number of triangles in circle. define it 0 for automatic
void uCircle(Vector2f center, float radius, uint color, int numSegments = 8);

void uCapsule(Vector2f center, float radius, float width, uint color, uint8 numSegments = 8);

//------------------------------------------------------------------------
Vector2f uCalcTextSize(const char* text);

void uBegin();

void uRender();

void uDestroy();

void PlayButtonClickSound();

void PlayButtonHoverSound();
