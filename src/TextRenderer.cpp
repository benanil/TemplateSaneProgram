

#define STB_TRUETYPE_IMPLEMENTATION  
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../External/stb_truetype.h"
#include "../External/stb_image_write.h"

#include "../ASTL/IO.hpp"
#include "../ASTL/Memory.hpp"
#include "../ASTL/Math/Matrix.hpp"

#include "include/TextRenderer.hpp"
#include "include/Renderer.hpp"
#include "include/Platform.hpp"

struct FontChar
{
    int width, height;
    int xoff, yoff;
    float advence;
};

struct FontAtlas
{
    FontChar characters[128-'!'];
    stbtt_fontinfo info;
    unsigned int textureHandle;
    
    const FontChar& GetCharacter(char c) const
    {
        return characters[c - '!'];
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
    
    const int CharSize = 48;
    const int MaxCharacters = 512;
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
    unsigned char atlas[12 * CharSize][12 * CharSize] = {0};

    for (int i = '!'; i <= '~'; i++) // iterate trough first and last characters of ascii
    {
        FontChar& character = currentAtlas->characters[i - '!'];

        unsigned char* sdf = stbtt_GetCodepointSDF(info, 
                                           scale, 
                                           i, 
                                           padding, 
                                           onedge_value, 
                                           pixel_dist_scale, 
                                           &character.width, &character.height, &character.xoff, &character.yoff);
        ASSERT(sdf != nullptr);

        int advance, leftSideBearing;
        stbtt_GetCodepointHMetrics(info, i, &advance, &leftSideBearing);
        character.advence = advance * scale;
        
        const int atlasWidth = 12 * CharSize;
        int xStart = (i * CharSize) % atlasWidth;
        int yStart = (i / 12) * CharSize;

        for (int yi = yStart, y = 0; yi < yStart + character.height; yi++, y++)
        {
            for (int xi = xStart, x = 0; xi < xStart + character.width; xi++, x++)
            {
                atlas[yi][xi] = sdf[(y * character.width) + x];
            }
        }

        free(sdf); //STBTT_free(image);
    }
    // stbi_write_png("font_test.png", 12 * CharSize, 12 * CharSize, 1, atlas, 12 * CharSize);
    currentAtlas->textureHandle = rCreateTexture(12 * CharSize, 12 * CharSize, atlas, TextureType_R8, TexFlags_None).handle;
    return mCurrentFontAtlas-1;
}

void DrawText(const char* text, int txtLen, float xPos, float yPos, float width, float height)
{
    ASSERT(txtLen < MaxCharacters);
    ASSERT(mInitialized == true);

    Vector2f pos  = { xPos , yPos };
    Vector2f size;

    FontAtlas* fontAtlas = &mFontAtlases[0];
    stbtt_fontinfo* info = &fontAtlas->info;
    float scale = 1.0f;
    float spaceWidth = (float)fontAtlas->GetCharacter('0').width;

    Vector2f positions[MaxCharacters];
    Vector2h sizes[MaxCharacters];
    uint8 charData[MaxCharacters];
    int numChars = 0; // textLen without spaces, or undefined characters

    // fill per quad data
    for (int i = 0; i < txtLen; i++)
    {
        char chr = text[i];
        if (chr == ' ') {
            xPos += spaceWidth * scale * 0.5f;
            continue;
        }
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
