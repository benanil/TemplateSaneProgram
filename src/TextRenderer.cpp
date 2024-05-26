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
#endif

#include "../ASTL/String.hpp"
#include "../ASTL/IO.hpp"
#include "../ASTL/Math/Matrix.hpp"

#include "include/TextRenderer.hpp"
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
    uint padd;
};

namespace
{
    uint mColors[] = { 
        ~0u, 0x8C000000u, // uColorText, uColorQuad
        0x8CFFFFFFu // uColorHover
    };
    const int NumColors = sizeof(mColors) / sizeof(uint);

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

    bool mWasHovered = false; // last button was hovered?

    int mNumChars = 0; // textLen without spaces, or undefined characters    
    char mCurrentDepth;
    bool mInitialized = false;
    // uniform locations
    int posTexLoc, dataTexLoc, atlasLoc, uScrSizeLoc, uScaleLoc;

    //------------------------------------------------------------------------
    // Quad Batch renderer
    const int MaxQuads = 512;

    Shader mQuadShader;

    Vector2f mQuadPositions[MaxQuads];
    Vector2i mQuadData[MaxQuads];

    Texture mQuadPosTex;
    Texture mQuadDataTex;

    int mQuadIndex = 0;
    // uniform locations
    int posTexLocQuad, dataTexLocQuad, uScrSizeLocQuad, uScaleLocQuad;
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
	uScaleLoc     = rGetUniformLocation("uScale");
    mInitialized  = true;
    mCurrentFontAtlas = nullptr, mCurrentDepth = 0;
    
    ScopedText quadVert = ReadAllText("Shaders/QuadBatch.glsl", nullptr, nullptr, AX_SHADER_VERSION_PRECISION());
    ScopedText quadFrag = ReadAllText("Shaders/QuadFrag.glsl", nullptr, nullptr, AX_SHADER_VERSION_PRECISION());
    mQuadShader = rCreateShader(quadVert.text, quadFrag.text);
    
    // per character textures
    mQuadPosTex  = rCreateTexture(MaxQuads, 1, nullptr, TextureType_RG32F , TexFlags_RawData);
    mQuadDataTex = rCreateTexture(MaxQuads, 1, nullptr, TextureType_RG32UI, TexFlags_RawData);
    
    rBindShader(mQuadShader);
    posTexLocQuad   = rGetUniformLocation("posTex");
    dataTexLocQuad  = rGetUniformLocation("dataTex");
    uScrSizeLocQuad = rGetUniformLocation("uScrSize");
    uScaleLocQuad   = rGetUniformLocation("uScale");
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
    ASSERT(mNumFontAtlas < 4); // max 4 atlas for now
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
            currentAtlas->textureHandle = rCreateTexture(AtlasWidth, AtlasWidth, image, TextureType_R8, TexFlags_None).handle;
            mCurrentFontAtlas = currentAtlas;
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

    for (ivfgnt i = 127; i < 127 + aditionalCharCount; i++)
    {
        addUnicodeGlyphFn(aditionalCharacters[i-127], i);
    }

    SaveFontAtlasBin(path, pathLen, currentAtlas, image);
    currentAtlas->textureHandle = rCreateTexture(AtlasWidth, AtlasWidth, image, TextureType_R8, TexFlags_None).handle;
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

void uSetFont(FontHandle font) {
    mCurrentFontAtlas = &mFontAtlases[font];
}

void uSetDepth(char depth) {
    mCurrentDepth = depth;
}

void uSetTheme(uColor what, uint color) {
    mColors[what] = color;
}

uint uGetColor(uColor color) {
    return mColors[color];
}

void uSetTheme(uint* colors) {
    SmallMemCpy(mColors, colors, sizeof(colors));
}

bool uIsHovered() {
    return mWasHovered;
}
// reference resolution is always 1920x1080,
// we will remap input position and scale according to current window size

// todo: allow different scales 
void uText(const char* text, Vector2f position, float scale)
{
    ASSERT(mInitialized == true);
    ASSERT(text != nullptr)
    ASSERT(mCurrentFontAtlas != nullptr); // you have to initialize at least one font
    
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
    ASSERT(txtLen < MaxCharacters);

    position *= mWindowRatio;
    float spaceWidth = (float)mCurrentFontAtlas->characters['0'].width;
    scale *= mUIScale;
    uint color = mColors[uColorText];

    const char* textEnd = text + txtSize;
    ushort scaleFP16 = ConvertFloatToHalf(scale);
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
        text += TextCharFromUtf8(&unicode, text, textEnd);
        chr = UnicodeToAtlasIndex(unicode);

        const FontChar& character = mCurrentFontAtlas->characters[chr];
        // pack per quad data:
        // x = half2:size, y = character: uint8, depth: uint8, scale: half  
        Vector2f size;
        size.x = float(character.width) * scale;
        size.y = float(character.height) * scale;
        mTextData[mNumChars].size  = ConvertFloatToHalf(size.x);
        mTextData[mNumChars].size |= uint(ConvertFloatToHalf(size.y)) << 16;

        mTextData[mNumChars].character = chr;  
        mTextData[mNumChars].depth = mCurrentDepth;
        mTextData[mNumChars].scale = scaleFP16;
        mTextData[mNumChars].color = color;

        Vector2f pos = position;
        pos.x += float(character.xoff) * scale;
        pos.y += float(character.yoff) * scale;
        mTextPositions[mNumChars] = pos;

        position.x += character.advence * scale;
        mNumChars++;
    }
}

void uRenderTexts()
{
    rBindShader(mFontShader);
    rSetBlending(true);
    rSetBlendingFunction(rBlendFunc_Alpha, rBlendFunc_OneMinusAlpha);

    {
        rUpdateTexture(mPosTex, mTextPositions);
        rUpdateTexture(mDataTex, mTextData);
        rSetTexture(mPosTex , 0, posTexLoc);
        rSetTexture(mDataTex, 1, dataTexLoc);
    }
    rSetTexture(mCurrentFontAtlas->textureHandle, 3, atlasLoc);

    Vector2i windowSize = GetWindowSize();
    rSetShaderValue(&windowSize.x, uScrSizeLoc, GraphicType_Vector2i);

    rRenderMeshNoVertex(6 * mNumChars); // 6 index for each char
    mNumChars = 0;
    rSetBlending(false);
}

Vector2f uCalcTextSize(const char* text, float scale)
{
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
        text += TextCharFromUtf8(&unicode, text, textEnd);
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
    ASSERT(mQuadIndex < MaxQuads);
    if (mQuadIndex >= MaxQuads) return;

    mQuadPositions[mQuadIndex] = position * mWindowRatio;

    mQuadData[mQuadIndex].x  = ConvertFloatToHalf(scale.x);
    mQuadData[mQuadIndex].x |= uint32_t(ConvertFloatToHalf(scale.y)) << 16;
    
    mQuadData[mQuadIndex].y = color;
    mQuadIndex++;
}

static void uRenderQuads()
{
    Vector2i windowSize = GetWindowSize();
    rSetBlending(true);
    rSetBlendingFunction(rBlendFunc_Alpha, rBlendFunc_OneMinusAlpha);

    rBindShader(mQuadShader);

    rUpdateTexture(mQuadPosTex, mQuadPositions);
    rUpdateTexture(mQuadDataTex, mQuadData);

    rSetTexture(mQuadPosTex , 0, posTexLocQuad);
    rSetTexture(mQuadDataTex, 1, dataTexLocQuad);
    
    rSetShaderValue(&mWindowRatio.x, uScaleLocQuad, GraphicType_Vector2f);
    rSetShaderValue(&windowSize.x, uScrSizeLocQuad, GraphicType_Vector2i);

    rRenderMeshNoVertex(6 * mQuadIndex); // 6 index for each char

    mQuadIndex = 0;
       
    rSetBlending(false);
}

static bool ClickCheck(Vector2f pos, Vector2f scale)
{
    Vector2f mousePos;
    GetMouseWindowPos(&mousePos.x, &mousePos.y);
    
    Vector2f scaledPos = pos * mWindowRatio;
    Vector2f scaledScale = scale * mWindowRatio;
    mWasHovered = PointBoxIntersection(scaledPos, scaledPos + scaledScale, mousePos);
    bool pressed = mWasHovered && GetMouseReleased(MouseButton_Left);
    return pressed;
}

//------------------------------------------------------------------------
bool uButton(const char* text, Vector2f pos, Vector2f scale, uButtonOptions opt)
{
    bool pressed = ClickCheck(pos, scale);

    uint quadColor = mColors[uColorQuad];
    if (mWasHovered || opt == uButtonOptions_Hovered)
        quadColor = mColors[uColorHovered];

    uQuad(pos, scale, quadColor);
    if (!text) return pressed;
    
    Vector2f textSize = uCalcTextSize(text, 1.0f);
    // align text to the center of the button
    Vector2f padding = (scale - textSize) * 0.5f;
    pos.y += textSize.y;
    pos += padding;
    uText(text, pos, 1.0f);
    return pressed;
}

bool uCheckBox(const char* text, bool* isEnabled, Vector2f pos, float scale)
{
    Vector2f textSize = uCalcTextSize(text, scale);
    uText(text, pos, 1.0f);

    const float boxPadding = 10.0f;
    Vector2f boxScale = { textSize.y, textSize.y };
    pos.x += textSize.x + boxPadding;
    pos.y -= textSize.y;

    uQuad(pos, boxScale, 0x7BCDABCDu);

    bool enabled = *isEnabled;
    if (ClickCheck(pos, boxScale)) {
        enabled = !enabled;
    }

    if (enabled) {
        const char* checkmark = IC_CHECK_MARK;
        pos.y += textSize.y;
        uText(checkmark, pos, scale);
    }

    bool changed = enabled != *isEnabled;
    *isEnabled = enabled;
    return changed;
}

bool uSlider(Vector2f pos, float* val, float scale)
{
    return false;
}

void uRender()
{
    uRenderQuads();
    uRenderTexts();
}