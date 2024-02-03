
#include "Scene.hpp"
#include "AssetManager.hpp"
#include "Renderer.hpp"
#include "Platform.hpp"
#include "Camera.hpp"

#include "../ASTL/IO.hpp"
#include "../ASTL/Random.hpp"

// from Renderer.cpp
extern unsigned int g_DefaultTexture;

namespace SceneRenderer
{
    Camera      m_Camera;
    Texture     m_ShadowTexture;
    Texture     m_SkyTexture;
    Matrix4     m_LightMatrix;
    FrameBuffer m_ShadowFrameBuffer;
    Shader      m_ShadowShader;
    Shader      m_PBRShader;
    Shader      m_PostProcessingShader;

    struct MainFrameBuffer
    {
        FrameBuffer Buffer;
        Texture     ColorTexture;
        Texture     DepthTexture;
        Texture     NormalTexture;
        int width, height;
    };

    MainFrameBuffer m_MainFrameBuffer;
    MainFrameBuffer m_MainFrameBufferHalf;
    Shader m_MainFrameBufferCopyShader;
    
    Shader      m_SSAOShader; 
    Shader      m_RedUpsampleShader; 
    Texture     m_SSAONoiseTexture;
    FrameBuffer m_SSAOFrameBuffer;
    Texture     m_SSAOHalfTexture;
    Texture     m_SSAOTexture;
    
    // uniform locations
    unsigned int lViewPos    , lSunDir   , lAlbedo     , lNormalMap, lHasNormalMap, 
                 lMetallicMap, lShadowMap, lLightMatrix, lModel    , lMvp;
    
    unsigned int lShadowModel, lShadowLightMatrix;
}

namespace ShadowSettings
{
    const int ShadowMapSize = 1 << (11 + !IsAndroid()); // mobile 2k, pc 4k
    const float OrthoSize = 35.0f;
    const float NearPlane = 1.0f;
    const float FarPlane = 128.0f;
    
    const float Bias = 0.001f;
    const Vector3f OrthoOffset{};

    inline Matrix4 GetOrthoMatrix()
    {
        return Matrix4::OrthoRH(-OrthoSize, OrthoSize, -OrthoSize, OrthoSize, NearPlane, FarPlane);
    }
}

inline Vector3f ColorMix(Vector3f col1, Vector3f col2, float p)
{
    p = 1.0f - p;
    float t = 1.0f - (p * p);
    Vector3f res = Vector3f::Lerp(col1 * col1, col2 * col2, t);
    return MakeVec3(Sqrt(res.x), Sqrt(res.y), Sqrt(res.z));
}

namespace SceneRenderer {

static void CreateMainFrameBuffer(MainFrameBuffer& frameBuffer, int width, int height, bool half)
{
    frameBuffer.Buffer = rCreateFrameBuffer();
    frameBuffer.width  = width;
    frameBuffer.height = height;
    rBindFrameBuffer(frameBuffer.Buffer);
    frameBuffer.ColorTexture  = rCreateTexture(width, height, nullptr, TextureType_RGB8, TexFlags_Nearest);
    frameBuffer.NormalTexture = rCreateTexture(width, height, nullptr, TextureType_R11F_G11F_B10, TexFlags_Nearest);

    rFrameBufferAttachColor(frameBuffer.ColorTexture , 0);
    rFrameBufferAttachColor(frameBuffer.NormalTexture, 1);

    frameBuffer.DepthTexture  = rCreateDepthTexture(width, height, DepthType_24);
    rFrameBufferAttachDepth(frameBuffer.DepthTexture);
    
    rFrameBufferSetNumColorBuffers(2);
    rFrameBufferCheck();
}

static void DeleteMainFrameBuffer(MainFrameBuffer& frameBuffer)
{
    rDeleteTexture(frameBuffer.ColorTexture);
    rDeleteTexture(frameBuffer.DepthTexture);
    rDeleteTexture(frameBuffer.NormalTexture);
    rDeleteFrameBuffer(frameBuffer.Buffer);
}

static void DeleteSSAOFrameBuffer()
{
    rDeleteFrameBuffer(m_SSAOFrameBuffer);
    rDeleteTexture(m_SSAOHalfTexture);
    rDeleteTexture(m_SSAOTexture);
}

static void CreateSSAOFrameBuffer(int width, int height)
{
    m_SSAOFrameBuffer = rCreateFrameBuffer();
    m_SSAOHalfTexture = rCreateTexture(width / 2, height / 2, nullptr, TextureType_R11F_G11F_B10, TexFlags_ClampToEdge);
    m_SSAOTexture = rCreateTexture(width, height, nullptr, TextureType_R11F_G11F_B10, TexFlags_ClampToEdge);
}

static void WindowResizeCallback(int width, int height)
{
    width = MAX(width, 16);
    height = MAX(height, 16);
    rSetViewportSize(width, height);
    m_Camera.RecalculateProjection(width, height);
    DeleteMainFrameBuffer(m_MainFrameBuffer);
    DeleteMainFrameBuffer(m_MainFrameBufferHalf);

    CreateMainFrameBuffer(m_MainFrameBuffer, width, height, false);
    CreateMainFrameBuffer(m_MainFrameBufferHalf, width / 2, height / 2, true); // ishalf true
    DeleteSSAOFrameBuffer();
    CreateSSAOFrameBuffer(width, height);
}

static void CreateSkyTexture()
{
    uint pixels[64 * 4];
    uint* currentPixel = pixels;

    Vector3f startColor = MakeVec3(0.92f, 0.91f, 0.985f);
    Vector3f endColor = MakeVec3(247.0f, 173.0f, 50.0f) / MakeVec3(255.0f);

    for (int i = 0; i < 64; i++)
    {
        Vector3f target = ColorMix(startColor, endColor, (float)(i) / 64.0f);
        uint color = PackColorRGBU32(&target.x);
        MemSet32(currentPixel, color, 4);
        currentPixel += 4;
    }
    m_SkyTexture = rCreateTexture(4, 64, pixels, TextureType_RGBA8, TexFlags_None);
}

static void GenSSAONoise()
{
    uint64_t noise[8];
    uint64_t xoro[2];
    Random::Xoroshiro128PlusInit(xoro);
    for (int i = 0; i < 8; i++)
        noise[i] = Random::Xoroshiro128Plus(xoro);
    m_SSAONoiseTexture = rCreateTexture(8, 8, noise, TextureType_RG16UI, TexFlags_None);
}

static void GetUniformLocations()
{ 
    lViewPos      = rGetUniformLocation(m_PBRShader, "viewPos");
    lSunDir       = rGetUniformLocation(m_PBRShader, "sunDir");
    lAlbedo       = rGetUniformLocation(m_PBRShader, "albedo");
    lNormalMap    = rGetUniformLocation(m_PBRShader, "normalMap");
    lHasNormalMap = rGetUniformLocation(m_PBRShader, "hasNormalMap");
    lMetallicMap  = rGetUniformLocation(m_PBRShader, "metallicRoughnessMap");
    lShadowMap    = rGetUniformLocation(m_PBRShader, "shadowMap");
    lLightMatrix  = rGetUniformLocation(m_PBRShader, "lightMatrix");
    lModel        = rGetUniformLocation(m_PBRShader, "model");
    lMvp          = rGetUniformLocation(m_PBRShader, "mvp");
}

void RenderShadows(SubScene* subScene, DirectionalLight& sunLight)
{
    rBindShader(m_ShadowShader);
    rBindFrameBuffer(m_ShadowFrameBuffer);
    
    rBeginShadow();
    rClearDepth();

    rSetViewportSize(ShadowSettings::ShadowMapSize, ShadowSettings::ShadowMapSize);
    
    Matrix4 view  = Matrix4::LookAtRH(sunLight.dir * 150.0f, -sunLight.dir, Vector3f::Up());    
    Matrix4 ortho = ShadowSettings::GetOrthoMatrix();
    m_LightMatrix = view * ortho;
    rSetShaderValue(m_LightMatrix.GetPtr(), lShadowLightMatrix, GraphicType_Matrix4);
    
    const Matrix4 model = Matrix4::CreateScale(subScene->data.scale);
    rSetShaderValue(model.GetPtr(), lShadowModel, GraphicType_Matrix4);

    rRenderMesh(subScene->bigMesh); // render all scene with one draw call

    rEndShadow();
    rUnbindFrameBuffer();

    Vector2i windowSize;
    wGetWindowSize(&windowSize.x, &windowSize.y);
    rSetViewportSize(windowSize.x, windowSize.y);
}

inline Shader FullScreenShaderFromPath(const char* path)
{
    ScopedText fragSource = ReadAllText(path, nullptr, nullptr, AX_SHADER_VERSION_PRECISION());
    Shader shader = rCreateFullScreenShader(fragSource);
    return shader;
}

static void CreateShaders()
{
    m_PBRShader = rImportShader("Shaders/3DVert.glsl", "Shaders/PBRFrag.glsl");
    GetUniformLocations();
    
    m_PostProcessingShader      = FullScreenShaderFromPath("Shaders/PostProcessing.glsl");
    m_SSAOShader                = FullScreenShaderFromPath("Shaders/SSAO.glsl");
    m_RedUpsampleShader         = FullScreenShaderFromPath("Shaders/UpscaleRed.glsl");
    m_MainFrameBufferCopyShader = FullScreenShaderFromPath("Shaders/MainFrameBufferCopy.glsl");
}

static void SetupShadowRendering()
{
    const char* vertexShaderSource =
    AX_SHADER_VERSION_PRECISION()
    R"(
        layout(location = 0) in vec3 aPosition;
        uniform mat4 model;
        uniform mat4 lightMatrix;
        
        void main() {
            gl_Position =  model * lightMatrix * vec4(aPosition, 1.0);
        }
    )";

    const char* fragmentShaderSource = AX_SHADER_VERSION_PRECISION() "void main() { }";

    m_ShadowShader      = rCreateShader(vertexShaderSource, fragmentShaderSource);
    m_ShadowFrameBuffer = rCreateFrameBuffer();
    m_ShadowTexture     = rCreateDepthTexture(ShadowSettings::ShadowMapSize, ShadowSettings::ShadowMapSize, DepthType_16);
    
    lShadowModel       = rGetUniformLocation(m_ShadowShader, "model");
    lShadowLightMatrix = rGetUniformLocation(m_ShadowShader, "lightMatrix");
    
    rBindFrameBuffer(m_ShadowFrameBuffer);
    rFrameBufferAttachDepth(m_ShadowTexture);
    rFrameBufferCheck();
}

void Init()
{
    CreateSkyTexture();
    CreateShaders();

    Vector2i windowStartSize;
    wGetMonitorSize(&windowStartSize.x, &windowStartSize.y);
    Vector2i halfRes = windowStartSize / 2;
    
    // Create frame buffers
    CreateSSAOFrameBuffer(halfRes.x, halfRes.y);
    CreateMainFrameBuffer(m_MainFrameBuffer, windowStartSize.x, windowStartSize.y, false);
    CreateMainFrameBuffer(m_MainFrameBufferHalf, halfRes.x, halfRes.y, true);

    m_Camera.Init(windowStartSize);
    wSetWindowResizeCallback(WindowResizeCallback);

    SetupShadowRendering();
}

void RenderSubScene(Scene* scene, SubSceneID subsceneId)
{
    SubScene* subScene = scene->GetSubScene(subsceneId);
    ParsedGLTF& data = subScene->data;
    DirectionalLight sunLight = scene->m_SunLight;
    
    static int twice = 2; // render shadows only once if not dynamic
    if (twice == 0)
    {
        Vector2i windowSize;
        wGetWindowSize(&windowSize.x, &windowSize.y);
        WindowResizeCallback(windowSize.x, windowSize.y);
        RenderShadows(subScene, sunLight);
    }
    twice = MAX(twice - 1, -1);

    rBindFrameBuffer(m_MainFrameBuffer.Buffer);
    rSetViewportSize(m_MainFrameBuffer.width, m_MainFrameBuffer.height);
    rClearDepth();

    // looks like a skybox
    {
        rSetDepthTest(false);
        rRenderFullScreen(m_SkyTexture.handle);
        rSetDepthTest(true);
    }
    rBindShader(m_PBRShader);
    m_Camera.Update();

    rSetShaderValue(&m_Camera.position.x  , lViewPos    , GraphicType_Vector3f);
    rSetShaderValue(&sunLight.dir.x       , lSunDir     , GraphicType_Vector3f);
    rSetShaderValue(m_LightMatrix.GetPtr(), lLightMatrix, GraphicType_Matrix4);
    
    rSetTexture(m_ShadowTexture, 3, lShadowMap);

    int numNodes  = data.numNodes;
    bool hasScene = data.numScenes > 0 && data.scenes[data.defaultSceneIndex].numNodes > 1;
    AScene defaultScene;
    if (hasScene)
    {
        defaultScene = data.scenes[data.defaultSceneIndex];
        numNodes = defaultScene.numNodes;
    }
   
    rBindMesh(subScene->bigMesh);

    Matrix4 viewProjection = m_Camera.view * m_Camera.projection;

    for (int i = 0; i < numNodes; i++)
    {
        ANode node = hasScene ? data.nodes[defaultScene.nodes[i]] : data.nodes[i];
        // if node is not mesh skip (camera)
        if (node.type != 0 || node.index == -1) 
            continue;

        Matrix4 model = Matrix4::PositionRotationScale(node.translation, node.rotation, node.scale);
        Matrix4 mvp = model * viewProjection;

        rSetShaderValue(mvp.GetPtr()  , lMvp  , GraphicType_Matrix4);
        rSetShaderValue(model.GetPtr(), lModel, GraphicType_Matrix4);

        AMesh mesh = data.meshes[node.index];

        for (int j = 0; j < mesh.numPrimitives; ++j)
        {
            APrimitive& primitive = mesh.primitives[j];
            
            if (primitive.numIndices == 0)
                continue;

            AMaterial material = data.materials[primitive.material];
            // SetMaterial(&material);

            int baseColorIndex = material.baseColorTexture.index;
            if (subScene->textures && baseColorIndex != -1)
                rSetTexture(subScene->textures[baseColorIndex], 0, lAlbedo);
            
            int normalIndex  = material.GetNormalTexture().index;
            int hasNormalMap = EnumHasBit(primitive.attributes, AAttribType_TANGENT) && normalIndex != -1;

            if (subScene->textures && hasNormalMap)
                rSetTexture(subScene->textures[normalIndex], 1, lNormalMap);
            
            rSetShaderValue(hasNormalMap, lHasNormalMap);

            int metalicRoughnessIndex = material.metallicRoughnessTexture.index;
            // if (scene->textures && metalicRoughnessIndex != -1 && scene->textures[metalicRoughnessIndex].width != 0)
            //     SetTexture(scene->textures[metalicRoughnessIndex], 2, metallicMapLoc);
            // else
            if (!IsAndroid())
            {
                Texture texture;
                texture.handle = g_DefaultTexture;
                rSetTexture(texture, 2, lMetallicMap);
            }
            int offset = primitive.indexOffset;
            rRenderMeshIndexOffset(subScene->bigMesh, primitive.numIndices, offset);
        }
    }
}

void PostProcessPass()
{
    // Downsample main frame buffer
    rBindFrameBuffer(m_MainFrameBufferHalf.Buffer);
    rSetViewportSize(m_MainFrameBufferHalf.width, m_MainFrameBufferHalf.height);
    {
        rClearDepth();
        rBindShader(m_MainFrameBufferCopyShader);

        rSetTexture(m_MainFrameBuffer.ColorTexture , 0, rGetUniformLocation("ColorTex"));
        rSetTexture(m_MainFrameBuffer.NormalTexture, 1, rGetUniformLocation("NormalTex"));
        rSetTexture(m_MainFrameBuffer.DepthTexture , 2, rGetUniformLocation("DepthTex"));
        rRenderFullScreen();
    }

    rSetDepthTest(false);
    rSetDepthWrite(false);

    // SSAO pass
    rBindFrameBuffer(m_SSAOFrameBuffer);
    rFrameBufferAttachColor(m_SSAOHalfTexture, 0);
    rBindShader(m_SSAOShader);
    {
        rSetTexture(m_MainFrameBufferHalf.DepthTexture , 0, rGetUniformLocation("depthMap")); // m_MainFrameBufferHalf.DepthTexture
        rSetTexture(m_MainFrameBufferHalf.NormalTexture, 1, rGetUniformLocation("normalTex"));
        rSetTexture(m_SSAONoiseTexture                 , 2, rGetUniformLocation("noiseTex"));
    
        rSetShaderValue(m_Camera.view.GetPtr()      , rGetUniformLocation("View"), GraphicType_Matrix4);
        rSetShaderValue(m_Camera.projection.GetPtr(), rGetUniformLocation("Proj"), GraphicType_Matrix4);
        
        rRenderFullScreen();
    }
    // Upsample SSAO
    rSetViewportSize(m_MainFrameBuffer.width, m_MainFrameBuffer.height);
    rFrameBufferAttachColor(m_SSAOTexture, 0);
    rBindShader(m_RedUpsampleShader);
    {
        rSetTexture(m_SSAOHalfTexture, 0, rGetUniformLocation("halfTex"));
        // rSetTexture(m_MainFrameBufferHalf.DepthTexture, 1, rGetUniformLocation("depthMap"));
        rRenderFullScreen();
    }
    rUnbindFrameBuffer(); // < draw to backbuffer after this line
    // ToneMapping and End Post Processing
    {
        rBindShader(m_PostProcessingShader);
        rSetTexture(m_MainFrameBuffer.ColorTexture, 0, rGetUniformLocation("tex"));
        rSetTexture(m_SSAOTexture, 1, rGetUniformLocation("aoTex"));
        rRenderFullScreen();
    }

    rSetDepthTest(true);
    rSetDepthWrite(true);
}

void Destroy()
{
    DeleteSSAOFrameBuffer();
    DeleteMainFrameBuffer(m_MainFrameBuffer);
    DeleteMainFrameBuffer(m_MainFrameBufferHalf);

    rDeleteTexture(m_ShadowTexture);
    rDeleteShader(m_PBRShader);
    rDeleteShader(m_ShadowShader);
    rDeleteShader(m_RedUpsampleShader);
    rDeleteShader(m_SSAOShader);
    rDeleteShader(m_MainFrameBufferCopyShader);
    rDeleteFrameBuffer(m_ShadowFrameBuffer);
}

} // scene renderer namespace end