
#pragma once

#include "../../ASTL/Math/Vector.hpp"

// usefull icons. Usage:  DrawText(IC_ALARM ": 12:00pm, " IC_CIRCLE) // c string concatanation
#define IC_LEFT_TRIANGLE  "\xE2\x8F\xB4"
#define IC_RIGHT_TRIANGLE "\xE2\x8F\xB5"
#define IC_UP_TRIANGLE    "\xE2\x8F\xB6"
#define IC_DOWN_TRIANGLE  "\xE2\x8F\xB7"
#define IC_PAUSE          "\xE2\x8F\xB8"
#define IC_SQUARE         "\xE2\x8F\xB9" 
#define IC_CIRCLE         "\xE2\x8F\xBA"
#define IC_RESTART        "\xE2\x86\xBA"
#define IC_HOUR_GLASS     "\xE2\x8F\xB3"
#define IC_ALARM          "\xE2\x8F\xB0"
#define IC_CHECK_MARK     "\xE2\x9C\x94"
#define IC_HEART          "\xE2\x9D\xA4"
#define IC_STAR           "\xE2\x98\x85"


enum {
    InvalidFontHandle = -1,
    uOptNone = 0
};

enum struct uColor : uint
{
    Text          , 
    Quad          , 
    Hovered       , // < button hovered color
    Line          ,
    Border        ,
    CheckboxBG    ,
    TextBoxBG     ,
    SliderInside  ,
    TextBoxCursor ,
    SelectedBorder, // < selected field, slider, textbox, checkbox...
};

// uEmptyInside: if you want an circle fades like center is black the outer area is white set this true
//               otherwise it will work like clock looking fade: counter clockwise fade 0 to 1
enum uTriEffect_ {
    uTriEffect_None = 0,
    uFadeBit        = 1,  // makes Fade effect
    uCutBit         = 2,  // discards rendering pixel if fade value is below CutStart
    uFadeInvertBit  = 4,  // inverts the per vertex fade value
    uEmptyInsideBit = 8,  // whatever the shape is this will set the center fade value to 0
    uIntenseFadeBit = 16, // in fragment shader it will multiply fade value by 2.0
    uCenterFadeBit  = 32  // maps fade value between [0.0f, 1.0f, 0.0f] instead of [0.0f, 1.0f] so center is white other areas are dark(left right)
};

enum uButtonOpt_ { // bitmask
    // all of the uTriEffect_.. is valid
    uButtonOpt_Hovered = 256,
    uButtonOpt_Border  = 512
};

enum struct uf : uint
{
    LineThickness,
    // if set to zero it will start at the end of the text
    // Content Start is: Vsync On _________ [X]  the space between label and content
    ContentStart ,
    ButtonSpace  , // Space between button text and button quad start
    TextScale    , // 1.0 default
    TextBoxWidth ,
    SliderHeight ,
    Depth        , // < between [0.0, 1.0] lower depth will shown on top
    FieldWidth   , // < width of float or int fields
    TextWrapWidth, // < only active if uTextFlags_WrapWidthDetermined flag is active
};

enum uTextFlags_{
    uTextFlags_None      = 0,
    uTextFlags_NoNewLine = 1,
    uTextFlags_WrapWidthDetermined = 2,
};

enum CheckOpt_ { 
    CheckOpt_WhileMouseDown = 1, 
    CheckOpt_BigColission = 2 
};

typedef int uClickOpt;
typedef uint FontHandle;
typedef uint uButtonOptions;
typedef uint uTriEffect;
typedef uint uTextFlags;
typedef uf uFloat;

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
void uSetFloat(uf color, float val);

// 0xFF000000 is black and alpha is 1.0 0x00FF0000 is blue so ABGR when writing with hex
void uPushColor(uColor color, uint val);
void uPushFloat(uFloat what, float val);

void uPopColor(uColor color);
void uPopFloat(uFloat what);

uint uGetColor(uColor color);
float uGetFloat(uFloat what);
bool uIsHovered(); // last button was hovered?

bool ClickCheck(Vector2f pos, Vector2f scale, uClickOpt flags = 0);
bool uClickCheckCircle(Vector2f pos, float radius, uClickOpt flags = 0);

//
float uToolTip(const char* text, float timeRemaining, bool wasHovered);

//------------------------------------------------------------------------

// text is an utf8 string
void uText(const char* text, Vector2f position, uTextFlags flags = 0u);

void uBorder(Vector2f begin, Vector2f scale);

// quad shaped 
void uQuad(Vector2f position, Vector2f scale, uint color, uint properties = 0u);

void uLineVertical(Vector2f begin, float size, uint properties = 0u);

void uLineHorizontal(Vector2f begin, float size, uint properties = 0u);

Vector2f uCalcCharSize(char c);

Vector2f uCalcTextSize(const char* text, uTextFlags flags = 0u);

int CalcTextNumLines(const char* text, uTextFlags flags = 0u);

// returns true if clicked, make scale [0.0,0.0] if you want to scale the button according to text
bool uButton(const char* text, Vector2f pos, Vector2f scale, uButtonOptions opt = 0);

bool uTextBox(const char* label, Vector2f pos, Vector2f size, char* text);

// returns true if changed
// if cubeCheckMark is true, selected checkbox will look like square instead of checkmark
bool uCheckBox(const char* text, Vector2f pos, bool* isEnabled, bool cubeCheckMark = false);

// val should be between 0 and 1
// minimum value that slicer can represent is 0.01f lower than that will round to 0.0f, be aware of that
bool uSlider(const char* label, Vector2f pos, float* val, float scale); 

int uDropdown(const char* label, Vector2f pos, const char** names, int numNames, int current);

int uChoice(const char* label, Vector2f pos, const char** names, int numNames, int current);

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

//------------------------------------------------------------------------
// Triangle Tendering

// we are using uint properties to describe the effects
// first 8 bit: uTriEffect bitmask
// next 8 bit: cutStart, normalized 8bit integer for this effect, think of it like [0, 255], [0.0f, 1.0f]
// next 8 bit: are number of triangles in circle or capsule. define it 0 for automatic
inline uint MakeTriProperty(uTriEffect effect, uint cutStart, uint numSegments)
{
    return effect | (cutStart << 8) | (numSegments << 16);
}

// ads a vertex for triangle drawing
// properties: leave it as zero if you don't want any effects, otherwise use the instructsions above
void uVertex(Vector2f pos, uint8 fade, uint color = ~0u, uint properties = 0u);

void uCircle(Vector2f center, float radius, uint color, uint properties = 0u);

void uCapsule(Vector2f center, float radius, float width, uint color, uint properties = 0u);

void uRoundedRectangle(Vector2f pos, float width, float height, uint color, uint properties = 0u);

void uDrawTriangle(Vector2f pos0, Vector2f pos1, Vector2f pos2, uint color);

// axis is -1 or 1, you may want to scale it as well (ie: 2x)
void uHorizontalTriangle(Vector2f pos, float size, float axis, uint color);

// axis is -1 or 1, you may want to scale it as well (ie: 2x)
void uVerticalTriangle(Vector2f pos, float size, float axis, uint color);

enum uScissorMask_ {
    uScissorMask_Quad = 1, // effects the quads
    uScissorMask_Text = 2, // effects the texts
    uScissorMask_Vertex = 4, // effects the circles, capsules, vertices
};
typedef uint uScissorMask;

// uQuad and uTexts between begin and EndStencil functions, will use scisor (outside of rectangle will not drawn)
void uBeginScissor(Vector2f pos, Vector2f scale, uScissorMask mask);

void uEndScissor(uScissorMask mask);



//------------------------------------------------------------------------
// Window API

void uBeginWindow(const char* name, uint32_t hash, Vector2f position, Vector2f scale);

void uWindowEnd();

void uSeperatorW(uint color, uTriEffect triEffect, float occupancy = 0.85f);

bool uButtonW(const char* text, Vector2f scale, uButtonOptions opt = 0);

// it will look like this: <  option  >
// current is the current index of elements.
// returns new index if value changed.
// usage:
//      const char* graphicsNames[] = { "Low" , "Medium", "High", "Ultra" };
//      static int CurrentGraphics = 0;
//      CurrentGraphics = uChoice("Graphics", pos, graphicsNames, ArraySize(graphicsNames), CurrentGraphics)
bool uTextBoxW(const char* label, Vector2f size, char* text);

// returns true if changed
// if cubeCheckMark is true, selected checkbox will look like square instead of checkmark
bool uCheckBoxW(const char* text, bool* isEnabled, bool cubeCheckMark = false);

// val should be between 0 and 1
// minimum value that slicer can represent is 0.01f lower than that will round to 0.0f, be aware of that
bool uSliderW(const char* label, float* val, float scale); 

FieldRes uIntFieldW(const char* label, int* val, int minVal = 0, int maxVal = INT32_MAX, float dragSpeed = 1.0f);

FieldRes uFloatFieldW(const char* label, float* val, float minVal = 0.0f, float maxVal = 1.0f, float dragSpeed = 0.1f);

bool uIntVecFieldW(const char* label,
                   int* val, 
                   int N, // number of vec elements
                   int* index = nullptr, // holds the current selected element index
                   int minVal = 0, 
                   int maxVal = INT32_MAX, 
                   float dragSpeed = 1.0f);

bool uFloatVecFieldW(const char* label, 
                    float* valArr, 
                    int N, // number of vec elements
                    int* index = nullptr, // holds the current selected element index
                    float minVal = 0,
                    float maxVal = 99999.0f, 
                    float dragSpeed = 1.0f);

bool uColorFieldW(const char* label, uint* color);

bool uColorField3W(const char* label, float* color3Ptr); // rgb32f color

bool uColorField4W(const char* label, float* color4Ptr); // rgba32f color

int uChoiceW(const char* label, const char** elements, int numElements, int current);

// similar to uChoice but when we click it opens dropdown menu and allows us to select
int uDropdownW(const char* label, const char** names, int numNames, int current);



//------------------------------------------------------------------------
// Rendering

void uBegin();

void uRender();

void uDestroy();

void PlayButtonClickSound();

void PlayButtonHoverSound();



