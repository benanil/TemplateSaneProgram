/*********************************************************************************
*  Purpose: Importing Fonts, Creating font atlases and rendering Text            *
*  Author : Anilcan Gulkaya 2024 anilcangulkaya7@gmail.com                       *
*  Note:                                                                         *
*  if you want icons, font must have Unicode Block 'Miscellaneous Technical'     *
*  I've analysed the european countries languages and alphabets                  *
*  to support most used letters in European languages.                           *
*  English, German, Brazilian, Portuguese, Finnish, Swedish fully supported      *
*  for letters that are not supported in other nations I've used transliteration *
*  to get closest character. for now 12*12 = 144 character supported             *
*  each character is maximum 48x48px                                             *
*********************************************************************************/

#include "include/AssetManager.hpp" // for AX_GAME_BUILD macro

#if AX_GAME_BUILD == 0 // we don't need to create sdf fonts at runtime when playing game
    #define STB_TRUETYPE_IMPLEMENTATION  
    #include "../External/stb_truetype.h"
// #define STB_IMAGE_WRITE_IMPLEMENTATION // you might want to see atlas
// #include "../External/stb_image_write.h"
#endif

#include "../ASTL/String.hpp"
#include "../ASTL/IO.hpp"
#include "../ASTL/Math/Matrix.hpp"
#include "../ASTL/Random.hpp"
#include "../ASTL/HashMap.hpp"

#include "include/UI.hpp"
#include "include/Renderer.hpp"
#include "include/Platform.hpp"

// Atlas Settings
static const int CellCount  = 12;
static const int CellSize   = 48;
static const int AtlasWidth = CellCount * CellSize;
static const int MaxCharacters = 512;
static const int AtlasVersion = 1;

// SDF Settings
static const int SDFPadding = 3;
static const unsigned char OnedgeValue = 128;
static const float PixelDistScale = 18.0f;

struct FontChar
{
    short width, height;
    short xoff, yoff;
    float advence;
};

struct FontAtlas
{
    FontChar characters[CellCount*CellCount];
    unsigned int textureHandle;
    unsigned int cellCount;
    unsigned int charSize;
    int ascent, descent, lineGap;
    float maxCharWidth; // width of @
};

// we will store this in RGBA32u texture, efficient than storing for each vertex
// 16 byte per quad: 4 byte for each vertex, we even have empty padding =)
// x = half2:size,
// y = character: uint8, depth: uint8, scale: half
// z = rgba8 color, w = empty
struct TextData
{
    uint size;
    uint8 character, depth;
    half scale;
    uint color;
    uint padding;
};

struct QuadData
{
    uint size;
    uint color;
    uint8 depth;
    uint8 padding[3];
    uint padding1;
};

namespace
{
    FontAtlas mFontAtlases[4] = {};
    FontAtlas* mCurrentFontAtlas;
    int mNumFontAtlas = 0;

    Shader mFontShader;
    Texture mPosTex;
    Texture mDataTex; // x = half2:size, y = character: uint8, depth: uint8, scale: half  

    Vector2f mTextPositions[MaxCharacters];
    TextData mTextData[MaxCharacters];

    Vector2f mWindowRatio; // ratio against 1920x1080, [1.0, 1.0] if 1080p 0.5 if half of it, 2.0 if two times bigger
    float mUIScale; // min(mWindowRatio.x, mWindowRatio.y)

    Vector2f mMouseOld;
    bool mWasHovered = false; // last button was hovered?

    int mNumChars = 0; // textLen without spaces, or undefined characters    
    bool mInitialized = false;
    bool mElementFocused[8] = {};
    int mElementFocusedIndex = 0;
    // uniform locations
    int posTexLoc, dataTexLoc, atlasLoc, uScrSizeLoc;

    //------------------------------------------------------------------------
    // Quad Batch renderer
    const int MaxQuads = 512;

    Shader mQuadShader;

    Vector2f mQuadPositions[MaxQuads];
    QuadData mQuadData[MaxQuads];

    Texture mQuadPosTex;
    Texture mQuadDataTex;

    int mQuadIndex = 0;
    // uniform locations
    int posTexLocQuad, dataTexLocQuad, uScrSizeLocQuad, uScaleLocQuad;

    //------------------------------------------------------------------------
    // TextBox
    struct CurrentText
    {
        char* str;  // utf8 str
        int Len;    // length in bytes
        int Pos;
        int MaxLen; // max utf8 characters that can fit into text field
        bool Editing;
    } mCurrText = {};
    
    bool mAnyTextEdited = false;
    uint mLastStrHash;

    bool mLastFloatEditing = false;
    bool mDotPressed = false;
    int mFloatDigits = 3;
    float* mEditingFloat;
        
    //------------------------------------------------------------------------
    // configuration
    uint mColors[] = { 
        0xFFE1E1E1u, // uColorText
        0x8C000000u, // uColorQuad
        0x8CFFFFFFu, // uColorHover
        0xFFDEDEDEu, // uColorLine
        0xFF484848u, // uColorBorder
        0xFF0B0B0Bu, // uColorCheckboxBG
        0xFF0B0B0Bu, // UColorTextBoxBG
        0xCF888888u, // uColorSliderInside 
        0xFFFFFFFFu, // uColorTextBoxCursor
        0xFF008CFAu  // uColorSelectedBorder
    };
    constexpr int NumColors = sizeof(mColors) / sizeof(uint);
    constexpr int StackSize = 6;
    // stack for pushed colors
    uint mColorStack[NumColors][StackSize];
    int mColorStackCnt[NumColors] = { };

    float mFloats[] = {
        1.82f  , // line thickness  
        160.0f, // ufContentStart
        18.0f , // ButtonSpace
        1.0f  , // TextScale,
        175.0f, // TextBoxWidth
        18.0f , // Slider Width
        0.9f  , // Depth 
        98.0f   // Field Width
    };
    constexpr int NumFloats = sizeof(mFloats) / sizeof(float);
    // stack for pushed floats
    float mFloatStack[NumFloats][StackSize];
    int mFloatStackCnt[NumFloats] = { };
    
    HashMap<void*, int> mIndexMap = {}; // given Field Pointer, returns Index  
}

void uWindowResizeCallback(int width, int height)
{
    mWindowRatio = { width / 1920.0f, height / 1080.0f };
    mUIScale = MIN(mWindowRatio.x, mWindowRatio.y);
}

void uInitialize()
{
    ScopedText fontVert = ReadAllText("Shaders/TextVert.glsl", nullptr, nullptr, AX_SHADER_VERSION_PRECISION());
    ScopedText fontFrag = ReadAllText("Shaders/TextFrag.glsl", nullptr, nullptr, AX_SHADER_VERSION_PRECISION());
    mFontShader = rCreateShader(fontVert.text, fontFrag.text);
    
    // per character textures
    mPosTex  = rCreateTexture(MaxCharacters, 1, nullptr, TextureType_RG32F , TexFlags_RawData);
    // we will store mTextData array in this texture
    mDataTex = rCreateTexture(MaxCharacters, 1, nullptr, TextureType_RGBA32UI, TexFlags_RawData);
    
    rBindShader(mFontShader);
    posTexLoc     = rGetUniformLocation("posTex");
    dataTexLoc    = rGetUniformLocation("dataTex");
    atlasLoc      = rGetUniformLocation("atlas");
    uScrSizeLoc   = rGetUniformLocation("uScrSize");
    mInitialized  = true;
    mCurrentFontAtlas = nullptr;
    
    ScopedText quadVert = ReadAllText("Shaders/QuadBatch.glsl", nullptr, nullptr, AX_SHADER_VERSION_PRECISION());
    ScopedText quadFrag = ReadAllText("Shaders/QuadFrag.glsl", nullptr, nullptr, AX_SHADER_VERSION_PRECISION());
    mQuadShader = rCreateShader(quadVert.text, quadFrag.text);
    
    // per character textures
    mQuadPosTex  = rCreateTexture(MaxQuads, 1, nullptr, TextureType_RG32F , TexFlags_RawData);
    mQuadDataTex = rCreateTexture(MaxQuads, 1, nullptr, TextureType_RGBA32UI, TexFlags_RawData);
    
    rBindShader(mQuadShader);
    posTexLocQuad   = rGetUniformLocation("posTex");
    dataTexLocQuad  = rGetUniformLocation("dataTex");
    uScrSizeLocQuad = rGetUniformLocation("uScrSize");
    uScaleLocQuad   = rGetUniformLocation("uScale");
    
    GetMouseWindowPos(&mMouseOld.x, &mMouseOld.y);
}

static void WriteGlyphToAtlass(int i, 
                               FontChar* character,
                               unsigned char (&atlas)[AtlasWidth][AtlasWidth],
                               unsigned char* sdf)
{
    const int atlasWidth = AtlasWidth;
    int xStart = (i * CellSize) % atlasWidth;
    int yStart = (i / CellCount) * CellSize;
    
    for (int yi = yStart, y = 0; yi < yStart + character->height; yi++, y++)
    {
        for (int xi = xStart, x = 0; xi < xStart + character->width; xi++, x++)
        {
            atlas[yi][xi] = sdf[(y * character->width) + x];
        }
    }
}

static void SaveFontAtlasBin(char* path, int pathLen,
                             FontAtlas* atlas,
                             unsigned char (&image)[AtlasWidth][AtlasWidth])
{
    // .bft = Binary Font Type
    ChangeExtension(path, pathLen, "bft");
    AFile file = AFileOpen(path, AOpenFlag_Write);
    AFileWrite(&AtlasVersion, sizeof(int), file);
    AFileWrite(&atlas->cellCount, sizeof(int), file);
    AFileWrite(&atlas->charSize, sizeof(int), file);
    AFileWrite(&atlas->ascent, sizeof(int), file);
    AFileWrite(&atlas->descent, sizeof(int), file);
    AFileWrite(&atlas->lineGap, sizeof(int), file);
    AFileWrite(&atlas->characters, CellCount * CellCount * sizeof(FontChar), file);
    AFileWrite(image, AtlasWidth * AtlasWidth, file);
    AFileClose(file);
}

static void LoadFontAtlasBin(const char* path,
                             FontAtlas* atlas,
                             unsigned char (&image)[AtlasWidth][AtlasWidth])
{
    int version;
    AFile file = AFileOpen(path, AOpenFlag_Read);
    AFileRead(&version, sizeof(int), file);
    AFileRead(&atlas->cellCount, sizeof(int), file);
    AFileRead(&atlas->charSize, sizeof(int), file);
    AFileRead(&atlas->ascent, sizeof(int), file);
    AFileRead(&atlas->descent, sizeof(int), file);
    AFileRead(&atlas->lineGap, sizeof(int), file);
    AFileRead(&atlas->characters, CellCount * CellCount * sizeof(FontChar), file);
    AFileRead(image, AtlasWidth * AtlasWidth, file);
    AFileClose(file);
}

static bool BFTLastVersion(const char* path)
{
    int version;
    AFile file = AFileOpen(path, AOpenFlag_Read);
    AFileRead(&version, sizeof(int), file);
    AFileClose(file);
    return version == AtlasVersion;
}

FontHandle uLoadFont(const char* file)
{
    ASSERTR(mNumFontAtlas < 4, return 0); // max 4 atlas for now
    // 12 is sqrt(128)
    unsigned char image[AtlasWidth][AtlasWidth] = {0};

    FontAtlas* currentAtlas;
    // check file is exist
    char path[256] = {};
    int pathLen = StringLength(file);
    {
        SmallMemCpy(path, file, pathLen);
        ChangeExtension(path, pathLen, "bft");
        if (FileExist(path) && BFTLastVersion(path))
        {
            currentAtlas = &mFontAtlases[mNumFontAtlas++];
            LoadFontAtlasBin(path, currentAtlas, image);
            currentAtlas->textureHandle = rCreateTexture(AtlasWidth, AtlasWidth, image, TextureType_R8, TexFlags_Linear).handle;
            mCurrentFontAtlas = currentAtlas;
            currentAtlas->maxCharWidth  = uCalcTextSize("a").x;
            return mNumFontAtlas - 1;
        }
    }
#if AX_GAME_BUILD == 0
    // .otf or .ttf there are two types of font file
    bool wasOTF = FileHasExtension(file, pathLen, ".otf"); 
    ChangeExtension(path, pathLen, wasOTF ? "otf" : "ttf");

    ScopedPtr<unsigned char> data = (unsigned char*)ReadAllFile(file);
    if (data.ptr == nullptr) {
        return InvalidFontHandle;
    }
    
    stbtt_fontinfo info = {};
    int res = stbtt_InitFont(&info, data.ptr, 0);
    ASSERT(res != 0);
    
    currentAtlas = &mFontAtlases[mNumFontAtlas++];
    currentAtlas->cellCount = CellCount;
    currentAtlas->charSize  = CellSize;
    stbtt_GetFontVMetrics(&info, &currentAtlas->ascent, &currentAtlas->descent, &currentAtlas->lineGap);

    // These functions compute a discretized SDF field for a single character, suitable for storing
    // in a single-channel texture, sampling with bilinear filtering, and testing against
    float scale = stbtt_ScaleForPixelHeight(&info, (float)CellSize);
    rUnpackAlignment(1);
    
    const auto addUnicodeGlyphFn = [&](int unicode, int i)
    {
        FontChar& character = currentAtlas->characters[i];
        int glyph = stbtt_FindGlyphIndex(&info, unicode);
        int xoff, yoff, width, height;
        uint8* sdf = stbtt_GetGlyphSDF(&info, scale, 
                                       glyph,
                                       SDFPadding, OnedgeValue, PixelDistScale,
                                       &width, &height,
                                       &xoff, &yoff);
        ASSERT(sdf != nullptr);
        character.xoff = (short)xoff, character.width  = (short)width;
        character.yoff = (short)yoff, character.height = (short)height;

        int advance, leftSideBearing;
        stbtt_GetGlyphHMetrics(&info, glyph, &advance, &leftSideBearing);
        character.advence = advance * scale;
        WriteGlyphToAtlass(i, &character, image, sdf);
        free(sdf); 
    };

    for (int i = '!'; i <= '~'; i++) // ascii characters
    {
        addUnicodeGlyphFn(i, i);
    }

    // unicode data taken from here: https://www.compart.com/en/unicode/
    // Turkish and European characters.
    const int europeanChars[] = {
        /* ü */0x0FC, /* ö */ 0x0F6, /* ç */ 0x0E7, /* ğ */ 0x11F,  /* ş */ 0x15F, 
        /* ı */0x131, /* ä */ 0x0E4, /* ß */ 0x0DF, /* ñ */ 0x0F1,  /* å */ 0x0E5, 
        /* â */0x0E2, /* á */ 0x0E1, /* æ */ 0x0E6, /* ê */ 0x0EA,  /* ł */ 0x142, 
        /* ć */0x107, /* ø */ 0x00F8,
        // Upper. 
        /* Ü */0x00DC, /* Ö */0x00D6, /* Ç */0x00C7, /* Ğ */0x011E, /* Ş */0x015E, 
        /* Ä */0x00C4, /* ẞ */0x1E9E, /* Ñ */0x00D1, /* Å */0x00C5, /* Â */0x00C2, 
        /* Á */0x00C1, /* Æ */0x00C6, /* Ê */0x00CA, /* Ł */0x0141, /* Ć */0x0106, 0x00D8 /* Ø */
    };

    constexpr int europeanGlyphCount = ArraySize(europeanChars);
    static_assert(europeanGlyphCount <= 33);

    for (int i = 0; i < europeanGlyphCount; i++)
    {
        addUnicodeGlyphFn(europeanChars[i], i);
    }
    
    const int aditionalCharacters[] = { 
        0x23F3, // hourglass flowing sand
        0x23F4, 0x23F5, 0x23F6, 0x23F7, // <, >, ^, v,
        0x23F8, 0X23F9, 0X23FA,         // ||, square, O 
        0x21BA, 0x23F0, //  ↺ anticlockwise arrow, alarm
        0x2605, 0x2764, 0x2714, 0x0130 // Star, hearth, speaker, İ
    };

    constexpr int aditionalCharCount = ArraySize(aditionalCharacters);
    // we can add 17 more characters for filling 144 char restriction
    static_assert(aditionalCharCount + 127 < 144); // if you want more you have to make bigger atlas than 12x12

    for (int i = 127; i < 127 + aditionalCharCount; i++)
    {
        addUnicodeGlyphFn(aditionalCharacters[i-127], i);
    }

    // stbi_write_png("atlas.png", AtlasWidth, AtlasWidth, 1, image, AtlasWidth);
    SaveFontAtlasBin(path, pathLen, currentAtlas, image);
    mCurrentFontAtlas = currentAtlas;
    currentAtlas->textureHandle = rCreateTexture(AtlasWidth, AtlasWidth, image, TextureType_R8, TexFlags_Linear).handle;
    currentAtlas->maxCharWidth  = uCalcTextSize("a").x;
#else
    AX_UNREACHABLE();
#endif
    return mNumFontAtlas-1;
}

struct UTF8Table
{
    uint8_t map[256] = { 0 };
    constexpr UTF8Table()
    {
        for (int c = 000; c < 128; c++)  map[c] = c;
        for (int c = 128; c < 256; c++)  map[c] = '-';
        map[0xFC] = 0;  // ü
        map[0xF6] = 1;  // ö
        map[0xE7] = 2;  // ç
        map[0xE4] = 6;  // ä
        map[0xDF] = 7;  // ß
        map[0xF1] = 8;  // ñ
        map[0xE5] = 9;  // å
        map[0xE2] = 10; // â
        map[0xE1] = 11; // á
        map[0xE6] = 12; // æ
        map[0xEA] = 13; // ê
        map[0xF8] = 16; // ø
        map[0xDC] = 17; // Ü
        map[0xD6] = 18; // Ö
        map[0xC7] = 19; // Ç
        map[0xD1] = 24; // Ñ
        map[0xC5] = 25; // Å
        map[0xC2] = 26; // Â
        map[0xC1] = 27; // Á
        map[0xC6] = 28; // Æ
        map[0xCA] = 29; // Ê
        map[0xC4] = 22; // Ä
        map[0xD8] = 32; // Ø
        // transliterate, use another char for inexistent characters
        // example: à, ă, ą -> a or ț, ț -> t
        map[0xF2] = map[0xF3] = map[0xF4] = 'o';
        map[0xEE] = map[0xCC] = map[0xCD] = 'i';
        map[0xE9] = map[0xE8] = 'e';
        map[0xE0] = 'a';

        map[240] = 3; // ğ 
        map[254] = 4; // ş
        map[253] = 5; // ı
    }
};

// https://en.wikipedia.org/wiki/Slovak_orthography
inline unsigned int UnicodeToAtlasIndex(unsigned int unicode)
{
    if (unicode < 256)
    {
        static constexpr UTF8Table table{};
        return table.map[unicode];
    }
    
    switch (unicode) {
        case 0x011Fu: return 3;  // ğ
        case 0x015Fu: return 4;  // ş
        case 0x0131u: return 5;  // ı
        case 0x0142u: return 14; // ł 
        case 0x0107u: return 15; // ć 
        case 0x011Eu: return 20; // Ğ 
        case 0x015Eu: return 21; // Ş 
        case 0x1E9Eu: return 23; // ẞ 
        case 0x0141u: return 30; // Ł 
        case 0x0106u: return 31; // Ć 
        case 0x21BAu: return 135; // ↺ anticlockwise arrow
        case 0x23F0u: return 136; // alarm
        case 0x2605u: return 137; // Star
        case 0x2764u: return 138; // hearth
        case 0x2714u: return 139; // checkmark
        case 0x0130u: return 140; // İ
    };

    // subsequent icons, 0x23F3u -> 0x23FAu
    // hourglass sand, <, >, ^, v, ||, square, O
    if (unicode >= 0x23F3u && unicode <= 0x23FAu) 
    {
        return unicode - 0x23F3u + 127;
    }

    // transliterate, optional
    switch (unicode) {
        case 0x017Au: case 0x017Bu: case 0x017Cu: case 0x017Eu:  return 'z'; // ź, ž, ż
        case 0x0103u: case 0x0105u: return 'a'; // à, ă, ą
        case 0x0143u: case 0x0144u: case 0x01f9u: return 'n'; // ń
        case 0x0119u: return 'e'; // ę
        case 0x0163u: case 0x021Bu: case 0x1E6Bu: return 't'; // ț, ț
    }; 

    return '-';
}

static inline Vector2i GetWindowSize()
{
    Vector2i windowSize;
    wGetWindowSize(&windowSize.x, &windowSize.y);
    return windowSize;
}

void uSetElementFocused(bool val) {
    ASSERTR(mElementFocusedIndex < 8, return);
    mElementFocused[mElementFocusedIndex++] = val;
}

static bool uGetElementFocused() {
    if (mElementFocusedIndex < 1) return false;
    return mElementFocused[--mElementFocusedIndex];
}

void uSetFont(FontHandle font) {
    mCurrentFontAtlas = &mFontAtlases[font];
}

void uSetTheme(uColor what, uint color) {
    mColors[what] = color;
}

void uSetFloat(uFloat what, float val) {
    mFloats[what] = val;
}

float uGetFloat(uFloat what) {
    int stackCnt = mFloatStackCnt[what];
    if (stackCnt > 0) // if user used PushFloat use the one from stack
        return mFloatStack[what][stackCnt-1];
    return mFloats[what];
}

uint uGetColor(uColor color) {
    int stackCnt = mColorStackCnt[color];
    if (stackCnt > 0) // if user used PushColor use the one from stack
        return mColorStack[color][stackCnt-1];
    return mColors[color];
}

void uSetTheme(uint* colors) {
    SmallMemCpy(mColors, colors, sizeof(colors));
}

void uPushColor(uColor color, uint val) {
    int& index = mColorStackCnt[color];
    ASSERT(index < StackSize);
    mColorStack[color][index++] = val;
}

void uPushFloat(uFloat what, float val) {
    int& index = mFloatStackCnt[what];
    ASSERT(index < StackSize);
    mFloatStack[what][index++] = val;    
}

void uPopColor(uColor color) {
    int& index = mColorStackCnt[color];
    if (index <= 0) return;
    index--;
}

void uPopFloat(uFloat what) {
    int& index = mFloatStackCnt[what];
    if (index <= 0) return;
    index--;
}

bool uIsHovered() {
    return mWasHovered;
}
// reference resolution is always 1920x1080,
// we will remap input position and scale according to current window size

// todo: allow different scales 
void uText(const char* text, Vector2f position)
{
    ASSERTR(mInitialized == true, return);
    ASSERTR(text != nullptr, return);
    ASSERTR(mCurrentFontAtlas != nullptr, return); // you have to initialize at least one font
    
    int txtLen = 0, txtSize = 0;
    {
        const char* s = text;
        while (*s) 
        {
            if ((*s & 0xC0) != 0x80) txtLen++;
            txtSize++;
            s++;
        }
    }
    ASSERTR(txtLen < MaxCharacters, return);

    position *= mWindowRatio;
    float spaceWidth = (float)mCurrentFontAtlas->characters['0'].width;
    float scale = uGetFloat(ufTextScale);
    scale *= mUIScale;
    half scalef16 = ConvertFloatToHalf(scale);
    uint color = uGetColor(uColorText);

    const char* textEnd = text + txtSize;
    uint8 currentDepth = int(uGetFloat(ufDepth) * 255.0f) ;
    // fill per quad data
    while (text < textEnd)
    {
        if (*text == ' ') {
            position.x += spaceWidth * scale * 0.5f * mWindowRatio.x;
            text++;
            continue;
        }

        unsigned int chr;
        unsigned int unicode;
        text += CodepointFromUtf8(&unicode, text, textEnd);
        chr = UnicodeToAtlasIndex(unicode);

        const FontChar& character = mCurrentFontAtlas->characters[chr];
        // pack per quad data:
        // x = half2:size, y = character: uint8, depth: uint8, scale: half  
        Vector2f size;
        size.x = float(character.width) * scale;
        size.y = float(character.height) * scale;

        mTextData[mNumChars].size  = uint32_t(size.x + 1.0f);
        mTextData[mNumChars].size |= uint32_t(size.y + 1.0f) << 16;

        mTextData[mNumChars].character = chr;  
        mTextData[mNumChars].depth = currentDepth;
        mTextData[mNumChars].scale = scalef16;
        mTextData[mNumChars].color = color;

        Vector2f pos = position;
        pos.x += float(character.xoff) * scale;
        pos.y += float(character.yoff) * scale;
        mTextPositions[mNumChars] = pos;

        position.x += character.advence * scale;
        mNumChars++;
    }
}

Vector2f uCalcTextSize(const char* text)
{
    if (!text) return { 0.0f, 0.0f };
    
    float scale = uGetFloat(ufTextScale);
    float spaceWidth = (float)mCurrentFontAtlas->characters['0'].width;
    const char* textEnd = text + StringLength(text);

    Vector2f size = {0.0f, 0.0f};
    short lastXoff, lastWidth;
    unsigned int chr;

    while (*text)
    {
        if (*text == ' ') {
            size.x += spaceWidth * scale * 0.5f * mWindowRatio.x;
            text++;
            continue;
        }

        unsigned int unicode;
        text += CodepointFromUtf8(&unicode, text, textEnd);
        chr = UnicodeToAtlasIndex(unicode);
        
        const FontChar& character = mCurrentFontAtlas->characters[chr];
        lastXoff  = character.xoff;
        lastWidth = character.width;
        size.x += character.advence * scale;
        size.y = MAX(size.y, character.height * scale);
    }
    // size.x += (lastWidth + lastXoff) * scale;
    return size;
}


//------------------------------------------------------------------------
// Quad Drawing
void uQuad(Vector2f position, Vector2f scale, uint color)
{
    ASSERTR(mQuadIndex < MaxQuads, return);
    if (mQuadIndex >= MaxQuads) return;

    mQuadPositions[mQuadIndex] = position * mWindowRatio;

    mQuadData[mQuadIndex].size  = uint32_t(scale.x + 1.0f);
    mQuadData[mQuadIndex].size |= uint32_t(scale.y + 1.0f) << 16;
    
    uint8 currentDepth = int(uGetFloat(ufDepth) * 255.0f);
    mQuadData[mQuadIndex].color = color;
    mQuadData[mQuadIndex].depth = currentDepth;
    mQuadIndex++;
}

enum CheckOpt_ { CheckOpt_WhileMouseDown = 1, CheckOpt_BigColission = 2 };
typedef int CheckOpt;

static bool ClickCheck(Vector2f pos, Vector2f scale, CheckOpt flags = 0)
{
    Vector2f mousePos;
    GetMouseWindowPos(&mousePos.x, &mousePos.y);
    
    // slightly bigger colission area for easier touching
    if (flags == CheckOpt_BigColission) 
    {
        float slightScaling = MIN(scale.x, scale.y) * 0.5f;
        pos   -= slightScaling;
        scale += slightScaling * 2.0f;
    }

    Vector2f scaledPos = pos * mWindowRatio;
    Vector2f scaledScale = scale * mWindowRatio;
    mWasHovered = PointBoxIntersection(scaledPos, scaledPos + scaledScale, mousePos);
    
    if (!!(flags & CheckOpt_WhileMouseDown) && 
        GetMouseDown(MouseButton_Left)) 
        return mWasHovered;

    bool released = GetMouseReleased(MouseButton_Left);
    return mWasHovered && released;
}

//------------------------------------------------------------------------
bool uButton(const char* text, Vector2f pos, Vector2f scale, uButtonOptions opt)
{
    if (scale.x + scale.y < Epsilon) {
        float buttonSpace = uGetFloat(ufButtonSpace);
        pos.x -= buttonSpace * 2.0f;
        pos.y += buttonSpace;
        scale = uCalcTextSize(text) + buttonSpace;
        scale.x += buttonSpace;
    }
    bool elementFocused = uGetElementFocused();
    bool entered = elementFocused && GetKeyPressed(Key_ENTER);
    bool pressed = entered || ClickCheck(pos, scale);

    uint quadColor = uGetColor(uColorQuad);
    if (mWasHovered || !!(opt & uButtonOpt_Hovered))
        quadColor = mColors[uColorHovered];

    uQuad(pos, scale, quadColor);
    if (!!(opt & uButtonOpt_Border)) {
        uBorder(pos, scale);
    }

    if (!text) return pressed;
    Vector2f textSize = uCalcTextSize(text);
    // align text to the center of the button
    Vector2f padding = (scale - textSize) * 0.5f;
    pos.y += textSize.y;
    pos += padding;
    uText(text, pos);
    return pressed;
}

bool uCheckBox(const char* text, bool* isEnabled, Vector2f pos, bool cubeCheckMark)
{
    Vector2f textSize = uCalcTextSize(text);
    uText(text, pos);
    float checkboxHeight = uCalcTextSize("a").y;
    // detect box position
    float checkboxStart = uGetFloat(ufContentStart);
    if (checkboxStart < 0.01f) {
        const float boxPadding = 20.0f;
        pos.x += textSize.x + boxPadding;
    }
    else {
        pos.x += checkboxStart - checkboxHeight;
    }

    textSize.y = checkboxHeight; // we want same box size for all checkboxes
    pos.y -= textSize.y - 4.0f;
    textSize *= mWindowRatio;

    Vector2f boxScale = { textSize.y * 0.85f, textSize.y * 0.85f};
    uQuad(pos, boxScale, uGetColor(uColorCheckboxBG));

    bool elementFocused = uGetElementFocused();
    uint borderColor = uGetColor(elementFocused ? uColorSelectedBorder : uColorBorder);
    uPushColor(uColorBorder, borderColor);
        uBorder(pos, boxScale);
    uPopColor(uColorBorder);
    
    bool enabled = *isEnabled;
    bool entered = elementFocused && GetKeyPressed(Key_ENTER);
    if (entered || ClickCheck(pos, boxScale, CheckOpt_BigColission)) {
        enabled = !enabled;
    }

    if (enabled && !cubeCheckMark) {
        float scale = uGetFloat(ufTextScale);
        pos.y += textSize.y - 4.0f;
        uPushFloat(ufTextScale, scale * 0.85f);
        uText(IC_CHECK_MARK, pos); // todo properly scale the checkmark
        uPopFloat(ufTextScale);
    }
    else if (enabled && cubeCheckMark)
    {
        Vector2f slightScale = boxScale * 0.13f;
        uint color = uGetColor(uColorSliderInside);
        float lineThickness = uGetFloat(ufLineThickness);
        uQuad(pos + slightScale + lineThickness, boxScale - (slightScale * 2.0f)-lineThickness, color);
    }
    
    bool changed = enabled != *isEnabled;
    *isEnabled = enabled;
    return changed;
}

struct LineThicknesBorderColor { float thickness; uint color; };

inline LineThicknesBorderColor GetLineData()
{
    return { uGetFloat(ufLineThickness), uGetColor(uColorLine) };
}

void uLineVertical(Vector2f begin, float size) {
    LineThicknesBorderColor data = GetLineData();
    uQuad(begin, MakeVec2(data.thickness, size), data.color);
}

void uLineHorizontal(Vector2f begin, float size) {
    LineThicknesBorderColor data = GetLineData();
    uQuad(begin, MakeVec2(size, data.thickness), data.color);
}

void uBorder(Vector2f begin, Vector2f scale)
{
    LineThicknesBorderColor data = { uGetFloat(ufLineThickness), uGetColor(uColorBorder) };
    uQuad(begin, MakeVec2(scale.x, data.thickness), data.color);
    uQuad(begin, MakeVec2(data.thickness, scale.y), data.color);
    
    begin.y += scale.y;
    uQuad(begin, MakeVec2(scale.x + data.thickness, data.thickness), data.color);
    
    begin.y -= scale.y;
    begin.x += scale.x;
    uQuad(begin, MakeVec2(data.thickness, scale.y), data.color);
}

inline char* utf8_prev_char(const char* start, char *s) {
    if (s == start) return s; // If already at the start, no previous character
    s--; // Move back at least one byte

    if (s > start && (*s & 0xC0) == 0x80) s--;
    if (s > start && (*s & 0xC0) == 0x80) s--;
    if (s > start && (*s & 0xC0) == 0x80) s--;
    if (s > start && (*s & 0xC0) == 0x80) s--;
    return s;
}

void uKeyPressCallback(unsigned unicode)
{
    bool isEnter = unicode == 13;
    bool isEscape = unicode == 27;

    if (unicode == '.') {
        mDotPressed = true;
        //mFloatDigits = 1;
    }

    if (!mCurrText.Editing || isEnter || GetKeyDown(Key_CONTROL) || isEscape) return;

    bool hasSpace = mCurrText.Pos < mCurrText.MaxLen;
    bool isBackspace = unicode == 8;

    if (!isBackspace && hasSpace) 
    {
        mCurrText.Pos += CodepointToUtf8(mCurrText.str + mCurrText.Pos, unicode);
    }
    else if (isBackspace && mCurrText.Pos > 0)
    {
        char* end = mCurrText.str + mCurrText.Pos;
        char* newEnd = utf8_prev_char(mCurrText.str, end);
        int removeLen = (int)(size_t)(end - newEnd);
        MemsetZero(newEnd, removeLen);
        mCurrText.Pos -= removeLen;
    }
}

inline Vector2f Label(const char* label, Vector2f pos) {
    Vector2f labelSize;
    if (label != nullptr) {
        labelSize = uCalcTextSize(label);
        uText(label, pos);
    } else {
        labelSize.y = uCalcTextSize("A").y;
        labelSize.x = labelSize.y;
    }
    return labelSize;
}

// maybe feature: TextBox cursor horizontal vertical movement (arrow keys)
// maybe feature: TextBox control shift fast move
// maybe feature: TextBox Shift Arrow select. (paste works copy doesnt)
// feature:       TextBox multiline text
bool uTextBox(const char* label, Vector2f pos, Vector2f size, char* text)
{
    Vector2f labelSize = Label(label, pos);

    if (size.x + size.y < Epsilon) // if size is not determined generate it
    {
        size.x = uGetFloat(ufTextBoxWidth);
        size.y = labelSize.y * 0.85f;
    }
    float contentStart = uGetFloat(ufContentStart);
    pos.x += contentStart - size.x;
    pos.y -= size.y;

    bool clicked = ClickCheck(pos, size);
    uQuad(pos, size, uGetColor(uColorTextBoxBG));

    bool elementFocused = uGetElementFocused();
    uint borderColor = uGetColor(elementFocused ? uColorSelectedBorder : uColorBorder);
    uPushColor(uColorBorder, borderColor);
        uBorder(pos, size);
    uPopColor(uColorBorder);

    // todo: add cursor movement
    // set text position
    float textScale = uGetFloat(ufTextScale);
    float offset = size.y * 0.1f;
    pos.y += size.y - offset;
    pos.x += offset;

    uPushFloat(ufTextScale, textScale * 0.7f);
    if (elementFocused)
    {
        // max number of characters that we can write
        const int TextCapacity = 128; // todo: make adjustable
        float maxCharWidth = mCurrentFontAtlas->maxCharWidth * 0.7f;
        mCurrText.MaxLen = MIN(TextCapacity, int(size.x * 1.3f / maxCharWidth));
        uint32_t hash = MurmurHash32(text, mCurrText.Len, 643364);
        if (hash != mLastStrHash) { // edited text is changed
            mCurrText.Pos = mCurrText.Len;
        }

        if (!IsAndroid() && GetKeyDown(Key_CONTROL) && GetKeyPressed('V'))
        {
            const char* copyText = wGetClipboardString();
            int copyLen = StringLength(copyText);
            if (copyLen < mCurrText.MaxLen && copyText != nullptr)
            {
                MemsetZero(mCurrText.str, mCurrText.Len);
                SmallMemCpy(mCurrText.str, copyText, copyLen);
                mCurrText.Pos = copyLen;
            }
        }

        if (!IsAndroid() && GetKeyDown(Key_CONTROL)  && GetKeyPressed('Q'))
        {
            mCurrText.str[mCurrText.Pos++] = '@';
        }

        mAnyTextEdited = true;
        mCurrText.Editing = true;
        mCurrText.str = text;
        mCurrText.Len = StringLength(text);
        mLastStrHash = hash;

        // draw textbox cursor
        double timeSinceStart = TimeSinceStartup();
        if (timeSinceStart - (float)int(timeSinceStart) < 0.5)
        {
            Vector2f textSize = uCalcTextSize(text);
            Vector2f cursorPos = pos;
            cursorPos.x += textSize.x;
            cursorPos.y -= textSize.y;

            uint cursorColor = uGetColor(uColorTextBoxCursor);
            uPushColor(uColorLine, cursorColor);
            uLineVertical(cursorPos, textSize.y);
            uPopColor(uColorLine);
        }
    }

    uText(text, pos);
    
    uPopFloat(ufTextScale);
    return clicked;
}

bool uSlider(const char* label, Vector2f pos, float* val, float scale)
{
    Vector2f labelSize = Label(label, pos);
    Vector2f size = { scale, uGetFloat(ufSliderHeight) };
    
    float contentStart = uGetFloat(ufContentStart);
    pos.x += contentStart - size.x;
    pos.y -= size.y;

    bool elementFocused = uGetElementFocused();
    uint borderColor = uGetColor(elementFocused ? uColorSelectedBorder : uColorBorder);
    uPushColor(uColorBorder, borderColor);
        uBorder(pos, size);
    uPopColor(uColorBorder);

    bool edited = ClickCheck(pos, size, CheckOpt_WhileMouseDown);
    if (edited && elementFocused) {
        Vector2f mousePos; GetMouseWindowPos(&mousePos.x, &mousePos.y);
        mousePos -= pos;
        *val = Remap(mousePos.x, 0.0f, size.x, 0.0f, 1.0f);
    }

    if (elementFocused && GetKeyReleased(Key_LEFT))  
        *val -= 0.1f,  edited = true;
    if (elementFocused && GetKeyReleased(Key_RIGHT)) 
        *val += 0.1f,  edited = true;

    *val = Clamp(*val, 0.0f, 1.0f);
    
    if (*val > 0.001f)
    {
        float lineThickness = uGetFloat(ufLineThickness);
        size.x *= *val;
        pos    += lineThickness;
        size   -= lineThickness;
        uQuad(pos, size, uGetColor(uColorSliderInside));
        
        uPushFloat(ufLineThickness, 3.0f);
        pos.x += size.x;
        uLineVertical(pos, size.y);
        uPopFloat(ufLineThickness);
    }
    return edited;
}

int uChoice(const char* label, Vector2f pos, const char** names, int numNames, int current)
{   
    Vector2f labelSize = Label(label, pos);
    Vector2f size = { uGetFloat(ufTextBoxWidth), uGetFloat(ufSliderHeight) };

    float contentStart = uGetFloat(ufContentStart);
    pos.x += contentStart - size.x;

    Vector2f startPos = pos;
    // write centered text
    Vector2f nameSize  = uCalcTextSize(names[current]);
    Vector2f arrowSize = MakeVec2(mCurrentFontAtlas->maxCharWidth);
    float centerOffset = (size.x - nameSize.x) / 2.0f;
    
    pos.x += centerOffset;
    uPushFloat(ufTextScale, uGetFloat(ufTextScale) * 0.8f);
        uText(names[current], pos);    
    uPopFloat(ufTextScale);
    pos.x -= centerOffset + arrowSize.x;

    bool elementFocused = uGetElementFocused();
    uint iconColor = uGetColor(elementFocused ? uColorSelectedBorder : uColorText);
    uPushColor(uColorText, iconColor);
    uText(IC_LEFT_TRIANGLE, pos);
    pos.y -= size.y;
    const CheckOpt chkOpt = CheckOpt_BigColission;
    bool goLeft = elementFocused && GetKeyPressed(Key_LEFT);
    if (ClickCheck(pos, arrowSize, chkOpt) || goLeft)
    {
        if (current > 0) current--;
        else current = numNames - 1;
    }
    pos = startPos;
    pos.x += size.x;
    uText(IC_RIGHT_TRIANGLE, pos);
    pos.y -= size.y;
    bool goRight = elementFocused && GetKeyPressed(Key_RIGHT);
    if (ClickCheck(pos, arrowSize, chkOpt) || goRight)
    {
        if (current < numNames-1) current++;
        else current = 0;
    }
    uPopColor(uColorText);
    return current;
}

FieldRes uIntField(const char* label, Vector2f pos, int* val, int minVal, int maxVal, float dragSpeed)
{
    Vector2f labelSize = Label(label, pos);
    Vector2f size = { uGetFloat(ufFieldWidth), uGetFloat(ufSliderHeight) };
    size.y = labelSize.y; // maybe change: y scale using label height is weierd 

    float contentStart = uGetFloat(ufContentStart);
    pos.x += contentStart - size.x;
    pos.y -= size.y;

    bool clicked = ClickCheck(pos, size, CheckOpt_BigColission);
    uQuad(pos, size, uGetColor(uColorTextBoxBG));

    bool elementFocused = uGetElementFocused();
    uint borderColor = uGetColor(elementFocused ? uColorSelectedBorder : uColorBorder);
    uPushColor(uColorBorder, borderColor);
        uBorder(pos, size);
    uPopColor(uColorBorder);

    FieldRes result = FieldRes_Clicked * clicked;
    int value = *val;
    bool changed = false;

    if (elementFocused)
    {
        bool mousePressing = GetMouseDown(MouseButton_Left);
        Vector2f mousePos;
        GetMouseWindowPos(&mousePos.x, &mousePos.y);
        Vector2f scaledPos = pos * mWindowRatio;
        Vector2f scaledSize = size * mWindowRatio;
        float mouseDiff = (mousePos.x - mMouseOld.x) * mWindowRatio.x;

        if (mousePressing && mousePos.y > scaledPos.y + 0.0f &&
                             mousePos.y < scaledPos.y + scaledSize.y) 
        {
            value += int(mouseDiff * dragSpeed);
            result |= FieldRes_Changed;
        }
        value += GetKeyReleased(Key_RIGHT);
        value -= GetKeyReleased(Key_LEFT);

        const int maxDigits = 10;
        int numDigits = Log10((unsigned)value) + 1;
        int pressedNumber = GetPressedNumber();

        if (pressedNumber != -1 && numDigits < maxDigits) {
            value *= 10;
            value += pressedNumber;
            result |= FieldRes_Changed;
        }
        if (GetKeyPressed(Key_BACK) && value != 0) {
            value -= value % 10;
            value /= 10;
            result |= FieldRes_Changed;
        }
        value = Clamp(value, minVal, maxVal);
        *val = value;
    }
    
    float offset = size.y * 0.1f;
    pos.y += size.y - offset;
    pos.x += offset;

    char valText[16] = {};
    IntToString(valText, value);
    float textScale = uGetFloat(ufTextScale);
    uPushFloat(ufTextScale, textScale * 0.7f);
        uText(valText, pos);
    uPopFloat(ufTextScale);
    return result;
}

static const int tenMap[] = { 1, 10, 100, 1000, 10000 };

// used to remove last fraction of the real number. ie: 12.345
inline float SetFloatFract0(float val, int n)
{
    float ival = float((int)val); // 12.0
    val -= ival; // 0.345
    val *= tenMap[n]; // 345
    int lastDigit = int(val) % 10; // 5
    val -= lastDigit; // 340
    val *= 1.0f / tenMap[n]; // 0.340
    return val + ival; // 12.340
}

FieldRes uFloatField(const char* label, Vector2f pos, float* val, float minVal, float maxVal, float dragSpeed)
{
    Vector2f labelSize = Label(label, pos);
    Vector2f size = { uGetFloat(ufFieldWidth), uGetFloat(ufSliderHeight) };
    size.y = labelSize.y; // maybe change: y scale using label height is weierd 

    float contentStart = uGetFloat(ufContentStart);
    pos.x += contentStart - size.x;
    pos.y -= size.y;

    bool clicked = ClickCheck(pos, size, CheckOpt_BigColission);
    uQuad(pos, size, uGetColor(uColorTextBoxBG));

    bool elementFocused = uGetElementFocused();
    uint borderColor = uGetColor(elementFocused ? uColorSelectedBorder : uColorBorder);
    uPushColor(uColorBorder, borderColor);
        uBorder(pos, size);
    uPopColor(uColorBorder);

    float value = *val;
    bool changed = false;
    FieldRes result = FieldRes_Clicked * clicked;
    int numDigits = Log10(unsigned(Abs(value))) + 1;

    if (elementFocused)
    {
        if (mEditingFloat != val) 
            mLastFloatEditing = false, mFloatDigits = 3;
        
        mEditingFloat = val;
        bool mousePressing = GetMouseDown(MouseButton_Left);
        Vector2f mousePos;
        GetMouseWindowPos(&mousePos.x, &mousePos.y);
        Vector2f scaledPos = pos * mWindowRatio;
        Vector2f scaledSize = size * mWindowRatio;
        float mouseDiff = (mousePos.x - mMouseOld.x) * mWindowRatio.x;

        if (mousePressing && mousePos.y > scaledPos.y + 0.0f && 
            mousePos.y < scaledPos.y + scaledSize.y) 
        {
            value += mouseDiff * dragSpeed;
            mLastFloatEditing = false, mFloatDigits = 3;
            changed = true;
            result |= FieldRes_Changed;
        }
        value += GetKeyReleased(Key_RIGHT) * dragSpeed;
        value -= GetKeyReleased(Key_LEFT) * dragSpeed;

        const int maxDigits = 10;
        int pressedNumber = GetPressedNumber();

        if (pressedNumber != -1 && numDigits < maxDigits) {
            // add pressed number at the end of the number
            mLastFloatEditing = true;
            result |= FieldRes_Changed;
            if (mFloatDigits != 0 || mDotPressed) {
                value = SetFloatFract0(value, mFloatDigits);
                mFloatDigits += mDotPressed;
                value += pressedNumber * (1.0f / tenMap[mFloatDigits++]);
            }
            else {
                value *= 10.0f;
                value += (float)pressedNumber;
            }
            mDotPressed = false;
            mFloatDigits = MIN(mFloatDigits, 4);
        }
        if (GetKeyPressed(Key_BACK)) {
            // remove digit left of the real number
            mLastFloatEditing = true;
            result |= FieldRes_Changed;

            if (mFloatDigits == 0) {
                int ipart = (int)value;
                value -= value - float(ipart); // remove fraction if any
                value -= float(ipart % 10); // remove last digit
                value /= 10.0f; // reduce number of digits
            }
            else {
                value = SetFloatFract0(value, --mFloatDigits);
                mFloatDigits = MAX(mFloatDigits, 0);
            }
        }
        
        value = Clamp(value, minVal, maxVal);
        *val = value;
    }

    float offset = size.y * 0.1f;
    pos.y += size.y - offset;
    pos.x += offset;

    char valText[32] = {};
    if (!mLastFloatEditing) {
        int afterpoint = 4 - MAX(numDigits, 1); // number of digits after dot'.' for example 123.000
        FloatToString(valText, value, afterpoint);
    } 
    else {
        if (mFloatDigits == 0) {
            int lastIdx = IntToString(valText, (int)value);
            if (mDotPressed) valText[lastIdx] = '.';
        } else {
            FloatToString(valText, value, mFloatDigits-1);
        }
    }
    
    float textScale = uGetFloat(ufTextScale);
    uPushFloat(ufTextScale, textScale * 0.7f);
        uText(valText, pos);
    uPopFloat(ufTextScale);
    return result;
}

bool uIntVecField(const char* label, Vector2f pos, int* valArr, int N, int minVal, int maxVal, float dragSpeed)
{
    Vector2f labelSize = Label(label, pos);
    const float fieldWidth = uGetFloat(ufFieldWidth);
    const float padding = fieldWidth * 0.07f;
    // start it from left
    const int Nmin1 = N - 1;
    pos.x -= fieldWidth * Nmin1 + (padding * Nmin1);

    if (!mIndexMap.Contains((void*)valArr)) {
        mIndexMap[(void*)valArr] = 0;
    }

    int currentIndex = mIndexMap[(void*)valArr];
    if (GetKeyPressed(Key_TAB)) {
        mIndexMap[(void*)valArr] = currentIndex < N-1 ? currentIndex+1 : currentIndex;
    }

    bool elementFocused = uGetElementFocused();
    bool changed = 0;
    FieldRes fieldRes;

    for (int i = 0; i < N; i++)
    {
        bool fieldFocused = i == currentIndex && elementFocused;
        uSetElementFocused(fieldFocused);
        fieldRes = uIntField(nullptr, pos, valArr + i, minVal, maxVal, dragSpeed);
        if (!!(fieldRes & FieldRes_Clicked))
            mIndexMap[(void*)valArr] = i;
        changed |= fieldRes > 0;
        pos.x += fieldWidth + padding;
    }
    return changed;
}

bool uFloatVecField(const char* label, Vector2f pos, float* valArr, int N, float minVal, float maxVal, float dragSpeed)
{
    Vector2f labelSize = Label(label, pos);
    const float fieldWidth = uGetFloat(ufFieldWidth);
    const float padding = fieldWidth * 0.07f;
    // start it from left
    const int Nmin1 = N - 1;
    pos.x -= fieldWidth * Nmin1 + (padding * Nmin1);

    if (!mIndexMap.Contains((void*)valArr)) {
        mIndexMap[(void*)valArr] = 0;
    }

    int currentIndex = mIndexMap[(void*)valArr];
    if (GetKeyPressed(Key_TAB)) {
        // todo go to next element when index go out
        mIndexMap[(void*)valArr] = currentIndex < N-1 ? currentIndex+1 : currentIndex;
    }

    bool elementFocused = uGetElementFocused();
    bool changed = 0;
    FieldRes fieldRes;

    for (int i = 0; i < N; i++)
    {
        bool fieldFocused = i == currentIndex && elementFocused;
        uSetElementFocused(fieldFocused);
        fieldRes = uFloatField(nullptr, pos, valArr + i, minVal, maxVal, dragSpeed);
        if (!!(fieldRes & FieldRes_Clicked))
            mIndexMap[(void*)valArr] = i;
        changed |= fieldRes > 0;
        pos.x += fieldWidth + padding;
    }
    return changed;
}

void uBegin()
{
    mAnyTextEdited = false;
}
//------------------------------------------------------------------------
// Rendering

static void uRenderQuads()
{
    Vector2i windowSize = GetWindowSize();
    rBindShader(mQuadShader);

    rUpdateTexture(mQuadPosTex, mQuadPositions);
    rUpdateTexture(mQuadDataTex, mQuadData);

    rSetTexture(mQuadPosTex , 0, posTexLocQuad);
    rSetTexture(mQuadDataTex, 1, dataTexLocQuad);
    
    rSetShaderValue(&mWindowRatio.x, uScaleLocQuad, GraphicType_Vector2f);
    rSetShaderValue(&windowSize.x, uScrSizeLocQuad, GraphicType_Vector2i);

    rRenderMeshNoVertex(6 * mQuadIndex); // 6 index for each char

    mQuadIndex = 0;
}

static void uRenderTexts()
{
    rBindShader(mFontShader);

    {
        rUpdateTexture(mPosTex, mTextPositions);
        rUpdateTexture(mDataTex, mTextData);
        rSetTexture(mPosTex , 0, posTexLoc);
        rSetTexture(mDataTex, 1, dataTexLoc);
        rSetTexture(mCurrentFontAtlas->textureHandle, 3, atlasLoc);
    }

    Vector2i windowSize = GetWindowSize();
    rSetShaderValue(&windowSize.x, uScrSizeLoc, GraphicType_Vector2i);

    rRenderMeshNoVertex(6 * mNumChars); // 6 index for each char
    mNumChars = 0;
}

void uRender()
{
    // prepare
    rSetBlending(true);
    rSetBlendingFunction(rBlendFunc_Alpha, rBlendFunc_OneMinusAlpha);

    rUnpackAlignment(4);
    rClearDepth();

    uRenderQuads();
    uRenderTexts();

    if (!mAnyTextEdited) {
        mCurrText.Editing = false;
    }

    rSetBlending(false);
    GetMouseWindowPos(&mMouseOld.x, &mMouseOld.y);
}

void uDestroy()
{
    if (!mInitialized) 
        return;
    rDeleteShader(mFontShader);
    rDeleteTexture(mPosTex);
    rDeleteTexture(mDataTex);

    for (int i = 0; i < mNumFontAtlas; i++) {
        Texture fakeTex;
        fakeTex.handle = mFontAtlases[i].textureHandle;
        rDeleteTexture(fakeTex);
    }

    rDeleteShader(mQuadShader);
    rDeleteTexture(mQuadPosTex);
    rDeleteTexture(mQuadDataTex);
}