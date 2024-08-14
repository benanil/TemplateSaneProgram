
// The performance is vastly improved by grouping all pixels that share the same direction values. 
// This means the screen-space linear depth buffer is stored in 16 layers each representing one direction of the 4x4 texture. 
// Each layer has a quarter of the original resolution. 
// The total amount of pixels is not reduced, 
// but the sampling is performed in equal directions for the entire layer, yielding better texture cache utilization.
// Linearizing the depth-buffer now stores into 16 texture layers.
// The actual HBAO effect is performed in each layer individually,
// however all layers are independent of each other, allowing them to be processed in parallel.
// Finally the results are stored scattered to their original locations in screen-space.
// Compared to the regular HBAO approach, the efficiency gains allow using the effect on full-resolution, improving the image quality.

// https://github.com/NVIDIAGameWorks/HBAOPlus
// https://github.com/nvpro-samples/gl_ssao

#include "include/Renderer.hpp"
#include "include/UI.hpp"
#include "include/Camera.hpp"

#include "../ASTL/Random.hpp"

struct HBAOData
{
    float RadiusToScreen; // radius
    float R2;             // 1.0 / radius
    float NegInvR2;       // radius * radius
    float NDotVBias;
          
    Vector2f  InvFullResolution;
    Vector2f  InvQuarterResolution;
    xyzw      projInfo;

    float AOMultiplier;
    float PowExponent;
    Vector2f Offset; // float(i % 4) + 0.5f, float(i / 4) + 0.5f
};

static HBAOData mHBAOData;

#define AO_RANDOMTEX_SIZE 4
static const int NUM_MRT         = 8;
static const int RANDOM_SIZE     = AO_RANDOMTEX_SIZE;
static const int RANDOM_ELEMENTS = RANDOM_SIZE * RANDOM_SIZE;
static const int MAX_SAMPLES     = 8;

static Shader mLinearizeDepthSH;
static Shader mReinterleaveSH;
static Shader mDeinterleaveSH;
static Shader mHBAOSH;
static Shader mBlurShader;
static Shader mWhiteShader;

static FrameBuffer mLinearDepthFB;
static FrameBuffer mDeinterleaveFB;
static FrameBuffer mHBAOProcessFB;
static FrameBuffer mHBAOResultFB;
static FrameBuffer mBlurFB;

static Texture mLinearDepthTX;

Texture mBlurResultTX;
static Texture mHBAOResultTX;

static Texture mDepth2D;
static Texture mDepthViews[RANDOM_ELEMENTS];

static Texture mResult2D;

static xyzw mHBAORandom[RANDOM_ELEMENTS * MAX_SAMPLES];

static bool mInitialized = false;

static int mWidth, mHeight;
static int mQuarterWidth, mQuarterHeight;

// Adjustables
static float mIntensity          = 2.0f;
static float mBias               = 0.1f;
static float mRadius             = 2.14f;
static float mBlurSharpness      = 30.0f;
static float mMattersToViewSpace = 1.2f;
static bool  mIsOpen = true;

// Uniform Locations
static int uHBAODataLoc;
static int uTexLinearDepthLoc;
static int uTexViewNormalLoc;
static int uViewLoc;
static int uJitterLoc;

Texture* HBAOGetResult()
{
    return &mBlurResultTX;
}

static void InitRandom()
{
    using namespace Random;

    const float numDir = 8; // keep in sync to glsl
    uint64_t xoro[2];
    Xoroshiro128PlusInit(xoro);

    for(int i = 0; i < RANDOM_ELEMENTS * MAX_SAMPLES; i++)
    {
        float Rand1 = (float)NextDouble01(Xoroshiro128Plus(xoro));
        float Rand2 = (float)NextDouble01(Xoroshiro128Plus(xoro));

        // Use random rotation angles in [0,2PI/NUM_DIRECTIONS)
        float Angle      = TwoPI * Rand1 / numDir;
        mHBAORandom[i].x = Sin0pi(Angle);
        mHBAORandom[i].y = Cos0pi(Angle);
        mHBAORandom[i].z = Rand2;
        mHBAORandom[i].w = 0;
    }
}

static void DeleteTextures()
{
    rDeleteTexture(mLinearDepthTX);
    rDeleteTexture(mHBAOResultTX);
    rDeleteTexture(mDepth2D);
    rDeleteTexture(mResult2D);
    rDeleteTexture(mBlurResultTX);
}

static void CreateFrameBuffers()
{
    mLinearDepthFB  = rCreateFrameBuffer();
    mHBAOProcessFB  = rCreateFrameBuffer();
    mHBAOResultFB   = rCreateFrameBuffer();
    mBlurFB         = rCreateFrameBuffer();
    mDeinterleaveFB = rCreateFrameBuffer(true);
    rFrameBufferSetNumColorBuffers(NUM_MRT);
}

static void InitFrameBuffers(int width, int height)
{
    mWidth = width; 
    mHeight = height;
    mQuarterWidth  = (width  + 3) / 4;
    mQuarterHeight = (height + 3) / 4;
    if (!mInitialized) {
        CreateFrameBuffers();
    }
    else {
        DeleteTextures();
    }

    mLinearDepthTX = rCreateTexture(width, height, nullptr, TextureType_R32F, TexFlags_RawData);
    mHBAOResultTX  = rCreateTexture(width, height, nullptr, TextureType_R8  , TexFlags_RawData);
    mBlurResultTX  = rCreateTexture(width, height, nullptr, TextureType_R8  , TexFlags_RawData);

    rBindFrameBuffer(mLinearDepthFB);
    rFrameBufferAttachColor(mLinearDepthTX, 0);
    rFrameBufferCheck();

    rBindFrameBuffer(mHBAOResultFB);
    rFrameBufferAttachColor(mHBAOResultTX, 0);
    rFrameBufferCheck();

    rBindFrameBuffer(mBlurFB);
    rFrameBufferAttachColor(mBlurResultTX, 0);
    rFrameBufferCheck();

    mDepth2D  = rCreateTexture2DArray(mDepthViews, mQuarterWidth, mQuarterHeight, RANDOM_ELEMENTS, nullptr, TextureType_R32F, TexFlags_RawData);
    mResult2D = rCreateTexture2DArray(nullptr, mQuarterWidth, mQuarterHeight, RANDOM_ELEMENTS, nullptr, TextureType_R8, TexFlags_RawData);
}

void HBAOInit(int width, int height)
{
    // Init shaders
    mLinearizeDepthSH  = rImportFullScreenShader("Shaders/LinearizeDepth.glsl");
    mReinterleaveSH    = rImportFullScreenShader("Shaders/HBAOReinterleave.glsl");
    mDeinterleaveSH    = rImportFullScreenShader("Shaders/HBAODeinterleave.glsl");
    mHBAOSH            = rImportFullScreenShader("Shaders/HBAO.glsl");
    mBlurShader        = rImportFullScreenShader("Shaders/HBAOBlur.glsl"); 

    mWhiteShader = rCreateFullScreenShader(
        AX_SHADER_VERSION_PRECISION()
        "layout(location = 0) out float result;"
        "void main() { result = 1.0; }"
    );

    uHBAODataLoc       = rGetUniformLocation(mHBAOSH, "uData");
    uTexLinearDepthLoc = rGetUniformLocation(mHBAOSH, "uTexLinearDepth");
    uTexViewNormalLoc  = rGetUniformLocation(mHBAOSH, "uTexNormal");
    uViewLoc           = rGetUniformLocation(mHBAOSH, "uView");
    uJitterLoc         = rGetUniformLocation(mHBAOSH, "uJitter");

    InitRandom();
    InitFrameBuffers(width, height);

    mInitialized = true;
}

void HBAOResize(int width, int height)
{
    if (!mInitialized || width < 128 || height < 128) return;
    InitFrameBuffers(width, height);
}

void SetHBAOData(float fov)
{
    float R = mRadius * mMattersToViewSpace;
    float tanHalfFovY = Tan(fov * RadToDeg * 0.5f);
    float projScale = float(mHeight) / (tanHalfFovY * 2.0f);
    
    mHBAOData.R2             = R * R;
    mHBAOData.NegInvR2       = -1.0f / mHBAOData.R2;
    mHBAOData.RadiusToScreen = R * 0.5f * projScale;
    
    mHBAOData.PowExponent    = mIntensity;
    mHBAOData.NDotVBias      = mBias;
    mHBAOData.AOMultiplier   = 1.0f / (1.0f - mHBAOData.NDotVBias);

    mHBAOData.InvQuarterResolution = { 1.0f / float(mQuarterWidth), 1.0f / float(mQuarterHeight) };
    mHBAOData.InvFullResolution    = { 1.0f / float(mWidth)       , 1.0f / float(mHeight)        };
    
    float aspect = float(mWidth) / float(mHeight);
    float pa = 1.0f / (aspect * tanHalfFovY);
    float pb = 1.0f / tanHalfFovY;

    mHBAOData.projInfo.x =  2.0f / pa; // (x) * (R - L)/N
    mHBAOData.projInfo.y =  2.0f / pb; // (y) * (T - B)/N
    mHBAOData.projInfo.z = -1.0f / pa; // L/N
    mHBAOData.projInfo.w = -1.0f / pb; // B/N
}

static void Linearize(Texture* depthTex, float near, float far)
{
    rSetViewportSize(mWidth, mHeight);
    rBindFrameBuffer(mLinearDepthFB);
    rBindShader(mLinearizeDepthSH);
    Vector3f clipInfo = { near * far, far - near, far };
    rSetShaderValue(clipInfo.arr, 0, GraphicType_Vector3f);
    rSetTexture(depthTex->handle, 0, 1);
    rRenderFullScreen();
}

// generates view space normal from depth
static void ReconstructNormal(float* projInfo)
{
    // rBindFrameBuffer(mNormalFB);
    // rBindShader(mExportNormalSH);
    rSetShaderValue(projInfo, rGetUniformLocation("projInfo"), GraphicType_Vector4f);
    rSetShaderValue(mHBAOData.InvFullResolution.arr, rGetUniformLocation("InvFullResolution"), GraphicType_Vector2f);
    rSetTexture(mLinearDepthTX, 0, rGetUniformLocation("texLinearDepth"));
    rRenderFullScreen();
}

static void Deinterleave()
{
    rSetViewportSize(mQuarterWidth, mQuarterHeight);
    rBindFrameBuffer(mDeinterleaveFB);
    rBindShader(mDeinterleaveSH);
    rSetTexture(mLinearDepthTX, 0, 1);

    for(int i = 0; i < RANDOM_ELEMENTS; i += NUM_MRT)
    {
        xyzw info = {
            float(i & 3) + 0.5f,
            float(i / 4) + 0.5f, 
            mHBAOData.InvFullResolution.x,
            mHBAOData.InvFullResolution.y 
        };
        rSetShaderValue(&info.x, 0, GraphicType_Vector4f);

        for(int layer = 0; layer < NUM_MRT; layer++)
        {
            rFrameBufferAttachColor(mDepthViews[i + layer], layer);
        }
        rRenderFullScreen();
    }
}

static void ReinterleavePass()
{
    rBindFrameBuffer(mHBAOResultFB);
    rSetViewportSize(mWidth, mHeight);
    rBindShader(mReinterleaveSH);
    rSetTexture2DArray(mResult2D, 0, 0);
    rRenderFullScreen();
}

static void HorizontalBiliteralBlur()
{
    rBindShader(mBlurShader);
    rBindFrameBuffer(mBlurFB);
    rSetTexture(mHBAOResultTX , 0, 0);
    rSetTexture(mLinearDepthTX, 1, 1);
    rRenderFullScreen();
}

static void HBAOPass(CameraBase* camera, Texture* normalTex)
{
    rBindFrameBuffer(mHBAOProcessFB);
    rSetViewportSize(mQuarterWidth, mQuarterHeight);
    rBindShader(mHBAOSH);
    rSetShaderValue(&camera->view, uViewLoc, GraphicType_Matrix4);
    rSetTexture(*normalTex, 1, uTexViewNormalLoc);
    
    rSetShaderValue(&mHBAOData, uHBAODataLoc, GraphicType_Matrix4);

    for(int i = 0; i < RANDOM_ELEMENTS; i++)
    {
        mHBAORandom[i].w = float(i);
        rSetShaderValue(&mHBAORandom[i].x, uJitterLoc, GraphicType_Vector4f);
        rSetTexture(mDepthViews[i], 0, uTexLinearDepthLoc);
        rFrameBufferAttachColorFrom2DArray(mResult2D, 0, i);
        rRenderFullScreen();
    }
}

void HBAORender(CameraBase* camera, Texture* depthTex, Texture* normalTex)
{
    if (!mIsOpen)
    {
        rBindFrameBuffer(mBlurFB);
        rBindShader(mWhiteShader);
        rRenderFullScreen();
        return;
    }

    rSetDepthTest(false);
    rSetDepthWrite(false);

    SetHBAOData(camera->verticalFOV);

    Linearize(depthTex, camera->nearClip, camera->farClip);
    
    Deinterleave();
    
    HBAOPass(camera, normalTex);
    
    ReinterleavePass();
    
    HorizontalBiliteralBlur();   
}

void HBAODestroy()
{
    rDeleteShader(mLinearizeDepthSH);
    rDeleteShader(mReinterleaveSH);
    rDeleteShader(mDeinterleaveSH);
    rDeleteShader(mHBAOSH);
    rDeleteShader(mBlurShader);

    DeleteTextures();

    rDeleteFrameBuffer(mLinearDepthFB);
    rDeleteFrameBuffer(mHBAOProcessFB);
    rDeleteFrameBuffer(mHBAOResultFB);
    rDeleteFrameBuffer(mDeinterleaveFB);
}

void HBAOEdit(Vector2f pos, int* CurrElement, float textPadding)
{
    pos.y += textPadding;
    uSetElementFocused(*CurrElement == 4);
    if (uFloatField("Radius", pos, &mRadius, 0.1f, 8.0f, 0.1f))
    {
        *CurrElement = 4;
    }

    pos.y += textPadding;
    uSetElementFocused(*CurrElement == 5);
    if (uFloatField("MattersToViewSpace", pos, &mMattersToViewSpace, 0.1f, 8.0f, 0.1f))
    {
        *CurrElement = 5;
    }

    pos.y += textPadding;
    uSetElementFocused(*CurrElement == 6);
    if (uFloatField("Intensity", pos, &mIntensity, 0.0f, 8.0f, 0.1f))
    {
        *CurrElement = 6;
    }
    
    pos.y += textPadding;
    uSetElementFocused(*CurrElement == 7);
    if (uFloatField("Bias", pos, &mBias, 0.0f, 2.0f, 0.01f))
    {
        *CurrElement = 7;
    }
        
    pos.y += textPadding;
    uSetElementFocused(*CurrElement == 8);
    if (uFloatField("Blur Sharpness", pos, &mBlurSharpness, 0.0f, 100.0f, 0.01f))
    {
        *CurrElement = 8;
    }
    
    pos.y += textPadding;
    uSetElementFocused(*CurrElement == 9);
    if (uCheckBox("SSAO", &mIsOpen, pos, true))
    {
        *CurrElement = 9;
    }

    // uSprite(MakeVec2(40.0f, 750.0f), MakeVec2(500.0f, 250.0f), &mBlurResultTX);
    // uSprite(MakeVec2(540.0f, 750.0f), MakeVec2(500.0f, 250.0f), normalTex);
}