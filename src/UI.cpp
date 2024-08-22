/*******************************************************************************************
*                                                                                         *
* Purpose:                                                                                *
*     Immediate Mode Graphical Rendering System for rendering user interfaces             *
*     Importing Fonts, Creating font atlases, rendering Text...                           *
*     There are 3 type of rendering:                                                      *
*         Quad Rendering: for quad shapes (lines, backgrounds, buttons etc.)              *
*         Triangle Rendering: for rounded shapes (circle capsule etc.)                    *
*         TextRenderer Rendering: its an single pass text batch renderer with atlas       *
*     Quad and Text renderers works without Vertex or index buffers,                      *
*     Storing per quad data in textures instead of per vertex data,                       *
*     sending data to GPU every frame so less data is faster.                             *
*     we are packing data in CPU and unpacking in the GPU.                                *
*     Fixed Point float number used to store position data with 16 bit integers           *
* Usage:                                                                                  *
*     Position (0.0, 0.0) is starting point Top left corner when using UI functions       *
*     be aware of that, Most of the functions are taking an position and scale,           *
*     if you not define the scale some of the functions will automatically                *
*     set the scale for you
* Note:                                                                                   *
*     if you want icons, font must have Unicode Block 'Miscellaneous Technical'           *
*     I've analysed the european countries languages and alphabets                        *
*     to support most used letters in European languages.                                 *
*     English, Turkish, German, Brazilian, Portuguese, Finnish, Swedish fully supported   *
*     for letters that are not supported in other nations I've used transliteration       *
*     to get closest character. for now 12*12 = 144 character supported                   *
*     each character is maximum 48x48px                                                   *
*                                                                                         *
* Author:                                                                                 *
*     Anilcan Gulkaya 2024 anilcangulkaya7@gmail.com                                      *
*******************************************************************************************/

#include "include/AssetManager.hpp" // for AX_GAME_BUILD macro

#if AX_GAME_BUILD == 0 // we don't need to create sdf fonts at runtime when playing game
    #define STB_TRUETYPE_IMPLEMENTATION  
    #include "../External/stb_truetype.h"
    #define STB_IMAGE_WRITE_IMPLEMENTATION // you might want to see atlas
    #include "../External/stb_image_write.h"
#endif

#include "../External/miniaudio.h"

#include "../ASTL/String.hpp"
#include "../ASTL/Array.hpp"
#include "../ASTL/IO.hpp"
#include "../ASTL/Math/Color.hpp"
#include "../ASTL/Math/SIMDVectorMath.hpp"
#include "../ASTL/Random.hpp"

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
static const float PixelDistScale = 18.5f;//18.0f;

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

// we will store this in RGBA32u texture
// x = fixed16x2: size
// y = fixed16x2: position
// z = character: char, depth: uint8, scale: half
// w = rgba8 color
struct TextData
{
    uint size;
    uint8 character, depth;
    half scale;
    uint color;
    ushort posX;
    ushort posY;
};

struct QuadData
{
    uint size;
    uint color;
    uint8 depth;
    uint8 cutStart;
    uint8 padding; // < maybe use this as rotation angle
    uint8 effect;
    ushort posX, posY;
};

struct ScissorRect
{
    Vector2f position;
    ushort sizeX, sizeY;
    int numElements;
};

const int MaxScissor = 32;

// we are going to use two pointer algorithm, stencilled quads or texts will be at the beginning of the array
// so we will render stencilled and non stencilled with seperate draw calls, this way we don't need extra array too
struct ScissorData
{
    int count = 0;
    int numRect = 0;
    ScissorRect rects[MaxScissor];
    
    void PushNewRect(Vector2f pos, Vector2f scale) {
        ScissorRect& rect = rects[numRect];
        rect.position = pos;
        rect.sizeX = (ushort)scale.x;
        rect.sizeY = (ushort)scale.y;
        rect.numElements = 0;
    }

    void Reset() {
        count = 0;
        numRect = 0;
    }
};

namespace
{
    FontAtlas mFontAtlases[4] = {};
    FontAtlas* mCurrentFontAtlas;
    int mNumFontAtlas = 0;

    // ratio against 1920x1080, [1.0, 1.0] if 1080p 0.5 if half of it, 2.0 if two times bigger
    // aspect ratio fixer thingy
    Vector2f mWindowRatio;
    float mUIScale; // min(mWindowRatio.x, mWindowRatio.y)

    Vector2f mMouseOld;
    bool mWasHovered = false; // last element was hovered ? (button, textField, int&float field)
    bool mAnyElementClicked = false;
    bool mInitialized = false; // is renderers initialized?
    bool mElementFocused[8] = {}; // stack data structure
    int mElementFocusedIndex = 0;
    uScissorMask mScissorMask = 0;

    //------------------------------------------------------------------------
    // Text Batch renderer
    Shader   mFontShader;
    Texture  mTextDataTex; // x = half2:size, y = character: uint8, depth: uint8, scale: half  
    TextData mTextData[MaxCharacters];
    int mNumChars = 0; // textLen without spaces, or undefined characters
    ScissorData mTextScissor = {};

    int dataTexLoc, atlasLoc, uScrSizeLoc, uIndexStartLocText; // uniform locations

    //------------------------------------------------------------------------
    // Quad Batch renderer
    const int MaxQuads = 512;
    Shader   mQuadShader;
    QuadData mQuadData[MaxQuads]= {};
    Texture  mQuadDataTex;
    int mQuadIndex = 0;

    int dataTexLocQuad, uScrSizeLocQuad, uScaleLocQuad, uIndexStartLocQuad; // uniform locations
    
    ScissorData mQuadScissor = {};
    
    //------------------------------------------------------------------------
    // Color Pick
    struct ColorPick
    {
        Shader shader;
        Vector2f size;
        Vector2f pos;
        Vector3f hsv;
        float alpha;
        bool isOpen;
    };
    ColorPick mColorPick;

    bool mDropdownOpen = false;
    int mLastHoveredDropdown = 0;
    size_t mCurrentDropdown; // < pointer of last edited dropdown label

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

    bool mLastFloatWriting = false;
    bool mAnyFloatEdited = false;
    bool mDotPressed = false;
    int mFloatDigits = 3;
    float* mEditedFloat;
        
    //------------------------------------------------------------------------
    // configuration
    uint mColors[] = { 
        0xFFE1E1E1u, // uColorText
        0xFF111111u, // uColorQuad
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
        1.5f  , // line thickness  
        160.0f, // ufContentStart
        12.0f , // ButtonSpace
        1.0f  , // TextScale,
        175.0f, // TextBoxWidth
        18.0f , // Slider Width
        0.9f  , // Depth 
        98.0f , // Field Width
        100.0f  // Text Wrap Width
    };
    constexpr int NumFloats = sizeof(mFloats) / sizeof(float);
    // stack for pushed floats
    float mFloatStack[NumFloats][StackSize];
    int mFloatStackCnt[NumFloats] = { };
    
    //------------------------------------------------------------------------
    // Sound
    ASound mButtonClickSound;
    ASound mButtonHoverSound;
 
    //------------------------------------------------------------------------
    // Sprite
    struct Sprite
    {
        Vector2f pos, scale;
        int handle;
    };
    const int MaxSprites = 128;
    int mNumSprites = 0;
    Sprite mSprites[MaxSprites];
    Shader mTextureDrawShader;
    
    //------------------------------------------------------------------------
    // Triangle Renderer
    
    struct TriangleVert
    {
        ushort posX, posY;
        uint8 fade;
        uint8 depth;
        uint8 cutStart;
        uint8 effect; 
        uint  color;
        uint8  padding[4];
    };

    GPUMesh mTriangleMesh;
    Array<TriangleVert> mTriangles={};
    int mCurrentTriangle = 0;
    Shader mTriangleShader;
}

void PlayButtonClickSound() {
    SoundRewind(mButtonClickSound);
    SoundPlay(mButtonClickSound);
}

void PlayButtonHoverSound() {
    SoundRewind(mButtonHoverSound);
    SoundPlay(mButtonHoverSound);
}

void uWindowResizeCallback(int width, int height)
{
    mWindowRatio = { width / 1920.0f, height / 1080.0f };
    mUIScale = (mWindowRatio.x + mWindowRatio.y) * 0.5f; // min(ratio.x, ratio.y)
    // ultra wide
    if (MAX(mWindowRatio.x, mWindowRatio.y) - MIN(mWindowRatio.x, mWindowRatio.y) > 0.6f) 
    {
        mUIScale = MIN(mWindowRatio.x, mWindowRatio.y);
    }
}

static void ImportShaders()
{
    mTriangleShader = rImportShader("Shaders/UITriangleVert.glsl", "Shaders/UITriangleFrag.glsl");
    mFontShader = rImportShader("Shaders/TextVert.glsl", "Shaders/TextFrag.glsl");
    mQuadShader = rImportShader("Shaders/QuadBatch.glsl", "Shaders/QuadFrag.glsl");
    mColorPick.shader = rImportShader("Shaders/SingleQuadVert.glsl", "Shaders/ColorPickFrag.glsl");
    mColorPick.alpha = 1.0f;

    ScopedText spriteDrawVertTxt   = ReadAllText("Shaders/SingleQuadVert.glsl", nullptr, nullptr, AX_SHADER_VERSION_PRECISION());
    const char* textureDrawFragTxt = AX_SHADER_VERSION_PRECISION() R"(
    layout(location = 0) in mediump vec2 vTexCoord; out vec4 color; uniform sampler2D tex;
    void main() {
        color = vec4(texture(tex, vec2(vTexCoord.x, 1.0-vTexCoord.y)).rgb, 1.0);
    })";
    mTextureDrawShader = rCreateShader(spriteDrawVertTxt.text, textureDrawFragTxt, "TextureDrawVert", "TextureDrawFrag");
}

static void InitSounds()
{
    mButtonClickSound = LoadSound("Audio/button-click.wav");
    mButtonHoverSound = LoadSound("Audio/pluck_001.ogg");
    SoundSetVolume(mButtonClickSound, 0.5f);
    SoundSetVolume(mButtonHoverSound, 0.5f);
}

void CreateTriangleMesh(int size)
{
    if (mTriangles.Size() != 0)
        rDeleteMesh(mTriangleMesh);

    const InputLayout inputLayout[] = {
        { 1, GraphicType_UnsignedInt     }, // < fixed point position.xy
        { 1, GraphicType_UnsignedInt     }, // < fade, depth, effect, cutStart
        { 4, GraphicType_UnsignedByte | GraphicType_NormalizeBit }, // color
        { 1, GraphicType_UnsignedInt     } // < padding
    };
    InputLayoutDesc description;
    description.numLayout = ArraySize(inputLayout);
    description.stride = sizeof(TriangleVert);
    description.layout = inputLayout;
    description.dynamic = true;

    mTriangles.Resize(size);
    mTriangleMesh = rCreateMesh(nullptr, nullptr, mTriangles.Size(), 0, 0, &description);
}

void uInitialize()
{
    InitSounds();
    ImportShaders();
    CreateTriangleMesh(2048);

    // per character textures
    // we will store mTextData array in this texture
    mTextDataTex = rCreateTexture(MaxCharacters, 1, nullptr, TextureType_RGBA32UI, TexFlags_RawData);
    mQuadDataTex = rCreateTexture(MaxQuads, 1, nullptr, TextureType_RGBA32UI, TexFlags_RawData);
    
    rBindShader(mFontShader);
    dataTexLoc    = rGetUniformLocation("dataTex");
    atlasLoc      = rGetUniformLocation("atlas");
    uScrSizeLoc   = rGetUniformLocation("uScrSize");
    uIndexStartLocText = rGetUniformLocation("uIndexStart");
    mInitialized  = true;
    mCurrentFontAtlas = nullptr;
    
    rBindShader(mQuadShader);
    dataTexLocQuad  = rGetUniformLocation("dataTex");
    uScrSizeLocQuad = rGetUniformLocation("uScrSize");
    uScaleLocQuad   = rGetUniformLocation("uScale");
    uIndexStartLocQuad = rGetUniformLocation("uIndexStart");
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
    AFile file = AFileOpen(path, AOpenFlag_WriteBinary);
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
    AFile file = AFileOpen(path, AOpenFlag_ReadBinary);
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
    AFile file = AFileOpen(path, AOpenFlag_ReadBinary);
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
    // .otf or .ttf there are two types of true type font file
    bool wasOTF = FileHasExtension(file, pathLen, ".otf"); 
    ChangeExtension(path, pathLen, wasOTF ? "otf" : "ttf");

    ScopedPtr<unsigned char> data = (unsigned char*)ReadAllFile(file);
    if (data.ptr == nullptr) {
        return InvalidFontHandle;
    }
    
    stbtt_fontinfo info = {};
    int res = stbtt_InitFont(&info, data.ptr, 0);
    ASSERTR(res != 0, return 0);
    
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
        ASSERTR(sdf != nullptr, return);
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
        0x2605, 0x2764, 0x2714, 0x0130 // Star, hearth, checkmark, İ
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
    if (unicode < 256)     {
        static constexpr UTF8Table table{};
        return table.map[unicode];
    }
    
    switch (unicode) {
        case 0x011Fu: case 0xC49F: return 3;  // ğ
        case 0x015Fu: case 0xC59F: return 4;  // ş
        case 0x0131u: case 0xC4B1: return 5;  // ı
        case 0x0142u: case 0xC582: return 14; // ł 
        case 0x0107u: case 0xC487: return 15; // ć 
        case 0x011Eu: case 0xC49E: return 20; // Ğ 
        case 0x015Eu: case 0xC59E: return 21; // Ş 
        case 0x0130u: case 0xC4B0: return 140; // İ
        case 0x0141u: case 0xC581: return 30; // Ł
        case 0x0106u: case 0xC486: return 31; // Ć 
        case 0x1E9Eu: return 23; // ẞ
        case 0xC3BC:  return 0; // ü
        case 0x21BAu: return 135; // ↺ anticlockwise arrow
        case 0x23F0u: return 136; // alarm
        case 0x2605u: return 137; // Star
        case 0x2764u: return 138; // hearth
        case 0x2714u: return 139; // checkmark
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
    SmallMemCpy(mColors, colors, sizeof(mColors));
}

void uPushColor(uColor color, uint val) {
    int& index = mColorStackCnt[color];
    ASSERTR(index < StackSize, return);
    mColorStack[color][index++] = val;
}

void uPushFloat(uFloat what, float val) {
    int& index = mFloatStackCnt[what];
    ASSERTR(index < StackSize, return);
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

// maximum supported number is: 6553.5 (16 bit unsigned number last digit used as fraction)
// it overflows for 8k monitors which is 7680px wide so we have to make the viewport width 1000px smaller, 
// using lower resolution framebuffers are not going to be noticable and it will improve performance. but if we are designing our software for that monitors we can change this code :=)
// it works like this: 123.4 * 10 = 1234.0 -> 1234 when we unpacking we devide by 10: 1234.0/10= 123.4
inline ushort MakeFixed(float f)
{
    return ushort(MIN(uint(f * 10.0f), 0xFFFFu));
}

// reference resolution is always 1920x1080,
// we will remap input position and scale according to current window size
void uText(const char* text, Vector2f position, uTextFlags flags)
{
    if (text == nullptr) return;
    ASSERTR(mInitialized == true, return);
    ASSERTR(mCurrentFontAtlas != nullptr, return); // you have to initialize at least one font
    
    int txtLen = 0, txtSize = 0; // txtLen is number of utf characters txtSize is size in bytes
    {
        const char* s = text;
        while (*s) 
        {
            if ((*s & 0xC0) != 0x80) txtLen++;
            txtSize++;
            s++;
        }
    }
    ASSERTR(mNumChars + txtLen < MaxCharacters, return);
    if (txtLen == 0) return;

    TextData* textData;
    int numNonScissor = mNumChars - mTextScissor.count;
    bool usingScissor = (mScissorMask & uScissorMask_Text) >> 1;
    if (!usingScissor) { 
        textData = &mTextData[mNumChars];
    }
    else { // if text is scisorred
        textData = &mTextData[mTextScissor.count];
    }

    position *= mWindowRatio;
    Vector2f posStart = position;
    float spaceWidth = (float)mCurrentFontAtlas->characters['0'].width;
    float ascent = (float)mCurrentFontAtlas->characters['0'].height * 1.15f;
    float scale = uGetFloat(ufTextScale);
    scale *= mUIScale;
    half scalef16 = ConvertFloatToHalf(scale);
    uint color = uGetColor(uColorText);
    int currentLine = 0;

    const char* textEnd = text + txtSize;
    uint8 currentDepth = int(uGetFloat(ufDepth) * 255.0f);
    int numChars = 0;
    // fill per quad data
    while (text < textEnd)
    {
        if (*text == '\n') { // new line 'enter'
            if (!(flags & uTextFlags_NoNewLine))
            {
                currentLine++;
                position = posStart;
                position.y += float(currentLine) * ascent * scale;
            }

            text++;
            continue;
        }

        if (*text == '\t') { // tab
            float space = spaceWidth * scale * 0.5f * mWindowRatio.x;
            position.x += space * 4.0f;
            text++;
            continue;
        }
        
        if (*text == ' ') {
            float sizeX = position.x - posStart.x;
            if ((flags & uTextFlags_WrapWidthDetermined) && sizeX > uGetFloat(ufTextWrapWidth))
            {
                currentLine++;
                position = posStart;
                position.y += float(currentLine) * ascent * scale;
            }
            position.x += spaceWidth * scale * 0.5f * mWindowRatio.x;
            text++;
            continue;
        }

        if (usingScissor) {
            // move all of the non scissor data right by txtLen amount
            textData[numChars + numNonScissor] = textData[numChars];
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
        
        Vector2f pos = position;
        pos.x += float(character.xoff) * scale;
        pos.y += float(character.yoff) * scale;

        textData[numChars].posX = MakeFixed(pos.x);
        textData[numChars].posY = MakeFixed(pos.y);

        textData[numChars].size  = uint32_t(MakeFixed(size.x + 0.5f));
        textData[numChars].size |= uint32_t(MakeFixed(size.y + 0.5f)) << 16;

        textData[numChars].character = chr;  
        textData[numChars].depth = currentDepth;
        textData[numChars].scale = scalef16;
        textData[numChars].color = color;

        position.x += character.advence * scale;
        numChars++;
    }
    if (usingScissor) {
        mTextScissor.count += numChars;
        mTextScissor.rects[mTextScissor.numRect].numElements += numChars;
    }
    mNumChars += numChars; // we shouldn't count spaces or tabs
}

Vector2f uCalcTextSize(const char* text, uTextFlags flags)
{
    if (!text) return { 0.0f, 0.0f };
    
    float scale = uGetFloat(ufTextScale);
    float spaceWidth = (float)mCurrentFontAtlas->characters['0'].width;
    float ascent = (float)mCurrentFontAtlas->characters['0'].height * 1.15f;
    
    const char* textEnd = text + StringLength(text);
    Vector2f size = {0.0f, 0.0f};
    float maxX = 0.0f;

    short lastXoff, lastWidth;
    unsigned int chr;
    int currentLine = 0;

    while (*text)
    {
        if (*text == '\n') { // newline
            if (!(flags & uTextFlags_NoNewLine))
            {
                currentLine++;
                maxX = MAX(size.x, maxX);
                size.x = 0.0f;
                size.y += float(currentLine) * ascent * scale;
            }

            text++;
            continue;
        }

        if (*text == '\t') { // tab
            float space = spaceWidth * scale * 0.5f * mWindowRatio.x;
            size.x += space * 4.0f;
            text++;
            continue;
        }

        if (*text == ' ') {
            if ((flags & uTextFlags_WrapWidthDetermined) && size.x  > uGetFloat(ufTextWrapWidth))
            {
                currentLine++;
                currentLine++;
                maxX = MAX(size.x, maxX);
                size.x = 0.0f;
                size.y += float(currentLine) * ascent * scale;
            }
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
    size.x = MAX(size.x, maxX);
    // size.x += (lastWidth + lastXoff) * scale;
    return size;
}

int CalcTextNumLines(const char* text, uTextFlags flags)
{
    int numLines = 1;
    while (*text)
    {
        if (*text == '\n') {
            if (!(flags & uTextFlags_NoNewLine))
            {
                numLines++;
            }
        }
        // if (*text == ' ') {
        //     numLines += (flags & uTextFlags_WrapWidthDetermined) && size.x > uGetFloat(ufTextWrapWidth);
        // }
        // sizeX += character.advence * scale;
        text += UTF8CharLen(text);
    }
    return numLines;
}

//------------------------------------------------------------------------
// Quad Drawing
void uQuad(Vector2f position, Vector2f scale, uint color, uint properties)
{
    ASSERTR(mQuadIndex < MaxQuads, return);
    position *= mWindowRatio;
    QuadData* quadData = nullptr;

    if (!(mScissorMask & uScissorMask_Quad)) { 
        quadData = &mQuadData[mQuadIndex];
    }
    else {
        quadData = &mQuadData[mQuadScissor.count];
        mQuadData[mQuadIndex] = mQuadData[mQuadScissor.count++];
        mQuadScissor.rects[mQuadScissor.numRect].numElements++;
    }
    mQuadIndex++;

    quadData->posX = MakeFixed(position.x);
    quadData->posY = MakeFixed(position.y);

    quadData->size  = uint32_t(MakeFixed(scale.x));
    quadData->size |= uint32_t(MakeFixed(scale.y)) << 16;
    
    uint8 currentDepth = int(uGetFloat(ufDepth) * 255.0f);
    quadData->color = color;
    quadData->depth = currentDepth;
    quadData->effect = 0xFF & properties;
    quadData->cutStart = 0xFF & (properties >> 8);
}

bool uClickCheck(Vector2f pos, Vector2f scale, uClickOpt flags = 0)
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
    bool released = GetMouseReleased(MouseButton_Left);
    mWasHovered = PointBoxIntersection(scaledPos, scaledPos + scaledScale, mousePos);
    mAnyElementClicked |= mWasHovered && released;

    if (!!(flags & CheckOpt_WhileMouseDown) && 
        GetMouseDown(MouseButton_Left)) 
        return mWasHovered;

    return mWasHovered && released;
}

bool uClickCheckCircle(Vector2f pos, float radius, uClickOpt flags)
{
    Vector2f mousePos;
    GetMouseWindowPos(&mousePos.x, &mousePos.y);
    
    Vector2f scaledPos = pos * mWindowRatio;
    bool released = GetMouseReleased(MouseButton_Left) || GetMouseReleased(MouseButton_Right);
    mWasHovered = Vector2f::Distance(scaledPos, mousePos) < radius * mUIScale;
    mAnyElementClicked |= mWasHovered && released;

    if (!!(flags & CheckOpt_WhileMouseDown) && 
        GetMouseDown(MouseButton_Left)) 
        return mWasHovered;

    return mWasHovered && released;
}

float uToolTip(const char* text, float timeRemaining, bool wasHovered)
{
    if (!wasHovered) {
        timeRemaining = 1.0f; // tooltip will shown after one second
    }
    else { 
        timeRemaining -= (float)GetDeltaTime();
    }

    if (timeRemaining <= 0.02f)
    {
        uTextFlags textFlag = uTextFlags_None;
        Vector2f mousePos;
        int numLines = CalcTextNumLines(text, textFlag);
        GetMouseWindowPos(&mousePos.x, &mousePos.y);
        // we apply window ratio in functions but we shouldn't apply because we get the mouse position 
        mousePos /= mWindowRatio; 
        uPushFloat(ufDepth, uGetFloat(ufDepth) * 0.9f);
        uPushFloat(ufTextScale, uGetFloat(ufTextScale) * 0.5f);
        Vector2f textSize = uCalcTextSize(text, textFlag) * 1.15f;

        float lineHeight = textSize.y / (float)numLines;
        mousePos.y += lineHeight;
        uText(text, mousePos + (textSize * 0.05f), textFlag);
        
        mousePos.y -= lineHeight;
        textSize.y += lineHeight * 0.5f;
        uQuad(mousePos, textSize, uGetColor(uColorQuad), uButtonOpt_Border);
        uBorder(mousePos, textSize);

        uPopFloat(ufTextScale);
        uPopFloat(ufDepth);
    }
    return timeRemaining;
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
    bool pressed = entered || uClickCheck(pos, scale);
    if (pressed) PlayButtonClickSound();

    uint quadColor = uGetColor(uColorQuad);
    if (mWasHovered || !!(opt & uButtonOpt_Hovered))
        quadColor = mColors[uColorHovered];

    uQuad(pos, scale, quadColor, opt & 0xFF);
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

inline Vector2f uLabel(const char* label, Vector2f pos) {
    Vector2f labelSize;
    uPushFloat(ufTextScale, uGetFloat(ufTextScale) * 0.8f);
    if (label != nullptr) {
        labelSize = uCalcTextSize(label);
        uText(label, pos);
    } else {
        labelSize.y = uCalcTextSize("A").y;
        labelSize.x = labelSize.y;
    }
    uPopFloat(ufTextScale);
    return labelSize * 1.2f;
}

bool uCheckBox(const char* text, bool* isEnabled, Vector2f pos, bool cubeCheckMark)
{
    Vector2f textSize = uLabel(text, pos);
    float checkboxHeight = textSize.y;
    // detect box position
    float checkboxStart = uGetFloat(ufContentStart);
    if (checkboxStart < 0.01f) {
        const float boxPadding = 20.0f;
        pos.x += textSize.x + boxPadding;
    }
    else {
        pos.x += checkboxStart - checkboxHeight;
    }

    Vector2f boxScale = { checkboxHeight, checkboxHeight };
    pos.y -= boxScale.y - 4.0f;
    boxScale /= mWindowRatio;
    boxScale *= 0.80f;
    uQuad(pos, boxScale, uGetColor(uColorCheckboxBG));

    bool elementFocused = uGetElementFocused();
    uint borderColor = uGetColor(elementFocused ? uColorSelectedBorder : uColorBorder);
    uPushColor(uColorBorder, borderColor);
        uBorder(pos, boxScale);
    uPopColor(uColorBorder);
    
    bool enabled = *isEnabled;
    bool entered = elementFocused && GetKeyPressed(Key_ENTER);
    if (entered || uClickCheck(pos, boxScale, CheckOpt_BigColission)) {
        enabled = !enabled;
        PlayButtonClickSound();
    }

    if (enabled && !cubeCheckMark) {
        float scale = uGetFloat(ufTextScale);
        pos.y += boxScale.y - 4.0f;
        uPushFloat(ufTextScale, scale * 0.85f);
        uText(IC_CHECK_MARK, pos); // todo properly scale the checkmark
        uPopFloat(ufTextScale);
    }
    else if (enabled && cubeCheckMark)
    {
        Vector2f slightScale = boxScale * 0.17f;
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

void uLineVertical(Vector2f begin, float size, uint properties) {
    LineThicknesBorderColor data = GetLineData();
    uQuad(begin, MakeVec2(data.thickness, size), data.color, properties);
}

void uLineHorizontal(Vector2f begin, float size, uint properties) {
    LineThicknesBorderColor data = GetLineData();
    uQuad(begin, MakeVec2(size, data.thickness), data.color, properties);
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
    if (unicode == '.') {
        mDotPressed = true;
    }

    if (!mCurrText.Editing || GetKeyDown(Key_CONTROL) 
        || unicode == Key_ENTER // enter
        || unicode == Key_ESCAPE // escape
        || unicode == Key_TAB) /* tab */ return; 

    bool hasSpace = mCurrText.Pos < mCurrText.MaxLen;
    bool isBackspace = unicode == Key_BACK;

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

// maybe feature: TextBox cursor horizontal vertical movement (arrow keys)
// maybe feature: TextBox control shift fast move
// maybe feature: TextBox Shift Arrow select. (paste works copy doesnt)
// feature:       TextBox multiline text
bool uTextBox(const char* label, Vector2f pos, Vector2f size, char* text)
{
    Vector2f labelSize = uLabel(label, pos);

    if (size.x + size.y < Epsilon) // if size is not determined generate it
    {
        size.x = uGetFloat(ufTextBoxWidth);
        size.y = labelSize.y * 0.85f;
    }
    float contentStart = uGetFloat(ufContentStart);
    pos.x += contentStart - size.x;
    pos.y -= size.y;

    bool clicked = uClickCheck(pos, size);
    uQuad(pos, size, uGetColor(uColorTextBoxBG));

    bool elementFocused = uGetElementFocused();
    uint borderColor = uGetColor(elementFocused ? uColorSelectedBorder : uColorBorder);
    uPushColor(uColorBorder, borderColor);
        uBorder(pos, size);
    uPopColor(uColorBorder);
    
    uBeginScissor(pos, size, uScissorMask_Text); // clip the text inside of rectangle

    if (clicked) {
        wShowKeyboard(true); // < for android
    }

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
        float maxCharWidth = mCurrentFontAtlas->maxCharWidth * 0.5f;
        mCurrText.MaxLen = MIN(TextCapacity, int(size.x * mWindowRatio.x / maxCharWidth));
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

    uText(text, pos, uTextFlags_NoNewLine);
    
    uEndScissor(uScissorMask_Text);
    uPopFloat(ufTextScale);
    return clicked;
}

int uDropdown(const char* label, Vector2f pos, const char** names, int numNames, int current)
{
    Vector2f labelSize = uLabel(label, pos);
    Vector2f size;
    size.x = uGetFloat(ufTextBoxWidth);
    size.y = labelSize.y * 0.85f;

    float contentStart = uGetFloat(ufContentStart);
    pos.x += contentStart - size.x;
    pos.y -= size.y;

    bool clicked = uClickCheck(pos, size);
    bool elementFocused = uGetElementFocused();
    uint borderColor = uGetColor(elementFocused ? uColorSelectedBorder : uColorBorder);

    size_t id = (size_t)label;
    bool newlyOpenned = false;
    if (clicked && !mDropdownOpen) {
        mCurrentDropdown = id;
        newlyOpenned = true;
        mDropdownOpen = true;
    }

    float textScale = uGetFloat(ufTextScale);
    float offset = size.y * 0.1f;
    Vector2f textPos = pos;
    textPos.y += size.y - offset;
    textPos.x += offset;

    bool isCurrentDropdown = mCurrentDropdown == id;
    size.y *= 1.0f + (float(numNames) * (isCurrentDropdown && mDropdownOpen));
    
    uPushFloat(ufDepth, uGetFloat(ufDepth) * 0.9f);
    
    uQuad(pos, size, uGetColor(uColorTextBoxBG));

    uPushColor(uColorBorder, borderColor);
    uBorder(pos, size);
    uPopColor(uColorBorder);

    uBeginScissor(pos, size, uScissorMask_Text); // clip the text inside of rectangle
    uPushFloat(ufTextScale, uGetFloat(ufTextScale) * 0.7f);

    if (!(mDropdownOpen && isCurrentDropdown))
    {
        current = Clamp(current, 0, numNames - 1);
        uText(names[current], textPos, uTextFlags_NoNewLine);
    }
    else if (!newlyOpenned) // < if dropdown is newly clicked, it will select first element, to avoid it we should do this test
    {
        float elementHeight = size.y / (float)numNames;
        size.y = elementHeight;

        for (int i = 0; i < numNames; i++)
        {
            uText(names[i], textPos, uTextFlags_NoNewLine);
            clicked = uClickCheck(pos, size);
            
            if (mWasHovered) {
                uQuad(pos, size, uGetColor(uColorHovered));
                if (mLastHoveredDropdown != i) 
                    PlayButtonHoverSound(), mLastHoveredDropdown = i;
            }

            if (clicked) {
                current = i;
                mDropdownOpen = false;
                PlayButtonClickSound();
            }

            pos.y += elementHeight;
            textPos.y += elementHeight;
        }
    }

    uEndScissor(uScissorMask_Text);
    uPopFloat(ufTextScale);
    uPopFloat(ufDepth);
    return current;
}

int uChoice(const char* label, Vector2f pos, const char** names, int numNames, int current)
{   
    Vector2f labelSize = uLabel(label, pos);
    Vector2f size = { uGetFloat(ufTextBoxWidth), uGetFloat(ufSliderHeight) };

    float contentStart = uGetFloat(ufContentStart);
    pos.x += contentStart - size.x;

    Vector2f startPos = pos;
    // write centered text
    Vector2f nameSize  = uCalcTextSize(names[current]);
    Vector2f arrowSize = MakeVec2(mCurrentFontAtlas->maxCharWidth * 1.65f);
    float centerOffset = (size.x - nameSize.x) / 2.0f;
    
    pos.x += centerOffset;
    uPushFloat(ufTextScale, uGetFloat(ufTextScale) * 0.8f);
        uText(names[current], pos);
    uPopFloat(ufTextScale);
    pos.x -= centerOffset + arrowSize.x;

    bool elementFocused = uGetElementFocused();
    pos.y -= size.y;
    //pos.y -= size.y * 0.1f; // < move triangles little bit up
    pos.x += arrowSize.x * 0.41f;

    const uClickOpt chkOpt = CheckOpt_BigColission;
    bool goLeft = elementFocused && GetKeyPressed(Key_LEFT);
    if (uClickCheck(pos, arrowSize, chkOpt) || goLeft)
    {
        PlayButtonHoverSound();
        if (current > 0) current--;
        else current = numNames - 1;
    }

    uint iconColor = uGetColor(elementFocused ? uColorSelectedBorder : uColorText);
    uHorizontalTriangle(pos, arrowSize.x * 0.82f, -0.8f, iconColor);
    pos.x += size.x;
    pos.x += arrowSize.x;
    uHorizontalTriangle(pos, arrowSize.x * 0.82f, 0.8f, iconColor);
    
    bool goRight = elementFocused && GetKeyPressed(Key_RIGHT);
    if (uClickCheck(pos, arrowSize, chkOpt) || goRight)
    {
        PlayButtonHoverSound();
        if (current < numNames-1) current++;
        else current = 0;
    }
    return current;
}

bool uSlider(const char* label, Vector2f pos, float* val, float scale)
{
    Vector2f labelSize = uLabel(label, pos);
    Vector2f size = { scale, uGetFloat(ufSliderHeight) };
    
    float contentStart = uGetFloat(ufContentStart);
    pos.x += contentStart - size.x;
    pos.y -= size.y;

    bool elementFocused = uGetElementFocused();
    uint borderColor = uGetColor(elementFocused ? uColorSelectedBorder : uColorBorder);
    uPushColor(uColorBorder, borderColor);
    uBorder(pos, size);
    uPopColor(uColorBorder);

    bool edited = uClickCheck(pos, size, CheckOpt_WhileMouseDown);
    
    // fix: doesn't work with different scales
    if (edited && elementFocused) {
        Vector2f mousePos; GetMouseWindowPos(&mousePos.x, &mousePos.y);
        mousePos -= pos * mWindowRatio;
        mousePos = Max(mousePos, Vector2f::Zero());
        *val = Remap(mousePos.x, 0.0f, size.x * mWindowRatio.x, 0.0f, 1.0f);
    }

    if (elementFocused && GetKeyReleased(Key_LEFT))
        *val -= 0.1f,  edited = true, PlayButtonHoverSound();
    if (elementFocused && GetKeyReleased(Key_RIGHT)) 
        *val += 0.1f,  edited = true, PlayButtonHoverSound();

    if (*val < 0.01f) *val = 0.0f;

    *val = Clamp(*val, 0.0f, 1.0f);
    
    if (*val > 0.01f)
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

FieldRes uIntField(const char* label, Vector2f pos, int* val, int minVal, int maxVal, float dragSpeed)
{
    Vector2f labelSize = uLabel(label, pos);
    Vector2f size = { uGetFloat(ufFieldWidth), uGetFloat(ufSliderHeight) };
    size.y = labelSize.y; // maybe change: y scale using label height is weierd 

    float contentStart = uGetFloat(ufContentStart);
    pos.x += contentStart - size.x;
    pos.y -= size.y;

    bool clicked = uClickCheck(pos, size, CheckOpt_BigColission);
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
    
    float offset = size.y * 0.2f;
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

FieldRes uFloatField(const char* label, Vector2f pos, float* valPtr, float minVal, float maxVal, float dragSpeed)
{
    Vector2f labelSize = uLabel(label, pos);
    Vector2f size = { uGetFloat(ufFieldWidth), uGetFloat(ufSliderHeight) };
    size.y = labelSize.y; // maybe change: y scale using label height is weierd 

    float contentStart = uGetFloat(ufContentStart);
    pos.x += contentStart - size.x;
    pos.y -= size.y;

    bool clicked = uClickCheck(pos, size, CheckOpt_BigColission);
    uQuad(pos, size, uGetColor(uColorTextBoxBG));

    bool elementFocused = uGetElementFocused();
    uint borderColor = uGetColor(elementFocused ? uColorSelectedBorder : uColorBorder);
    uPushColor(uColorBorder, borderColor);
        uBorder(pos, size);
    uPopColor(uColorBorder);

    float value = *valPtr;
    bool changed = false;
    FieldRes result = FieldRes_Clicked * clicked;
    int numDigits = Log10(unsigned(Abs(value))) + 1;

    if (elementFocused)
    {
        mAnyFloatEdited = true;
        if (mEditedFloat != valPtr) 
            mLastFloatWriting = false, mFloatDigits = 3;
        
        mEditedFloat = valPtr;
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
            mLastFloatWriting = false, mFloatDigits = 3;
            changed = true;
            result |= FieldRes_Changed;
        }
        value += GetKeyReleased(Key_RIGHT) * dragSpeed;
        value -= GetKeyReleased(Key_LEFT) * dragSpeed;

        const int maxDigits = 10;
        int pressedNumber = GetPressedNumber();

        if (pressedNumber != -1 && numDigits < maxDigits) {
            // add pressed number at the end of the number
            mLastFloatWriting = true;
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
            mLastFloatWriting = true;
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
        *valPtr = value;
    }

    float offset = size.y * 0.20f;
    pos.y += size.y - offset;
    pos.x += offset;

    char valText[32] = {};
    if (mLastFloatWriting && elementFocused) 
    {
        if (mFloatDigits == 0) {
            int lastIdx = IntToString(valText, (int)value);
            if (mDotPressed) valText[lastIdx] = '.';
        } else {
            FloatToString(valText, value, mFloatDigits-1);
        }
    } 
    else {
        int afterpoint = MAX(4 - numDigits, 1); // number of digits after dot'.' for example 123.000
        FloatToString(valText, value, afterpoint);
    }
    
    float textScale = uGetFloat(ufTextScale);
    uPushFloat(ufTextScale, textScale * 0.7f);
        uText(valText, pos);
    uPopFloat(ufTextScale);
    return result;
}

bool uIntVecField(const char* label, Vector2f pos, int* vecPtr, int N, int* index, int minVal, int maxVal, float dragSpeed)
{
    AX_ASSUME(N <= 8);
    Vector2f labelSize = uLabel(label, pos);
    const float fieldWidth = uGetFloat(ufFieldWidth);
    const float padding = fieldWidth * 0.07f;
    // start it from left
    const int Nmin1 = N - 1;
    pos.x -= fieldWidth * Nmin1 + (padding * Nmin1);
    bool elementFocused = uGetElementFocused();

    int currentIndex = index != nullptr ? *index : 0;
    if (currentIndex > N - 1) currentIndex = 0;
    currentIndex += elementFocused && GetKeyPressed(Key_TAB);

    bool changed = 0;
    FieldRes fieldRes;

    for (int i = 0; i < N; i++)
    {
        bool fieldFocused = i == currentIndex && elementFocused;
        uSetElementFocused(fieldFocused);
        fieldRes = uIntField(nullptr, pos, vecPtr + i, minVal, maxVal, dragSpeed);
        if (!!(fieldRes & FieldRes_Clicked))
            currentIndex = i;
        changed |= fieldRes > 0;
        pos.x += fieldWidth + padding;
    }
    if (index != nullptr)
        *index = currentIndex;
    return changed;
}

bool uFloatVecField(const char* label, Vector2f pos, float* vecPtr, int N, int* index, float minVal, float maxVal, float dragSpeed)
{
    AX_ASSUME(N <= 8);
    Vector2f labelSize = uLabel(label, pos);
    const float fieldWidth = uGetFloat(ufFieldWidth);
    const float padding = fieldWidth * 0.07f;
    // start it from left
    const int Nmin1 = N - 1;
    pos.x -= fieldWidth * Nmin1 + (padding * Nmin1);

    bool elementFocused = uGetElementFocused();

    int currentIndex = index != nullptr ? *index : 0;
    if (currentIndex > N - 1) currentIndex = 0;
    currentIndex += elementFocused && GetKeyPressed(Key_TAB);

    bool changed = 0;
    FieldRes fieldRes;

    for (int i = 0; i < N; i++)
    {
        bool fieldFocused = i == currentIndex && elementFocused;
        uSetElementFocused(fieldFocused);
        fieldRes = uFloatField(nullptr, pos, vecPtr + i, minVal, maxVal, dragSpeed);
        if (!!(fieldRes & FieldRes_Clicked))
            currentIndex = i;
        changed |= fieldRes > 0;
        pos.x += fieldWidth + padding;
    }
    if (index != nullptr)
        *index = currentIndex;
    return changed;
}

bool uColorField(const char* label, Vector2f pos, uint* colorPtr)
{
    Vector2f labelSize = uLabel(label, pos);
    Vector2f size = { uGetFloat(ufFieldWidth), uGetFloat(ufSliderHeight) };
    size.y = labelSize.y; // maybe change: y scale using label height is weierd 

    float contentStart = uGetFloat(ufContentStart);
    pos.x += contentStart - size.x;
    pos.y -= size.y;

    bool clicked = uClickCheck(pos, size, CheckOpt_BigColission);
    uQuad(pos, size, *colorPtr);

    bool elementFocused = uGetElementFocused();
    uint borderColor = uGetColor(elementFocused ? uColorSelectedBorder : uColorBorder);
    uPushColor(uColorBorder, borderColor);
        uBorder(pos, size);
    uPopColor(uColorBorder);
    
    bool edited = false;
    clicked |= GetKeyPressed(Key_ENTER) & elementFocused;
    if (clicked && elementFocused)
    {
        PlayButtonClickSound();
        Vector3f colorf; 
        UnpackColor3Uint(*colorPtr, colorf.arr);
        mColorPick.hsv = RGBToHSV(colorf);
        mColorPick.isOpen ^= 1; // change the open value
    }
    
    if (GetKeyPressed(Key_ESCAPE) || GetKeyPressed(Key_TAB))
    {
        mColorPick.isOpen = false;
    }

    if (mColorPick.isOpen) {
        float lineThickness = uGetFloat(ufLineThickness) * 2.0f;
        mColorPick.pos = pos;
        mColorPick.size = MakeVec2(300.0f, 200.0f) * (1.0f + IsAndroid());
        mColorPick.pos.y -= mColorPick.size.y + lineThickness;
        mColorPick.pos.x += size.x + lineThickness; // field width

        uPushColor(uColorBorder, 0xFFFFFFFFu);
        uPushFloat(ufLineThickness, lineThickness);
        mColorPick.pos -= lineThickness * 0.9f;
            uBorder(mColorPick.pos, mColorPick.size + lineThickness);
        mColorPick.pos += lineThickness * 0.9f;
        uPopFloat(ufLineThickness);
        uPopColor(uColorBorder);
        
        Vector2f mousePos; 
        GetMouseWindowPos(&mousePos.x, &mousePos.y);

        // Warning:
        // width of the alpha selection area and hue selection area 
        // is %88 smaller than color field, that's why you are seeing 0.88f and 0.12f. 
        // if you change these you have to change in shader too, same values have been used in shader for visualization
        Vector2f alphaPos  = mColorPick.pos;
        Vector2f alphaSize = mColorPick.size;
        alphaPos.x  += alphaSize.x * 0.88f;
        alphaSize.x *= 0.12f;
        alphaSize.y *= 0.88f;

        if (uClickCheck(alphaPos, alphaSize, CheckOpt_WhileMouseDown))
        {
            edited = true;
            mousePos -= alphaPos * mWindowRatio; 
            mousePos = Max(mousePos, Vector2f::Zero());
            mColorPick.alpha = 1.0f - Remap(mousePos.y, 0.0f, alphaSize.y * mWindowRatio.y, 0.0f, 1.0f);
        }

        uPushFloat(ufDepth, uGetFloat(ufDepth) * 0.8f);
        uPushColor(uColorLine, 0xFF000000u);
            alphaPos.y += alphaSize.y * (1.0f-mColorPick.alpha);
            uLineHorizontal(alphaPos, alphaSize.x); // alpha black indicator line 
        uPopColor(uColorLine);
        uPopFloat(ufDepth);

        // sv = saturation, vibrance
        Vector2f svPosition = mColorPick.pos;
        Vector2f svSize = mColorPick.size;
        svSize *= 0.88f;

        if (uClickCheck(svPosition, svSize, CheckOpt_WhileMouseDown))
        {
            edited = true;
            mousePos -= svPosition * mWindowRatio; 
            mousePos = Max(mousePos, Vector2f::Zero());
            mColorPick.hsv.y = Remap(mousePos.x, 0.0f, svSize.x * mWindowRatio.x, 0.0f, 1.0f);
            mColorPick.hsv.z = 1.0f - Remap(mousePos.y, 0.0f, svSize.y * mWindowRatio.y, 0.0f, 1.0f);
        }

        // hue control is at the bottom of the color picker, its height is colorpick.size.y * 0.22
        Vector2f huePosition = mColorPick.pos;
        Vector2f hueSize = mColorPick.size;
        hueSize.y *= 0.12f;
        huePosition.y += mColorPick.size.y - hueSize.y; // start of hue select

        if (uClickCheck(huePosition, hueSize, CheckOpt_WhileMouseDown))
        {
            edited = true;
            mousePos -= huePosition * mWindowRatio;
            mousePos = Max(mousePos, Vector2f::Zero());
            mColorPick.hsv.x = Remap(mousePos.x, 0.0f, hueSize.x * mWindowRatio.x, 0.0f, 1.0f);
        }

        float dt = (float)GetDeltaTime();
        bool left = GetKeyDown(Key_LEFT), right = GetKeyDown(Key_RIGHT);
        const float dragSpeed = 0.5f;
        mColorPick.hsv.x -= left * dt * dragSpeed;
        mColorPick.hsv.x += right * dt * dragSpeed;
        mColorPick.hsv.x = Clamp01(mColorPick.hsv.x);
        edited |= left | right;

        if (edited) {
            float color[4];
            HSVToRGB(mColorPick.hsv, color);
            color[3] = mColorPick.alpha;
            *colorPtr = PackColor4PtrToUint(color);
        }

        huePosition.x += hueSize.x * mColorPick.hsv.x;

        uPushFloat(ufDepth, uGetFloat(ufDepth) * 0.8f);
        uPushColor(uColorLine, 0xFF000000u);
            uLineVertical(huePosition, hueSize.y); // hue black indicator line 
        uPopColor(uColorLine);
        uPopFloat(ufDepth);
    }

    return clicked || edited;
}

bool uColorField3(const char* label, Vector2f pos, float* colorPtr)
{
    uint ucolor = PackColor3PtrToUint(colorPtr);
    bool res = uColorField(label, pos, &ucolor);
    UnpackColor3Uint(ucolor, colorPtr);
    return res;
}

bool uColorField4(const char* label, Vector2f pos, float* colorPtr)
{
    uint ucolor = PackColor4PtrToUint(colorPtr);
    bool res = uColorField(label, pos, &ucolor);
    UnpackColor4Uint(ucolor, colorPtr);
    return res;
}

void uSprite(Vector2f pos, Vector2f scale, Texture* texturePtr)
{
    ASSERTR(mNumSprites < MaxSprites, return);
    mSprites[mNumSprites].pos    = pos;
    mSprites[mNumSprites].scale  = scale;
    mSprites[mNumSprites].handle = texturePtr->handle;
    mNumSprites++;
}

//------------------------------------------------------------------------
// Triangle Tendering
void uVertex(Vector2f pos, uint8 fade, uint color, uint32 properties)
{
    TriangleVert& vert = mTriangles[mCurrentTriangle++];
    pos *= mWindowRatio; // multiply on gpu?
    vert.posX   = MakeFixed(pos.x); 
    vert.posY   = MakeFixed(pos.y); 
    vert.fade   = fade;
    vert.depth  = uint8(uGetFloat(ufDepth) * 255.0f);
    vert.cutStart = 0xFF & (properties >> 8);
    vert.effect = 0xFF & properties;
    vert.color  = color;
}

void uHorizontalTriangle(Vector2f pos, float size, float axis, uint color)
{
    if (axis > 0.0f) {
        uVertex(pos, 255, color); pos.y += size;
        uVertex(pos, 255, color); pos.y -= size * 0.5f; pos.x += axis * size;
        uVertex(pos, 255, color);
    }
    else {
        pos.y += size;
        uVertex(pos, 255, color); 
        pos.y -= size;
        uVertex(pos, 255, color);
        pos.y += size * 0.5f; pos.x += axis * size;
        uVertex(pos,  255, color);        
    }
}

void uVerticalTriangle(Vector2f pos, float size, float axis, uint color)
{
    if (axis > 0.0f) {
        uVertex(pos, 255, color); pos.x += size;
        uVertex(pos, 255, color); pos.x -= size * 0.5f; pos.y += axis * size;
        uVertex(pos, 255, color);
    }
    else {
        pos.x += size;
        uVertex(pos, 255, color); 
        pos.x -= size;
        uVertex(pos, 255, color);
        pos.x += size * 0.5f; pos.y += axis * size;
        uVertex(pos, 255, color);
    }
}

void uCircle(Vector2f center, float radius, uint color, uint32 properties)
{
    uint numSegments = 0xFFu & (properties >> 16); // skip TriEffect and CutStart bytes
     if (numSegments != 0)
        numSegments *= 2;
    else
        numSegments = (int)(10.0f / (radius / 30.0f)) * 2; // < auto detect

    Vector2f posPrev  = { 0.0f, -1.0f};
    posPrev *= mWindowRatio; // < correct aspect ratio
    posPrev *= radius;
    posPrev += center;

    uint8 hasInvertFade = 255 * !!(properties & uFadeInvertBit); // if has invert this is 255 otherwise 0
    uint8    fadePrev = hasInvertFade;

    for (float i = 1.0f; i < (float)numSegments + 1.0f; i += 1.0f)
    {
        float t = TwoPI * (i / float(numSegments));
        Vector2f posNew = { -Sin(t), -Cos(t) };
        posNew *= mWindowRatio; // < correct aspect ratio

        posNew *= radius;
        posNew += center;
        
        if (properties & uEmptyInsideBit) {
            uVertex(center ,  hasInvertFade, color, properties);
            uVertex(posPrev, ~hasInvertFade, color, properties);
            uVertex(posNew , ~hasInvertFade, color, properties);
        }
        else
        {
            uint8 fadeNew = uint8((t / TwoPI) * 255.0f);
            fadeNew = hasInvertFade ? 255-fadeNew : fadeNew;
            uVertex(center , fadeNew, color, properties);
            uVertex(posPrev, fadePrev, color, properties);
            uVertex(posNew , fadeNew, color, properties);
            fadePrev = fadeNew;
        }
        posPrev = posNew;
    }
}

void uCapsule(Vector2f center, float radius, float width, uint color, uint32 properties)
{
    uint numSegments = 0xFFu & (properties >> 16); // skip TriEffect and CutStart bytes
    if (numSegments == 0) 
        numSegments = (uint8)(10.0f / (radius / 30.0f));

    center.x += radius; // go right for left half of the circle
    width -= radius * 2.0f; // we have to reduce the width because we are adding 2 radius from left and right
    Vector2f p0 = { center.x, center.y - radius};
    uint8 hasInvertFade = 255 * !!(properties & uFadeInvertBit); // if has invert this is 255 otherwise 0
    // left half
    for (float i = 1.0f; i < (float)numSegments + 1.0; i += 1.0f)
    {
        float t = PI * (i / float(numSegments));
        Vector2f p1 = { -Sin0pi(t) * radius, -Cos0pi(t) * radius};
        p1 += center;
        uVertex(center, hasInvertFade, color, properties);
        uVertex(p0, hasInvertFade, color, properties);
        uVertex(p1, hasInvertFade, color, properties);
        p0 = p1;
    }
    uint hasInvert = properties & uFadeInvertBit;
    properties ^= hasInvert; // invert hasInvert bit because triangleQuad using it too
    // draw connecting quad
    uQuad(center + MakeVec2(0.0f, -radius), MakeVec2(width, radius * 2.0f), color, properties);
    properties |= hasInvert; // replace the invert bit

    center.x += width;
    p0  = { center.x, center.y + radius};
    hasInvertFade = ~hasInvertFade;
    // right half
    for (float i = 1.0f; i < (float)numSegments + 1.0; i += 1.0f)
    {
        float t = PI * (i / float(numSegments));
        Vector2f p1 = { Sin0pi(t) * radius, Cos0pi(t) * radius };
        p1 += center;
        uVertex(center, hasInvertFade, color, properties);
        uVertex(p0, hasInvertFade, color, properties);
        uVertex(p1, hasInvertFade, color, properties);
        p0 = p1;
    }   
}

void uRoundedRectangle(Vector2f pos, float width, float height, uint color, uint properties)
{
    const int numSegments = 8;
    const float roundRatio = 0.15f, invRoundRatio = 1.0f - roundRatio;
    float radius = MIN(width, height) * roundRatio;
    // width *= invRoundRatio;
    height *= invRoundRatio;

    uint8 hasInvertFade = 255 * !!(properties & uFadeInvertBit); // if has invert this is 255 otherwise 0
    uint8      fadePrev = hasInvertFade;
    Vector2f center = pos + MakeVec2(width * 0.5f + radius * 0.5f, height * 0.5f + radius * 0.5f);
    Vector2f triPos = {pos.x, pos.y + radius};

    uVertex(center, hasInvertFade, color, properties); // left triangle
    uVertex(triPos, ~hasInvertFade, color, properties);
    triPos.y += height * invRoundRatio;
    uVertex(triPos, ~hasInvertFade, color, properties);

    triPos += radius;
    uVertex(center, hasInvertFade, color, properties); // bottom triangle
    uVertex(triPos, ~hasInvertFade, color, properties);
    triPos.x += width * invRoundRatio;
    uVertex(triPos, ~hasInvertFade, color, properties);

    triPos.y -= radius;
    triPos.x += radius;
    uVertex(center, hasInvertFade, color, properties); // right triangle
    uVertex(triPos, ~hasInvertFade, color, properties);
    triPos.y -= height * invRoundRatio ;
    uVertex(triPos, ~hasInvertFade, color, properties);
    
    triPos -= radius;
    uVertex(center, hasInvertFade, color, properties); // up triangle
    uVertex(triPos, ~hasInvertFade, color, properties);
    triPos.x -= width * invRoundRatio;
    uVertex(triPos, ~hasInvertFade, color, properties);
    
    Vector2f samplePos  = { pos.x + radius, pos.y + radius };
    Vector2f posPrev = { samplePos.x, samplePos.y - radius};

    for (float i = 1.0f; i < (float)numSegments + 1.0f; i += 1.0f)
    {
        float t = HalfPI * (i / float(numSegments));
        Vector2f posNew = { -Sin0pi(t*1.001f), -Cos0pi(t*1.001f) };
        uint8 fadeNew = uint8((t / TwoPI) * 255.0f);
        posNew *= radius;
        posNew += samplePos;
        uVertex(center ,  hasInvertFade, color, properties);
        uVertex(posPrev, ~hasInvertFade, color, properties);
        uVertex(posNew , ~hasInvertFade, color, properties);
        posPrev = posNew;
        fadePrev = fadeNew;
    }
    samplePos.y += height * invRoundRatio;
    posPrev = { samplePos.x - radius, samplePos.y };
    for (float i = 1.0f; i < (float)numSegments + 1.0f; i += 1.0f)
    {
        float t = HalfPI * (i / float(numSegments));
        Vector2f posNew = { -Sin0pi(HalfPI + t), -Cos0pi(HalfPI + t) };
        uint8 fadeNew = uint8((t / TwoPI) * 255.0f);
        posNew *= radius;
        posNew += samplePos;
        uVertex(center,  hasInvertFade, color, properties);
        uVertex(posPrev, ~hasInvertFade, color, properties);
        uVertex(posNew , ~hasInvertFade, color, properties);
        posPrev = posNew;
        fadePrev = fadeNew;
    }
    samplePos.x += width * invRoundRatio;
    posPrev = { samplePos.x, samplePos.y + radius };
    for (float i = 1.0f; i < (float)numSegments + 1.0f; i += 1.0f)
    {
        float t = HalfPI * (i / float(numSegments));
        Vector2f posNew = { -Sin(PI + t * 1.001f), -Cos(PI + t* 1.001f) };
        uint8 fadeNew = uint8((t / TwoPI) * 255.0f);
        posNew *= radius;
        posNew += samplePos;
        uVertex(center,  hasInvertFade, color, properties);
        uVertex(posPrev, ~hasInvertFade, color, properties);
        uVertex(posNew , ~hasInvertFade, color, properties);
        posPrev = posNew;
        fadePrev = fadeNew;
    }
    samplePos.y -= height * invRoundRatio;
    posPrev = { samplePos.x + radius, samplePos.y };
    for (float i = 1.0f; i < (float)numSegments + 1.0f; i += 1.0f)
    {
        float t = HalfPI * (i / float(numSegments));
        Vector2f posNew = { -Sin(PI + HalfPI + t), -Cos(PI + HalfPI + t) };
        uint8 fadeNew = uint8((t / TwoPI) * 255.0f);
        posNew *= radius;
        posNew += samplePos;
        uVertex(center,  hasInvertFade, color, properties);
        uVertex(posPrev, ~hasInvertFade, color, properties);
        uVertex(posNew , ~hasInvertFade, color, properties);
        posPrev = posNew;
        fadePrev = fadeNew;
    }
}

void uTriangle(Vector2f pos0, Vector2f pos1, Vector2f pos2, uint color)
{
    uVertex(pos0, 255, color);
    uVertex(pos1, 255, color);
    uVertex(pos2, 255, color);
}

//------------------------------------------------------------------------
// Rendering

void uBeginScissor(Vector2f pos, Vector2f scale, uScissorMask mask)
{
    ASSERTR(mQuadScissor.numRect < MaxScissor, return);
    Vector2i windowSize;
    wGetWindowSize(&windowSize.x, &windowSize.y);
    pos *= mWindowRatio;
    scale *= mWindowRatio;
    pos.y = (float)windowSize.y - (pos.y + scale.y); // convert to opengl coordinates

    mScissorMask = mask;
    if (mask & uScissorMask_Quad) {
        mQuadScissor.PushNewRect(pos, scale);
    }

    if (mask & uScissorMask_Text) {
        mTextScissor.PushNewRect(pos, scale);
    }
}

void uEndScissor(uScissorMask mask)
{
    mQuadScissor.numRect += mask & uScissorMask_Quad;
    mTextScissor.numRect += (mask & uScissorMask_Text) >> 1;
    mScissorMask &= ~mask;
}

static void uRenderTriangles(Vector2i windowSize)
{
    if (mCurrentTriangle <= 0) return;

    static int scrSizeLoc = INT32_MAX;
    rBindShader(mTriangleShader);
    if (scrSizeLoc == INT32_MAX) {
        scrSizeLoc = rGetUniformLocation(mTriangleShader, "uScrSize"); // to calculate projection matrix
    }
    
    rBindMesh(mTriangleMesh);
    rUpdateMesh(&mTriangleMesh, mTriangles.Data(), mCurrentTriangle * sizeof(TriangleVert));
    rSetShaderValue(&windowSize.x, scrSizeLoc, GraphicType_Vector2i);
    rRenderMesh(mCurrentTriangle);
    mCurrentTriangle = 0;   
}

static void RenderScissored(ScissorData& scissorData, int indexStartLoc)
{
    rScissorToggle(true);
    int numScisorred = 0;
    for (int i = 0; i < scissorData.numRect; i++)
    {
        ScissorRect scissorRect = scissorData.rects[i];
        if (scissorRect.numElements == 0) continue;
        rScissor((int)scissorRect.position.x, (int)scissorRect.position.y, scissorRect.sizeX, (int)scissorRect.sizeY);
        rSetShaderValue(numScisorred * 6, indexStartLoc);
        rRenderMeshNoVertex(6 * scissorRect.numElements); // 6 index for each char
        numScisorred += scissorRect.numElements;
    }
    rSetShaderValue(numScisorred, indexStartLoc);
    rScissorToggle(false);
}

static void uRenderQuads(Vector2i windowSize)
{
    if (mQuadIndex + mQuadScissor.count <= 0) return;
    rBindShader(mQuadShader);

    rUpdateTexture(mQuadDataTex, mQuadData);
    rSetTexture(mQuadDataTex, 0, dataTexLocQuad);
    
    rSetShaderValue(&mWindowRatio.x, uScaleLocQuad, GraphicType_Vector2f);
    rSetShaderValue(&windowSize.x, uScrSizeLocQuad, GraphicType_Vector2i);
    rSetShaderValue(0, uIndexStartLocQuad);
    
    if (mQuadScissor.count) {
        RenderScissored(mQuadScissor, uIndexStartLocQuad);
        rSetShaderValue(mQuadScissor.count * 6, uIndexStartLocQuad);
    }

    int numNonScissorred = mQuadIndex - mQuadScissor.count;
    rRenderMeshNoVertex(6 * numNonScissorred); // 6 index for each char
    mQuadIndex = 0;
}

static void uRenderTexts(Vector2i windowSize)
{
    if (mNumChars + mTextScissor.count <= 0) return;

    rBindShader(mFontShader);

    rUpdateTexture(mTextDataTex, mTextData);
    rSetTexture(mTextDataTex, 0, dataTexLoc);
    rSetTexture(mCurrentFontAtlas->textureHandle, 1, atlasLoc);

    rSetShaderValue(&windowSize.x, uScrSizeLoc, GraphicType_Vector2i);
    rSetShaderValue(0, uIndexStartLocText);

    if (mTextScissor.count) {
        RenderScissored(mTextScissor, uIndexStartLocText);
        rSetShaderValue(mTextScissor.count * 6, uIndexStartLocText);
    }
    
    int numNonScissorred = mNumChars - mTextScissor.count;
    rRenderMeshNoVertex(6 * numNonScissorred); // 6 index for each char
    mNumChars = 0;
}

static void uRenderColorPicker(Vector2i windowSize)
{
    if (!mColorPick.isOpen) return;

    static bool first = true;
    static int hueLoc, sizeLoc, posLoc, scrSizeLoc;
    
    rBindShader(mColorPick.shader);

    if (first) {
        first = false;
        hueLoc = rGetUniformLocation("uHSV");
        posLoc = rGetUniformLocation("uPos");
        sizeLoc = rGetUniformLocation("uSize");
        scrSizeLoc = rGetUniformLocation("uScrSize");
    }
    
    Vector2f size = mColorPick.size * mWindowRatio;
    Vector2f pos  = mColorPick.pos * mWindowRatio;
    rSetShaderValue(mColorPick.hsv.arr , hueLoc    , GraphicType_Vector3f);
    rSetShaderValue(pos.arr            , posLoc    , GraphicType_Vector2f);
    rSetShaderValue(size.arr           , sizeLoc   , GraphicType_Vector2f);
    rSetShaderValue(windowSize.arr     , scrSizeLoc, GraphicType_Vector2i);

    rRenderMeshNoVertex(6);
}

static void uRenderSprites(Vector2i windowSize)
{
    if (mNumSprites <= 0) return;

    static bool first = true;
    static int texLoc, sizeLoc, posLoc, scrSizeLoc;
    rBindShader(mTextureDrawShader);

    if (first) {
        first = false;
        texLoc = rGetUniformLocation("tex");
        posLoc = rGetUniformLocation("uPos");
        sizeLoc = rGetUniformLocation("uSize");
        scrSizeLoc = rGetUniformLocation("uScrSize");
    }
    
    rSetShaderValue(windowSize.arr, scrSizeLoc, GraphicType_Vector2i);
    
    for (int i = 0; i < mNumSprites; i++)
    {
        Vector2f size = mSprites[i].scale * mWindowRatio;
        Vector2f pos  = mSprites[i].pos * mWindowRatio;
        rSetShaderValue(pos.arr , posLoc , GraphicType_Vector2f);
        rSetShaderValue(size.arr, sizeLoc, GraphicType_Vector2f);
        rSetTexture(mSprites[i].handle, 0, texLoc);
        rRenderMeshNoVertex(6);
    }
    mNumSprites = 0;
}

void uBegin()
{
    mAnyTextEdited = false;
    mAnyFloatEdited = false;
    mAnyElementClicked = false;
    mQuadScissor.Reset();
    mTextScissor.Reset();
}

void uRender()
{
    // prepare
    rSetBlending(true);
    rSetBlendingFunction(rBlendFunc_Alpha, rBlendFunc_OneMinusAlpha);

    rUnpackAlignment(4);
    rClearDepth(); // we don't care about depth before UI

    Vector2i windowSize;
    wGetWindowSize(&windowSize.x, &windowSize.y);
    uRenderQuads(windowSize);
    uRenderTexts(windowSize);
    uRenderColorPicker(windowSize);
    uRenderSprites(windowSize);
    uRenderTriangles(windowSize);

    if (!mAnyTextEdited) 
        mCurrText.Editing = false;

    if (!mAnyFloatEdited)
        mLastFloatWriting = false, mFloatDigits = 3;

    bool released = GetMouseReleased(1);
    if (mColorPick.isOpen && released && !uClickCheck(mColorPick.pos, mColorPick.size, CheckOpt_BigColission))
    {
        mColorPick.isOpen = false;
    }

    if (released && !mAnyElementClicked) // mouse is released at empty space
    {
        mCurrText.Editing = false;
        mLastFloatWriting = false, mFloatDigits = 3;
    }

    rSetBlending(false);
    GetMouseWindowPos(&mMouseOld.x, &mMouseOld.y);
}

void uDestroy()
{
    if (!mInitialized) 
        return;

    rDeleteShader(mFontShader);
    rDeleteShader(mQuadShader);
    rDeleteShader(mColorPick.shader);
    rDeleteShader(mTextureDrawShader);

    for (int i = 0; i < mNumFontAtlas; i++) {
        Texture fakeTex;
        fakeTex.handle = mFontAtlases[i].textureHandle;
        rDeleteTexture(fakeTex);
    }

    rDeleteTexture(mTextDataTex);
    rDeleteTexture(mQuadDataTex);

    SoundDestroy(mButtonClickSound);
    SoundDestroy(mButtonHoverSound);
}
