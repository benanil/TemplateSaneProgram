

#define STB_TRUETYPE_IMPLEMENTATION  
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../External/stb_truetype.h"
#include "../External/stb_image_write.h"

#include "../ASTL/String.hpp"
#include "../ASTL/IO.hpp"
#include "../ASTL/Math/Matrix.hpp"

#include "include/TextRenderer.hpp"
#include "include/Renderer.hpp"
#include "include/Platform.hpp"

struct FontChar
{
    short width, height;
    short xoff, yoff;
    float advence;
};

static const int CellCount  = 12;
static const int CharSize   = 48;
static const int AtlasWidth = CellCount * CharSize;
static const int MaxCharacters = 512;

struct FontAtlas
{
    FontChar characters[CellCount*CellCount];
    stbtt_fontinfo info;
    unsigned int textureHandle;
    
    const FontChar& GetCharacter(unsigned int c) const
    {
        return characters[c];
    }
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
    posTexLoc   = rGetUniformLocation("posTex");
    sizeTexLoc  = rGetUniformLocation("sizeTex");
    charTexLoc  = rGetUniformLocation("charTex");
    atlasLoc    = rGetUniformLocation("atlas");
    uScrSizeLoc = rGetUniformLocation("uScrSize");
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
    int xStart = (i * CharSize) % atlasWidth;
    int yStart = (i / CellCount) * CharSize;
    
    for (int yi = yStart, y = 0; yi < yStart + character->height; yi++, y++)
    {
        for (int xi = xStart, x = 0; xi < xStart + character->width; xi++, x++)
        {
            atlas[yi][xi] = sdf[(y * character->width) + x];
        }
    }
}

FontAtlasHandle LoadFontAtlas(const char* file)
{
    ASSERT(mCurrentFontAtlas < 4);
    ScopedPtr<unsigned char> data = (unsigned char*)ReadAllFile(file);
    if (data.ptr == nullptr) {
        return InvalidFontHandle;
    }
    
    FontAtlas* currentAtlas = &mFontAtlases[mCurrentFontAtlas++];
    stbtt_fontinfo* info = &currentAtlas->info;

    int res = stbtt_InitFont(info, data.ptr, 0);
    ASSERT(res != 0);

    // These functions compute a discretized SDF field for a single character, suitable for storing
    // in a single-channel texture, sampling with bilinear filtering, and testing against
    
    int padding = 0;
    unsigned char onedge_value = 128;
    float pixel_dist_scale = 64.0f;
    float scale = stbtt_ScaleForPixelHeight(info, (float)CharSize);
    rUnpackAlignment(1);

    // 12 is sqrt(128)
    unsigned char atlas[AtlasWidth][AtlasWidth] = {0};

    for (int i = '!'; i <= '~'; i++) // ascii characters
    {
        FontChar& character = currentAtlas->characters[i];
        int xoff, yoff, width, height;
        uint8* sdf = stbtt_GetCodepointSDF(info, 
                                           scale, 
                                           i, 
                                           padding, 
                                           onedge_value, 
                                           pixel_dist_scale, 
                                           &width, &height, &xoff, &yoff);
        ASSERT(sdf != nullptr);
        character.xoff = (short)xoff, character.width  = (short)width;
        character.yoff = (short)yoff, character.height = (short)height;

        int advance, leftSideBearing;
        stbtt_GetCodepointHMetrics(info, i, &advance, &leftSideBearing);
        character.advence = advance * scale;
        WriteGlyphToAtlass(i, &character, atlas, sdf);
        free(sdf); //STBTT_free(image);
    }
    
    const auto addUnicodeGlyphFn = [&](int unicode, int i)
    {
        FontChar& character = currentAtlas->characters[i];
        int glyph = stbtt_FindGlyphIndex(info, unicode);
        int xoff, yoff, width, height;
        uint8* sdf = stbtt_GetGlyphSDF(info, scale, 
                                       glyph,
                                       padding, onedge_value, pixel_dist_scale,
                                       &width, &height,
                                       &xoff, &yoff);
        ASSERT(sdf != nullptr);
        character.xoff = (short)xoff, character.width  = (short)width;
        character.yoff = (short)yoff, character.height = (short)height;

        int advance, leftSideBearing;
        stbtt_GetGlyphHMetrics(info, glyph, &advance, &leftSideBearing);
        character.advence = advance * scale;
        WriteGlyphToAtlass(i, &character, atlas, sdf);
        free(sdf); 
    };

    // Turkisih and European characters.
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

    for (int i = 0; i < europeanGlyphCount; i++) // iterate trough first and last characters of ascii
    {
        addUnicodeGlyphFn(europeanChars[i], i);
    }
    
    // we can add 17 more characters for filling 128 char restriction
    const int aditionalCharacters[] = { 
        0x23F3, // hourglass flowing sand
        0x23F4, 0x23F5, 0x23F6, 0x23F7, // <, >, ^, v,
        0x23F8, 0X23F9, 0X23FA,         // ||, square, O 
        0x21BA, 0x23F0, //  ↺ anticlockwise arrow, alarm
        0x2605, 0x2764, 0x2714, 0x0130 // Star, hearth, speaker, İ
    };

    constexpr int aditionalCharCount = ArraySize(aditionalCharacters);
    static_assert(aditionalCharCount + 127 < 144); // if you want more you have to make bigger atlas than 12x12

    for (int i = 127; i < 127 + aditionalCharCount; i++) // iterate trough first and last characters of ascii
    {
        addUnicodeGlyphFn(aditionalCharacters[i-127], i);
    }

    stbi_write_png("font_test.png", AtlasWidth, AtlasWidth, 1, atlas, AtlasWidth);
    currentAtlas->textureHandle = rCreateTexture(AtlasWidth, AtlasWidth, atlas, TextureType_R8, TexFlags_None).handle;
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
    if (unicode <= 256)
    {
        static constexpr UTF8Table table{};
        return table.map[unicode];
    }

    switch (unicode) {
        case 0x011Fu: return 3;   // ğ
        case 0x015Fu: return 4;   // ş
        case 0x0131u: return 5;   // ı
        case 0x0142u: return 14;  // ł 
        case 0x0107u: return 15;  // ć 
        // upper case
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
void DrawText(const char* text, float xPos, float yPos, float scale)
{
    int txtLen  = UTF8StrLen(text);
    int txtSize = StringLength(text);

    ASSERT(txtLen < MaxCharacters);
    ASSERT(mInitialized == true);

    Vector2f pos  = { xPos , yPos };
    Vector2f size;

    FontAtlas* fontAtlas = &mFontAtlases[0];
    stbtt_fontinfo* info = &fontAtlas->info;
    float spaceWidth = (float)fontAtlas->GetCharacter('0').width;

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

        const FontChar& character = fontAtlas->GetCharacter(chr);
        /* set size */ {
            size.x = float(character.width) * scale;
            size.y = float(character.height) * scale;
            sizes[numChars].x = ConvertFloatToHalf(size.x);
            sizes[numChars].y = ConvertFloatToHalf(size.y);
        }
        /* set position */ {
            pos.x = xPos + (float(character.xoff) * scale);
            pos.y = yPos + (float(character.yoff) * scale);
            positions[numChars] = pos;
        }
        /* set tile */ {
            charData[numChars] = chr;
        }

        // int kern = stbtt_GetCodepointKernAdvance(&mFontAtlases[0].info, chr, text[i + 1]);
        // xPos += kern * scale;
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
