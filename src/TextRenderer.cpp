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

namespace 
{
    FontAtlas mFontAtlases[4] = {};
    Shader fontShader;
    Texture mPosTexture;
    Texture mScaleTexture;
    Texture mCharTexture;
    int mCurrentFontAtlas = 0;
    bool mInitialized = false;
    // uniform locations
    int posTexLoc, sizeTexLoc, charTexLoc, atlasLoc, uScrSizeLoc;
}

void TextRendererInitialize()
{
    ScopedText vert = ReadAllText("Shaders/TextVert.glsl", nullptr, nullptr, AX_SHADER_VERSION_PRECISION());
    ScopedText frag = ReadAllText("Shaders/TextFrag.glsl", nullptr, nullptr, AX_SHADER_VERSION_PRECISION());
    fontShader = rCreateShader(vert.text, frag.text);
    
    // per character textures
    mPosTexture   = rCreateTexture(MaxCharacters, 1, nullptr, TextureType_RG32F , TexFlags_RawData);
    mScaleTexture = rCreateTexture(MaxCharacters, 1, nullptr, TextureType_RG16F , TexFlags_RawData);
    mCharTexture  = rCreateTexture(MaxCharacters, 1, nullptr, TextureType_R8UI, TexFlags_RawData);
    
    rBindShader(fontShader);
    posTexLoc    = rGetUniformLocation("posTex");
    sizeTexLoc   = rGetUniformLocation("sizeTex");
    charTexLoc   = rGetUniformLocation("charTex");
    atlasLoc     = rGetUniformLocation("atlas");
    uScrSizeLoc  = rGetUniformLocation("uScrSize");
    mInitialized = true;
}

void DestroyTextRenderer()
{
    if (!mInitialized) 
        return;
    rDeleteShader(fontShader);
    rDeleteTexture(mPosTexture);
    rDeleteTexture(mScaleTexture);
    rDeleteTexture(mCharTexture);

    for (int i = 0; i < mCurrentFontAtlas; i++) {
        Texture fakeTex;
        fakeTex.handle = mFontAtlases[i].textureHandle;
        rDeleteTexture(fakeTex);
    }
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

FontAtlasHandle LoadFontAtlas(const char* file)
{
    ASSERT(mCurrentFontAtlas < 4); // max 4 atlas for now
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
            currentAtlas = &mFontAtlases[mCurrentFontAtlas++];
            LoadFontAtlasBin(path, currentAtlas, image);
            currentAtlas->textureHandle = rCreateTexture(AtlasWidth, AtlasWidth, image, TextureType_R8, TexFlags_None).handle;
            return mCurrentFontAtlas - 1;
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
    
    currentAtlas = &mFontAtlases[mCurrentFontAtlas++];
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

    SaveFontAtlasBin(path, pathLen, currentAtlas, image);
    currentAtlas->textureHandle = rCreateTexture(AtlasWidth, AtlasWidth, image, TextureType_R8, TexFlags_None).handle;
#else
    AX_UNREACHABLE();
#endif
    return mCurrentFontAtlas-1;
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


// todo: allow different scales 
void DrawText(const char* text, float xPos, float yPos, float scale, FontAtlasHandle atlasHandle)
{
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
    
    ASSERT(atlasHandle < mCurrentFontAtlas); // you have to initialize at least one font
    ASSERT(txtLen < MaxCharacters);
    ASSERT(mInitialized == true);

    Vector2f pos  = { xPos , yPos };
    Vector2f size;

    FontAtlas* fontAtlas = &mFontAtlases[atlasHandle];
    float spaceWidth = (float)fontAtlas->characters['0'].width;

    Vector2f positions[MaxCharacters];
    Vector2h sizes[MaxCharacters];
    uint8 charData[MaxCharacters];
    int numChars = 0; // textLen without spaces, or undefined characters
    
    const char* textEnd = text + txtSize;
    // fill per quad data
    while (text < textEnd)
    {
        if (*text == ' ') {
            xPos += spaceWidth * scale * 0.5f;
            text++;
            continue;
        }

        unsigned int chr;
        unsigned int unicode;
        text += TextCharFromUtf8(&unicode, text, textEnd);
        chr = UnicodeToAtlasIndex(unicode);

        const FontChar& character = fontAtlas->characters[chr];
        
        {
            size.x = float(character.width) * scale;
            size.y = float(character.height) * scale;
            sizes[numChars].x = ConvertFloatToHalf(size.x);
            sizes[numChars].y = ConvertFloatToHalf(size.y);
        }
        {
            pos.x = xPos + (float(character.xoff) * scale);
            pos.y = yPos + (float(character.yoff) * scale);
            positions[numChars] = pos;
        }
        {
            charData[numChars] = chr;
        }

        // xPos += (padding-1) * scale;
        xPos += character.advence * scale;
        numChars++;
    }
    
    rBindShader(fontShader);
    rSetBlending(true);
    rSetBlendingFunction(rBlendFunc_Alpha, rBlendFunc_OneMinusAlpha);

    {
        rUpdateTexture(mPosTexture, positions);
        rUpdateTexture(mScaleTexture, sizes);
        rUpdateTexture(mCharTexture, charData);

        rSetTexture(mPosTexture  , 0, posTexLoc);
        rSetTexture(mScaleTexture, 1, sizeTexLoc);
        rSetTexture(mCharTexture , 2, charTexLoc);
    }
    rSetTexture(fontAtlas->textureHandle, 3, atlasLoc);

    Vector2i windowSize;
    wGetWindowSize(&windowSize.x, &windowSize.y);
    rSetShaderValue(&windowSize.x, uScrSizeLoc, GraphicType_Vector2i);

    rRenderMeshNoVertex(6 * numChars); // 6 index for each char

    rSetBlending(false);
}
