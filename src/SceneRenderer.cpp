
#include "include/SceneRenderer.hpp"

#include "include/Scene.hpp"
#include "include/Animation.hpp"
#include "include/AssetManager.hpp"
#include "include/Renderer.hpp"
#include "include/Platform.hpp"
#include "include/Camera.hpp"
#include "include/UI.hpp"
#include "include/HBAO.hpp"
#include "include/BVH.hpp"
// #include "include/PostProcessing.hpp"

#include "../ASTL/IO.hpp"
#include "../ASTL/Array.hpp"
#include "../ASTL/Queue.hpp"
#include "../ASTL/String.hpp"
#include "../ASTL/Math/Color.hpp"

// from Renderer.cpp
extern unsigned int g_DefaultTexture;

namespace SceneRenderer
{
    CameraBase* m_Camera;
    FreeCamera   m_FreeCamera;
    PlayerCamera m_PlayerCamera;

    Matrix4     m_ViewProjection;
    Matrix4     m_LightMatrix;

    Shader      m_ShadowShader;
    Shader      m_GBufferShader;
    Shader      m_GBufferShaderAlpha;
    Shader      m_SkyboxShader;
    Shader      m_DepthCopyShader;
    Shader      m_PostProcessingShader; // tone-mapping, vignate
    Shader      m_MLAAEdgeShader;
    Shader      m_MLAAShader;
    Shader      m_GodRaysShader;
    Shader      m_DeferredPBRShader;
    Shader      m_BlackShader;

    GPUMesh     m_BoxMesh; // -0.5, 0.5 scaled
    
    FrameBuffer m_ShadowFrameBuffer;
    FrameBuffer m_MLAAEdgeFrameBuffer;
    FrameBuffer m_LightingFrameBuffer;
    FrameBuffer m_PostProcessingFrameBuffer;
    FrameBuffer m_GodRaysFB;

    Texture     m_GodRaysTex;
    Texture     m_MLAAEdgeTex;
    Texture     m_ShadowTexture;
    Texture     m_LightingTexture;
    Texture     m_SkyNoiseTexture; // 32x32 small noise
    Texture     m_WhiteTexture;

    struct GBuffer
    {
        FrameBuffer Buffer;
        Texture     ColorTexture; // < a component is shadow
        Texture     DepthTexture; 
        Texture     NormalTexture;// < a component is metallic
        Texture     RoughnessTexture;
        int width, height;
    };

    GBuffer m_Gbuffer;

    // Gbuffer uniform locations
    int lAlbedo, lNormalMap, lHasNormalMap, lMetallicMap, lShadowMap, lLightMatrix, 
        lModel , lHasAnimation, lSunDirG, lViewProj, lAnimTex;

    // Deferred uniform locations
    int lSunDir, lPlayerPos, lAlbedoTex, lRoughnessTex, lNormalTex, lDepthMap, lInvView, lInvProj, lAmbientOclussionTex;

    // Skybox uniform locations
    int lSkyTime, lSkySun, lSkyViewPos, lSkyViewProj, lSkyNoiseTex;
    // SSAO uniform locations
    int sDepthMap, sNormalTex, sView;
    
    // SSAO downsample uniform locations
    int dColorTex, dNormalTex, dDepthTex;

    int uMLAAColorTex, uMLAAEdgeTex, uMLAAInputTex, uMLAAGodRaysTex, uMLAAAmbientOcclussionTex;

    struct LightUniforms
    {
        int lIntensities[MaxNumLights];
        int lDirections[MaxNumLights];
        int lPositions[MaxNumLights];
        int lCutoffs[MaxNumLights];
        int lColors[MaxNumLights];
        int lRanges[MaxNumLights];
    };

    LightUniforms m_PointLightUniforms;
    LightUniforms m_SpotLightUniforms;
    
    int lNumPointLights;
    int lNumSpotLights;

    // Shadow uniform locations
    int lShadowModel, lShadowLightMatrix;
    
    Array<KeyValuePair<APrimitive*, Matrix4>> m_DelayedAlphaCutoffs;
    AMaterial m_defaultMaterial;

    bool m_ShadowFollowCamera = false;
    int  m_RedrawShadows = false; // maybe: set this after we rotate sun
    bool m_ShouldReRender = false;

    Vector3f m_CharacterPos;
}

namespace ShadowSettings
{
    const int ShadowMapSize = 1 << (11 + (!IsAndroid() << 1)); // mobile 2k, pc 4k
    float OrthoSize   = 32.0f;//128.0f; 
    float NearPlane   = 1.0f;
    float FarPlane    = 192.0f;
    
    float Bias = 0.001f;
    Vector3f OrthoOffset = {}; // { 32.0f, 56.0f, 5.0f }; // < bistro

    inline Matrix4 GetOrthoMatrix()
    {
        return Matrix4::OrthoRH(-OrthoSize, OrthoSize, -OrthoSize, OrthoSize, NearPlane, FarPlane);
    }
}

namespace SceneRenderer 
{

static void CreateShaders();
static void DeleteShaders();

void DrawLastRenderedFrame()
{
    rRenderFullScreen(m_LightingTexture.handle);
}

CameraBase* GetCamera()
{
    return m_Camera;
}

Matrix4* GetViewProjection()
{
    return &m_ViewProjection;
}

void SetCharacterPos(float x, float y, float z)
{
    m_CharacterPos.x = x; m_CharacterPos.y = y; m_CharacterPos.z = z;
}

#ifdef __ANDROID__
// make width and height smaller, to improve cache utilization and less processing
// return h; // https://www.youtube.com/watch?v=feb-Hl_Cl3g  28:11
inline int GetRBWidth(int w, int h) {
    return w / 2;
}

inline int GetRBHeight(int w, int h) {
    return h - (h / 4);
}
#else
inline int GetRBWidth(int w, int h) {
    return w;
}

inline int GetRBHeight(int w, int h) {
    return h;
}
#endif

static void CreateGBuffer(GBuffer& gBuffer, int width, int height)
{
    width  = GetRBWidth(width, height);
    height = GetRBHeight(width, height);

    gBuffer.Buffer = rCreateFrameBuffer();
    gBuffer.width  = width;
    gBuffer.height = height;
    rBindFrameBuffer(gBuffer.Buffer);
    gBuffer.ColorTexture     = rCreateTexture(width, height, nullptr, TextureType_RGBA8, TexFlags_RawData);
    gBuffer.NormalTexture    = rCreateTexture(width, height, nullptr, TextureType_RGBA8, TexFlags_RawData);
    gBuffer.RoughnessTexture = rCreateTexture(width, height, nullptr, TextureType_R8, TexFlags_RawData);

    rFrameBufferAttachColor(gBuffer.ColorTexture    , 0);
    rFrameBufferAttachColor(gBuffer.NormalTexture   , 1);
    rFrameBufferAttachColor(gBuffer.RoughnessTexture, 2);

    __const DepthType depthType = IsAndroid() ? DepthType_24 : DepthType_32;
    gBuffer.DepthTexture  = rCreateDepthTexture(width, height, depthType);
    rFrameBufferAttachDepth(gBuffer.DepthTexture);

    rFrameBufferSetNumColorBuffers(3);
    rFrameBufferCheck();

    m_LightingTexture = rCreateTexture(width, height, nullptr, TextureType_RGBA8, TexFlags_RawData);
    m_LightingFrameBuffer = rCreateFrameBuffer(true);
    rFrameBufferAttachColor(m_LightingTexture);
    rFrameBufferCheck();

    m_PostProcessingFrameBuffer = rCreateFrameBuffer(true);
    rFrameBufferAttachColor(gBuffer.ColorTexture);
    rFrameBufferAttachDepth(gBuffer.DepthTexture);
    rFrameBufferCheck();

    m_GodRaysTex = rCreateTexture(width, height, nullptr, TextureType_R8, TexFlags_RawData);
    m_GodRaysFB  = rCreateFrameBuffer(true);
    rFrameBufferAttachColor(m_GodRaysTex, 0);

    m_MLAAEdgeTex = rCreateTexture(width, height, nullptr, TextureType_R16UI, TexFlags_RawData);
    m_MLAAEdgeFrameBuffer = rCreateFrameBuffer(true);
    rFrameBufferAttachColor(m_MLAAEdgeTex, 0);
}

static void DeleteGBuffer(GBuffer& gbuffer)
{
    rDeleteTexture(gbuffer.ColorTexture);
    rDeleteTexture(gbuffer.DepthTexture);
    rDeleteTexture(gbuffer.NormalTexture);
    rDeleteTexture(gbuffer.RoughnessTexture);
    rDeleteTexture(m_MLAAEdgeTex);
    rDeleteTexture(m_GodRaysTex);
    rDeleteTexture(m_LightingTexture);
    rDeleteFrameBuffer(m_MLAAEdgeFrameBuffer);
    rDeleteFrameBuffer(m_GodRaysFB);
    rDeleteFrameBuffer(m_PostProcessingFrameBuffer);
    rDeleteFrameBuffer(m_LightingFrameBuffer);
    rDeleteFrameBuffer(gbuffer.Buffer);
}

void WindowResizeCallback(int width, int height)
{
    int smallerWidth  = GetRBWidth(width, height);
    int smallerHeight = GetRBHeight(width, height);
    width = MAX(width, 16);
    height = MAX(height, 16);
    rSetViewportSize(smallerWidth, smallerHeight);

    m_FreeCamera.RecalculateProjection(width, height);
    m_PlayerCamera.RecalculateProjection(width, height);

    DeleteGBuffer(m_Gbuffer);
    HBAOResize(smallerWidth, smallerHeight);
    // PostProcessingResize(width, height);
    CreateGBuffer(m_Gbuffer, width, height);
    m_RedrawShadows = 2;
    m_ShouldReRender = true;
}

static void GetUniformLocations()
{ 
    char lightText[32] = {};
    int arrBegin;

    rBindShader(m_GBufferShader);
    lSunDirG        = rGetUniformLocation("uSunDir");
    lAlbedo         = rGetUniformLocation("uAlbedo");
    lNormalMap      = rGetUniformLocation("uNormalMap");
    lHasNormalMap   = rGetUniformLocation("uHasNormalMap");
    lMetallicMap    = rGetUniformLocation("uMetallicRoughnessMap");
    lShadowMap      = rGetUniformLocation("uShadowMap");
    lLightMatrix    = rGetUniformLocation("uLightMatrix");
    lModel          = rGetUniformLocation("uModel");
    lHasAnimation   = rGetUniformLocation("uHasAnimation");
    lViewProj       = rGetUniformLocation("uViewProj");
    lAnimTex        = rGetUniformLocation("uAnimTex");

    rBindShader(m_DeferredPBRShader);
    lPlayerPos                  = rGetUniformLocation("uPlayerPos");
    lSunDir                     = rGetUniformLocation("uSunDir");
    lAlbedoTex                  = rGetUniformLocation("uAlbedoShadowTex");
    lRoughnessTex               = rGetUniformLocation("uRoughnessTex");
    lNormalTex                  = rGetUniformLocation("uNormalMetallicTex");
    lDepthMap                   = rGetUniformLocation("uDepthMap");
    lInvView                    = rGetUniformLocation("uInvView");
    lInvProj                    = rGetUniformLocation("uInvProj");
    lNumPointLights             = rGetUniformLocation("uNumPointLights");
    lNumSpotLights              = rGetUniformLocation("uNumSpotLights");
    lAmbientOclussionTex        = rGetUniformLocation("uAmbientOclussionTex");

    arrBegin = sizeof("uPointLights");
    // we get all of the array uniform locations not just first uniform
    rGetUniformArrayLocations(arrBegin, lightText, m_PointLightUniforms.lPositions  , MaxNumLights, "uPointLights[0].position");
    rGetUniformArrayLocations(arrBegin, lightText, m_PointLightUniforms.lColors     , MaxNumLights, "uPointLights[0].color");
    rGetUniformArrayLocations(arrBegin, lightText, m_PointLightUniforms.lDirections , MaxNumLights, "uPointLights[0].direction");
    rGetUniformArrayLocations(arrBegin, lightText, m_PointLightUniforms.lIntensities, MaxNumLights, "uPointLights[0].intensity");
    rGetUniformArrayLocations(arrBegin, lightText, m_PointLightUniforms.lCutoffs    , MaxNumLights, "uPointLights[0].cutoff");
    rGetUniformArrayLocations(arrBegin, lightText, m_PointLightUniforms.lRanges     , MaxNumLights, "uPointLights[0].range");

    arrBegin = sizeof("uSpotLights");
    rGetUniformArrayLocations(arrBegin, lightText, m_SpotLightUniforms.lPositions  , MaxNumLights, "uSpotLights[0].position");
    rGetUniformArrayLocations(arrBegin, lightText, m_SpotLightUniforms.lColors     , MaxNumLights, "uSpotLights[0].color");
    rGetUniformArrayLocations(arrBegin, lightText, m_SpotLightUniforms.lDirections , MaxNumLights, "uSpotLights[0].direction");
    rGetUniformArrayLocations(arrBegin, lightText, m_SpotLightUniforms.lIntensities, MaxNumLights, "uSpotLights[0].intensity");
    rGetUniformArrayLocations(arrBegin, lightText, m_SpotLightUniforms.lCutoffs    , MaxNumLights, "uSpotLights[0].cutoff");
    rGetUniformArrayLocations(arrBegin, lightText, m_SpotLightUniforms.lRanges     , MaxNumLights, "uSpotLights[0].range");

    // skybox locations
    rBindShader(m_SkyboxShader);
    lSkyTime = rGetUniformLocation("time");
    lSkySun  = rGetUniformLocation("fsun"); 
    lSkyViewPos = rGetUniformLocation("viewPos");
    lSkyViewProj = rGetUniformLocation("uViewProj");
    lSkyNoiseTex = rGetUniformLocation("noiseTex");

    // shadow locations
    lShadowModel       = rGetUniformLocation(m_ShadowShader, "model");
    lShadowLightMatrix = rGetUniformLocation(m_ShadowShader, "lightMatrix");
    
    uMLAAColorTex   = rGetUniformLocation(m_MLAAShader, "uColorTex"); 
    uMLAAEdgeTex    = rGetUniformLocation(m_MLAAShader, "uEdgesTex"); 
    uMLAAGodRaysTex = rGetUniformLocation(m_MLAAShader, "uGodRaysTex"); 
    uMLAAAmbientOcclussionTex = rGetUniformLocation(m_MLAAShader, "uAmbientOcclussion");

    uMLAAInputTex = rGetUniformLocation(m_MLAAEdgeShader, "uInputTex");
}

static void CreateShaders()
{
    m_DeferredPBRShader = rImportFullScreenShader("Shaders/DeferredPBR.glsl");

    ScopedText gbufferVertexShader   = ReadAllText("Shaders/3DVert.glsl", nullptr, nullptr, AX_SHADER_VERSION_PRECISION());
    ScopedText gbufferFragmentShader = ReadAllText("Shaders/GBuffer.glsl", nullptr, nullptr, AX_SHADER_VERSION_PRECISION());
    m_GBufferShader = rCreateShader(gbufferVertexShader.text, gbufferFragmentShader.text, "gbufferVert", "gbufferFrag");

    // Compile alpha cutoff version of the gbuffer shader, to make sure early z test. I really don't want an branch for discard statement. 
    int alphaIndex = StringContains(gbufferFragmentShader.text, "ALPHA_CUTOFF");
    ASSERT(alphaIndex != -1);
    gbufferFragmentShader.text[alphaIndex + sizeof("ALPHA_CUTOFF")] = '1';
    m_GBufferShaderAlpha = rCreateShader(gbufferVertexShader.text, gbufferFragmentShader.text, "gbufferVert", "gbufferFrag");

    m_DepthCopyShader = rCreateFullScreenShader(
                            AX_SHADER_VERSION_PRECISION()
                            "in vec2 texCoord; uniform sampler2D DepthTex;"
                            "void main() { gl_FragDepth = texture(DepthTex, texCoord).r; }"
                        );

    ScopedText shadowVertexShader = ReadAllText("Shaders/ShadowVert.glsl", nullptr, nullptr, AX_SHADER_VERSION_PRECISION());
    const char* shadowFragmentShader = AX_SHADER_VERSION_PRECISION() "void main() { }";
    m_ShadowShader      = rCreateShader(shadowVertexShader.text, shadowFragmentShader, "ShadowVert", "ShadowFrag");

    m_SkyboxShader = rImportShader("Shaders/SkyboxVert.glsl", "Shaders/SkyboxFrag.glsl");

    m_PostProcessingShader = rImportFullScreenShader("Shaders/PostProcessing.glsl");
    m_GodRaysShader = rImportFullScreenShader("Shaders/GodRays.glsl");

    m_MLAAEdgeShader = rImportFullScreenShader("Shaders/MLAA_Edge.glsl");
    m_MLAAShader     = rImportFullScreenShader("Shaders/MLAA.glsl");

    m_BlackShader = rCreateFullScreenShader(
        AX_SHADER_VERSION_PRECISION()
        "layout(location = 0) out float result;"
        "void main() { result = 0.0; }"
    );
    
    GetUniformLocations();
}

static void DeleteShaders()
{
    rDeleteShader(m_BlackShader);
    rDeleteShader(m_PostProcessingShader);
    rDeleteShader(m_GodRaysShader);
    rDeleteShader(m_GBufferShader);
    rDeleteShader(m_DeferredPBRShader);
    rDeleteShader(m_ShadowShader);
    rDeleteShader(m_MLAAShader);
    rDeleteShader(m_MLAAEdgeShader);
}

static void SetupShadowRendering()
{
    m_ShadowFrameBuffer = rCreateFrameBuffer();
    m_ShadowTexture     = rCreateDepthTexture(ShadowSettings::ShadowMapSize, ShadowSettings::ShadowMapSize, DepthType_24);

    rBindFrameBuffer(m_ShadowFrameBuffer);
    rFrameBufferAttachDepth(m_ShadowTexture);
    rFrameBufferCheck();

    m_RedrawShadows = 2;
}

void BeginUpdateLights()
{
    rBindShader(m_DeferredPBRShader);
}

void UpdateLight(int index, LightInstance* instance)
{
    index &= 0x7FFFFFF;
    LightUniforms* uniforms = instance->cutoff > 0.0f ? &m_SpotLightUniforms : &m_PointLightUniforms;
    rSetShaderValue(&instance->position.x , uniforms->lPositions[index] , GraphicType_Vector3f);
    rSetShaderValue(&instance->direction.x, uniforms->lDirections[index], GraphicType_Vector3f);
    rSetShaderValue(instance->color    , uniforms->lColors[index]);
    rSetShaderValue(instance->intensity, uniforms->lIntensities[index]);
    rSetShaderValue(instance->cutoff   , uniforms->lCutoffs[index]);
    rSetShaderValue(instance->range    , uniforms->lRanges[index]);
}

void EndUpdateLights() { }

static void PrepareSkybox()
{
    const Vector3f vertices[] = {
        {-0.5f, -0.5f, -0.5f},
        { 0.5f, -0.5f, -0.5f},
        { 0.5f,  0.5f, -0.5f},
        {-0.5f,  0.5f, -0.5f},
        {-0.5f,  0.5f, 0.5f},
        { 0.5f,  0.5f, 0.5f},
        { 0.5f, -0.5f, 0.5f},
        {-0.5f, -0.5f, 0.5f}
    };

    const ushort triangles[] = {
        0, 2, 1, 0, 3, 2, // face front 
        2, 3, 4, 2, 4, 5, // face top
        1, 2, 5, 1, 5, 6, // face right
        0, 7, 4, 0, 4, 3, // face left
        5, 4, 7, 5, 7, 6, // face back
        0, 6, 7, 0, 1, 6  // face bottom
    };
    
    const InputLayout layout = { 3, GraphicType_Float };
    const InputLayoutDesc desc = { 1, sizeof(Vector3f), &layout, false };

    m_BoxMesh = rCreateMesh(vertices, triangles, ArraySize(vertices), ArraySize(triangles), GraphicType_UnsignedShort, &desc);
    m_SkyNoiseTexture = rImportTexture("Shaders/PerlinNoise.png");
}

void Init()
{
    PrepareSkybox();
    CreateShaders();

    Vector2i windowStartSize;
    Vector2i windowSmallSize;
    wGetMonitorSize(&windowStartSize.x, &windowStartSize.y);
    windowSmallSize = { GetRBWidth(windowStartSize.x, windowStartSize.y), GetRBHeight(windowStartSize.x, windowStartSize.y) };

    CreateGBuffer(m_Gbuffer, windowStartSize.x, windowStartSize.y);

    m_FreeCamera.Init(windowStartSize);
    m_PlayerCamera.Init(windowStartSize);

    m_Camera = &m_PlayerCamera; // reinterpret_cast<CameraBase*>(&m_PlayerCamera); // ->Init(windowStartSize);
    // m_Camera = &m_FreeCamera; // reinterpret_cast<CameraBase*>(&m_PlayerCamera); // ->Init(windowStartSize);

    HBAOInit(windowSmallSize.x, windowSmallSize.y);

    // PostProcessingInit(windowStartSize.x, windowStartSize.y);

    SetupShadowRendering();

    MemsetZero(&m_defaultMaterial, sizeof(AMaterial));
    m_defaultMaterial.metallicFactor  = 1256;
    m_defaultMaterial.roughnessFactor = 1256;
    m_defaultMaterial.diffuseColor    = 0xFCBD8733;
    m_defaultMaterial.specularColor   = 0xFCBD8733;
    m_defaultMaterial.baseColorFactor = 0xFFFFFFFF;
    m_defaultMaterial.baseColorTexture.index = UINT16_MAX;
    m_defaultMaterial.GetNormalTexture().index = -1;

    uint8_t whiteTexData[8 * 8];
    FillN(whiteTexData, (uint8_t)0xff, 8 * 8);
    m_WhiteTexture = rCreateTexture(8, 8, whiteTexData, TextureType_R8, TexFlags_ClampToEdge);
}

void BeginRendering()
{
    if (GetKeyPressed('L'))
    {
        if (m_Camera == &m_PlayerCamera) 
            m_Camera = &m_FreeCamera;
        else
            m_Camera = &m_PlayerCamera;
    }

    rBindFrameBuffer(m_Gbuffer.Buffer);
    rSetViewportSize(m_Gbuffer.width, m_Gbuffer.height);
    rClearColor(0.2f, 0.2f, 0.2f, 1.0);
    rClearDepth();

    m_Camera->Update();
    m_ViewProjection = m_Camera->view * m_Camera->projection;

    rBindShader(m_GBufferShader);

    // shadow uniforms
    rSetShaderValue(m_LightMatrix.GetPtr(), lLightMatrix, GraphicType_Matrix4);

    rSetTexture(m_ShadowTexture, 3, lShadowMap);
}

static void RenderShadowOfNode(ANode* node, Prefab* prefab, Matrix4 parentMat)
{
    Matrix4 model = Matrix4::PositionRotationScale(node->translation, node->rotation, node->scale) * parentMat;
    // is camera or empty node skip.
    if (node->type == 1 || node->index == -1)
    {
        for (int i = 0; i < node->numChildren; i++)
        {
            RenderShadowOfNode(&prefab->nodes[node->children[i]], prefab, model);
        }
        return;
    }
    rSetShaderValue(model.GetPtr(), lShadowModel, GraphicType_Matrix4);

    AMesh mesh = prefab->meshes[node->index];
    for (int i = 0; i < mesh.numPrimitives; i++)
    {
        APrimitive* primitive = &mesh.primitives[i];
        rRenderMeshIndexOffset(prefab->bigMesh, primitive->numIndices, primitive->indexOffset); 
    }

    for (int i = 0; i < node->numChildren; i++)
    {
        RenderShadowOfNode(&prefab->nodes[node->children[i]], prefab, model);
    }
}

static void RenderShadows(Prefab* prefab, DirectionalLight& sunLight, AnimationController* animSystem)
{
    int hasAnimation = (int)(animSystem != nullptr);
    if (hasAnimation) rSetTexture(animSystem->mMatrixTex, 0, rGetUniformLocation("uAnimTex"));
    rSetShaderValue(hasAnimation, rGetUniformLocation("uHasAnimation"));

    bool hasScene = prefab->numScenes > 0;

    rBindMesh(prefab->bigMesh);
    if (!hasScene)
    {
        Matrix4 model = Matrix4::FromScale(prefab->scale);
        rSetShaderValue(model.GetPtr(), lShadowModel, GraphicType_Matrix4);
        rRenderMeshIndexed(prefab->bigMesh); // render all scene with one draw call
    }
    else
    {
        AScene defaultScene = prefab->scenes[prefab->defaultSceneIndex];
        int numNodes = defaultScene.numNodes;
        for (int i = 0; i < numNodes; i++)
        {
            RenderShadowOfNode(&prefab->nodes[defaultScene.nodes[i]], prefab, Matrix4::Identity());
        }
    }
}

void BeginShadowRendering(Scene* scene)
{
    if (m_RedrawShadows != 0) return;
    
    DirectionalLight sunLight = scene->m_SunLight;
    rBindShader(m_ShadowShader);
    rBindFrameBuffer(m_ShadowFrameBuffer);
    rSetViewportSize(ShadowSettings::ShadowMapSize, ShadowSettings::ShadowMapSize);
    rClearDepth();

    Vector3f offset = ShadowSettings::OrthoOffset; // m_ShadowFollowCamera ? m_Camera.targetPos : Vector3f::Zero();
    Matrix4 view  = Matrix4::LookAtRH(sunLight.dir * 90.0f + offset, -sunLight.dir, Vector3f::Up());
    Matrix4 ortho = ShadowSettings::GetOrthoMatrix();
    m_LightMatrix = view * ortho;

    rSetShaderValue(m_LightMatrix.GetPtr(), lShadowLightMatrix, GraphicType_Matrix4);

    rBeginShadow();
}

void RenderShadowOfPrefab(Scene* scene, PrefabID prefabID, AnimationController* animSystem)
{
    if (m_RedrawShadows == 0)// || prefab->firstTimeRender == 0)
    {
        Prefab* prefab = scene->GetPrefab(prefabID);
        DirectionalLight sunLight = scene->m_SunLight;
    
        RenderShadows(prefab, sunLight, animSystem);
    }
}

void EndShadowRendering()
{
    if (m_RedrawShadows == 0)// || prefab->firstTimeRender == 0)
    {
        rEndShadow();
        rUnbindFrameBuffer();
        Vector2i windowSize;
        wGetWindowSize(&windowSize.x, &windowSize.y);
        rSetViewportSize(windowSize.x, windowSize.y);
    }

    m_RedrawShadows -= 1;
    m_RedrawShadows = MAX(m_RedrawShadows, -1);
}

bool ShouldReRender()
{
    bool should = m_ShouldReRender == true;
    m_ShouldReRender = false;
    return should;
}

static void RenderPrimitive(AMaterial& material, Prefab* prefab, APrimitive& primitive)
{
    int baseColorIndex = material.baseColorTexture.index;
    if (prefab->numTextures > 0 && baseColorIndex != UINT16_MAX)
        rSetTexture(prefab->GetGPUTexture(baseColorIndex), 0, lAlbedo);

    int normalIndex  = material.GetNormalTexture().index;
    int hasNormalMap = EnumHasBit(primitive.attributes, AAttribType_TANGENT) && normalIndex != UINT16_MAX;

    if (prefab->numTextures > 0 && hasNormalMap)
        rSetTexture(prefab->GetGPUTexture(normalIndex), 1, lNormalMap);

    rSetShaderValue(hasNormalMap, lHasNormalMap);

    int metalicRoughnessIndex = material.metallicRoughnessTexture.index;
    if (metalicRoughnessIndex == UINT16_MAX)
        metalicRoughnessIndex = material.specularTexture.index;

    if (prefab->textures && metalicRoughnessIndex != UINT16_MAX && prefab->gpuTextures[metalicRoughnessIndex].width != 0)
        rSetTexture(prefab->gpuTextures[metalicRoughnessIndex], 2, lMetallicMap);
    else
    if (!IsAndroid())
    {
        Texture texture;
        texture.handle = g_DefaultTexture;
        rSetTexture(texture, 2, lMetallicMap);
    }
    int offset = primitive.indexOffset;
    rRenderMeshIndexOffset(prefab->bigMesh, primitive.numIndices, offset);
}


static int numCulled = 0;

// Renders to gbuffer
void RenderPrefab(Scene* scene, PrefabID prefabID, AnimationController* animSystem)
{
    Prefab* prefab = scene->GetPrefab(prefabID);
    const int hasAnimation = (int)(prefab->numSkins > 0 && animSystem != nullptr);

    rBindShader(m_GBufferShader);
    rSetShaderValue(&scene->m_SunLight.dir.x, lSunDirG, GraphicType_Vector3f);
    rSetShaderValue(hasAnimation, lHasAnimation);

    rBindMesh(prefab->bigMesh);

    rSetShaderValue(m_ViewProjection.GetPtr(), lViewProj, GraphicType_Matrix4);
    rSetShaderValue(m_LightMatrix.GetPtr(), lLightMatrix, GraphicType_Matrix4);

    int numNodes  = prefab->numNodes;
    bool hasScene = prefab->numScenes > 0;
    AScene defaultScene;
    if (hasScene)
    {
        defaultScene = prefab->scenes[prefab->defaultSceneIndex];
        numNodes = defaultScene.numNodes;
    }

    if (hasAnimation)
    {
        rSetTexture(animSystem->mMatrixTex, 4, lAnimTex);
    }
        
    Queue<int> nodeQueue = {};
    nodeQueue.Enqueue(hasScene ? defaultScene.nodes[0] : 0);

    while (!nodeQueue.Empty())
    {
        int nodeIndex = nodeQueue.Dequeue();
        ANode& node = prefab->nodes[nodeIndex];
        Matrix4 model = prefab->globalNodeTransforms[nodeIndex]; // Matrix4::PositionRotationScale(node.translation, node.rotation, node.scale) * parentMat;

        // if node is not mesh skip (camera or empty node)
        AMesh mesh = prefab->meshes[node.index];

        // Random::PCG pcg;
        // Random::PCGInitialize(pcg, 0x675890u);
        // Vector3f corners[8];

        if (node.type == 0 && node.index != -1)
        for (int j = 0; j < mesh.numPrimitives; ++j)
        {
            APrimitive& primitive = mesh.primitives[j];
            bool hasMaterial = prefab->materials && primitive.material != UINT16_MAX;
            AMaterial material = hasMaterial ? prefab->materials[primitive.material] : m_defaultMaterial;
            
            vec_t vmin = VecSet1(1e30f);  
            vec_t vmax = VecSet1(-1e30f); 

            // convert local Bounds to global bounds
            for (int i = 0; i < 8; i++)
            {
                vec_t point = VecSetR(i & 1 ? primitive.max[0] : primitive.min[0],
                                      i & 2 ? primitive.max[1] : primitive.min[1],
                                      i & 4 ? primitive.max[2] : primitive.min[2], 1.0f);
                point = Vector3Transform(point, model.r);
                vmin = VecMin(vmin, point);
                vmax = VecMax(vmax, point);
            }

            // float rnd = Random::NextFloat01(Random::PCGNext(pcg));
            // GetAABBCorners(corners, vmin, vmax);
            // rDrawCube(corners, HUEToRGBU32(0.1f));

            bool shouldDraw = true;
            bool culled = CheckAABBCulled(vmin, vmax, m_Camera->frustumPlanes, model);
            shouldDraw &= culled;
            numCulled += culled == false;

            shouldDraw &= primitive.numIndices != 0;
            bool isAlpha = (material.alphaMode == AMaterialAlphaMode_Mask ||
                material.alphaMode == AMaterialAlphaMode_Blend);

            // if alpha blended render after all meshes for performance
            if (shouldDraw && isAlpha) {
                m_DelayedAlphaCutoffs.EmplaceBack(&mesh.primitives[j], model);
                shouldDraw = false;
            }

            if (shouldDraw) {
                // SetMaterial(&material);
                rSetShaderValue(model.GetPtr(), lModel, GraphicType_Matrix4);
                RenderPrimitive(material, prefab, primitive);
            }
        }

        for (int i = 0; i < node.numChildren; i++)
        {
            nodeQueue.Enqueue(node.children[i]);
        }
    }

    // Render alpha masked meshes after opaque meshes, to make sure performance is good. (early z)
    if (m_DelayedAlphaCutoffs.Size() > 0)
    {
        rBindShader(m_GBufferShaderAlpha);
        // shadow uniforms
        rSetShaderValue(m_LightMatrix.GetPtr(), lLightMatrix, GraphicType_Matrix4);
        rSetShaderValue(m_ViewProjection.GetPtr(), lViewProj, GraphicType_Matrix4);

        rSetTexture(m_ShadowTexture, 3, lShadowMap);

        // todo we need parent matrix instead of primitive pointer
        for (int i = 0; i < m_DelayedAlphaCutoffs.Size(); i++)
        {
            KeyValuePair<APrimitive*, Matrix4> alphaCutoff = m_DelayedAlphaCutoffs[i];

            APrimitive& primitive = *alphaCutoff.key;
            bool hasMaterial = prefab->materials && primitive.material != UINT16_MAX;
            AMaterial material = hasMaterial ? prefab->materials[primitive.material] : m_defaultMaterial;

            rSetShaderValue(alphaCutoff.value.GetPtr(), lModel, GraphicType_Matrix4); // < set model matrix
            RenderPrimitive(material, prefab, primitive);
        }
        m_DelayedAlphaCutoffs.Resize(0);
    }
}

static bool MeshInstanceIndexCompare(MeshInstance* a, MeshInstance* b)
{
    return a->meshIndex < b->meshIndex;
}

void RenderAllSceneContent(Scene* scene)
{
    // MeshInstance* instances = scene->m_MeshInstances.Data();
    // const int numInstances = scene->m_MeshInstances.Size();
    // 
    // QuickSortFn(instances, 0, numInstances - 1, MeshInstanceIndexCompare);
    // 
    // {
    //     rBindShader(m_GBufferShader);
    //     rSetShaderValue(&scene->m_SunLight.dir.x, lSunDirG, GraphicType_Vector3f);
    //     rSetShaderValue(hasAnimation, lHasAnimation);
    // 
    //     rBindMesh(prefab->bigMesh);
    // 
    //     Matrix4 viewProjection = m_Camera.view * m_Camera.projection;
    //     rSetShaderValue(viewProjection.GetPtr(), lViewProj, GraphicType_Matrix4);
    //     rSetShaderValue(m_LightMatrix.GetPtr(), lLightMatrix, GraphicType_Matrix4);
    // }
    // 
    // for (const MeshInstance& instance : scene->m_MeshInstances.Size())
    // {
    //     
    // }
}

static void LightingPass()
{
    DirectionalLight sunLight = g_CurrentScene.m_SunLight;

    rBindShader(m_DeferredPBRShader);
    {
        Matrix4 invView = m_Camera->inverseView;
        Matrix4 invProj = m_Camera->inverseProjection;

        rSetShaderValue(&sunLight.dir.x  , lSunDir, GraphicType_Vector3f);
        rSetShaderValue(&m_CharacterPos.x, lPlayerPos, GraphicType_Vector3f);

        rSetShaderValue(invView.GetPtr(), lInvView, GraphicType_Matrix4);
        rSetShaderValue(invProj.GetPtr(), lInvProj, GraphicType_Matrix4);

        rSetTexture(m_Gbuffer.ColorTexture    , 0, lAlbedoTex);
        rSetTexture(m_Gbuffer.RoughnessTexture, 1, lRoughnessTex);
        rSetTexture(m_Gbuffer.NormalTexture   , 2, lNormalTex);
        rSetTexture(m_Gbuffer.DepthTexture    , 3, lDepthMap);
        rSetTexture(HBAOGetResult()           , 4, lAmbientOclussionTex);

        rSetShaderValue(g_CurrentScene.m_PointLights.Size(), lNumPointLights);
        rSetShaderValue(g_CurrentScene.m_SpotLights.Size(), lNumSpotLights);
        rRenderFullScreen();
    }

    rBindFrameBuffer(m_MLAAEdgeFrameBuffer);
    rBindShader(m_MLAAEdgeShader);
    rSetTexture(m_LightingTexture, 0, uMLAAInputTex);
    rRenderFullScreen();

    // does post processing as well
    rBindFrameBuffer(m_PostProcessingFrameBuffer); // < uses gbuffers color attachment
    rBindShader(m_MLAAShader);
    rSetTexture(m_LightingTexture, 0, uMLAAColorTex);
    rSetTexture(m_MLAAEdgeTex, 1, uMLAAEdgeTex);
    rSetTexture(m_GodRaysTex, 2, uMLAAGodRaysTex);

    if (IsAndroid())  rSetTexture(m_WhiteTexture, 3, uMLAAAmbientOcclussionTex);
    else              rSetTexture(HBAOGetResult(), 3, uMLAAAmbientOcclussionTex);

    rRenderFullScreen();
}

static void DrawSkybox()
{
    rSetClockWise(true);
        rBindShader(m_SkyboxShader);
        rSetShaderValue((float)TimeSinceStartup() * 0.3f, lSkyTime);
        rSetShaderValue(g_CurrentScene.m_SunLight.dir.arr, lSkySun, GraphicType_Vector3f);
        rSetShaderValue(m_Camera->position.arr, lSkyViewPos, GraphicType_Vector3f);
        rSetShaderValue(m_ViewProjection.GetPtr(), lSkyViewProj, GraphicType_Matrix4);
        rSetTexture(m_SkyNoiseTexture, 0, lSkyNoiseTex);
        rBindMesh(m_BoxMesh);
        rRenderMeshIndexed(m_BoxMesh);
    rSetClockWise(false);
}

static void GodRaysPass()
{
    Texture depthTexture = m_Gbuffer.DepthTexture;
    Vector3f sunDir = g_CurrentScene.m_SunLight.dir;
    Vector3f camDir = m_Camera->Front;
    Vector2f sunScreenCoord = WorldToNDC(m_ViewProjection, m_CharacterPos + (sunDir * 600.0f));
    sunScreenCoord = (sunScreenCoord + 1.0) * 0.5f; // convert to 0, 1 space

    float angle = Abs(ACos(Dot(sunDir, camDir)));

    rBindFrameBuffer(m_GodRaysFB);

    if (!IsAndroid() &&  angle < PI) // god rays are disabled on android
    {
        float fovRad = m_Camera->verticalFOV * DegToRad;
        float intensity = ((PI - angle) / PI);

        rBindShader(m_GodRaysShader);
        rSetShaderValue(intensity, 0);
        rSetShaderValue(sunScreenCoord.arr, 1, GraphicType_Vector2f);
        rSetTexture(depthTexture, 0, 2);
        rRenderFullScreen();
    }
    else
    {
        rBindShader(m_BlackShader);
        rRenderFullScreen();
    }
    
    // rBindFrameBuffer(m_PostProcessingFrameBuffer); // gbuffer's color is target
    // 
    // rBindShader(m_PostProcessingShader);
    // rSetTexture(m_LightingTexture, rGetUniformLocation("uLightingTex"), 0);
    // rSetTexture(m_GodRaysTex, rGetUniformLocation("uGodRays"), 1);
    // rSetTexture(HBAOGetResult(), rGetUniformLocation("uAmbientOcclussion"), 2);
    // rRenderFullScreen();
}

void EndRendering(bool renderToBackBuffer)
{
    Vector2i windowSize;
    wGetWindowSize(&windowSize.x, &windowSize.y);
    int smallerWidth  = GetRBWidth(windowSize.x, windowSize.y);
    int smallerHeight = GetRBHeight(windowSize.x, windowSize.y);

    HBAORender(m_Camera, &m_Gbuffer.DepthTexture, &m_Gbuffer.NormalTexture);

    rSetViewportSize(smallerWidth, smallerHeight);

    // screen space passes, no need depth
    rSetDepthWrite(false);
    rSetDepthTest(false);

    GodRaysPass();
    rBindFrameBuffer(m_LightingFrameBuffer);
    
    LightingPass(); // Deferred Lighting

    rSetDepthTest(true);

    rBindFrameBuffer(m_Gbuffer.Buffer);

    DrawSkybox();
    
    rSetViewportSize(windowSize.x, windowSize.y);
    rUnbindFrameBuffer(); // < draw to backbuffer after this line
    // rRenderFullScreen(m_LightingTexture.handle);
    rRenderFullScreen(m_Gbuffer.ColorTexture.handle);

    rSetDepthTest(true);
    rSetDepthWrite(true);

    // char culledText[16]={};
    // IntToString(culledText, numCulled);
    // uDrawText(culledText, MakeVec2(1810.0f, 185.0f));
    // numCulled = 0;
}

void Destroy()
{
    DeleteGBuffer(m_Gbuffer);

    DeleteShaders();

    rDeleteTexture(m_ShadowTexture);
    rDeleteFrameBuffer(m_ShadowFrameBuffer);
    HBAODestroy();
}


void ShowEditor()
{
    static bool open = false;
    if (GetKeyPressed('B')) open = !open;
    if (!open) return;
    Vector2f bgPos   = { 20.0f, 90.0f };
    Vector2f bgScale = { 450.0f, 600.0f };
    Vector2f pos = bgPos;

    uQuad(pos, bgScale, uGetColor(uColorQuad) & 0x77FFFFFFu);
    uBorder(pos, bgScale);

    const float textPadding = 13.0f;
    Vector2f zero2 = { 0.0f, 0.0f };

    // uSetFloat(ufContentStart, uGetFloat(ufContentStart) * 2.0f);
    float settingElementWidth = bgScale.x / 1.15f;
    float elementsXOffset = bgScale.x / 2.0f - (settingElementWidth / 2.0f);

    Vector2f textSize = uCalcTextSize("Graphics");
    uPushFloat(ufContentStart, settingElementWidth);
 
    // uPushFloat(ufTextScale, uGetFloat(ufTextScale) * 1.0f);
    float settingsXStart = 20.0f;
    pos.y += textSize.y + textPadding;
    pos.x += settingsXStart;
    uText("Graphics", pos);
    // uPopFloat(ufTextScale);

    float lineLength = bgScale.x * 0.85f;
    float xoffset = (bgScale.x - lineLength) * 0.5f; // where line starts
    pos.x += xoffset;
    pos.y += 20.0f; // line padding
    pos.x -= settingsXStart;

    uPushColor(uColorLine, uGetColor(uColorSelectedBorder));
    uLineHorizontal(pos, lineLength, uTriEffect_None);
    uPopColor(uColorLine);

    pos.x -= xoffset;
    pos.x += elementsXOffset;
    pos.y += textSize.y + textPadding;

    static int CurrElement = 0;
    const int numElements = 11; // number of options plus back button

    uPushFloat(ufTextScale, 0.6f);
    uPushFloat(ufFieldWidth, uGetFloat(ufFieldWidth) * 0.8f);

    uSetElementFocused(CurrElement == 0);
    if (uFloatField("Ortho Size", pos, &ShadowSettings::OrthoSize, 16.0f, 512.0f, 0.5f))
    {
        CurrElement = 0;
        m_RedrawShadows = 1;
    }

    pos.y += textSize.y + textPadding;
    uSetElementFocused(CurrElement == 1);
    static int orthoIndex = 0;
    if (uFloatVecField("Ortho Offset", pos, &ShadowSettings::OrthoOffset.x, 3, &orthoIndex, -200.0f, +200.0f, 0.2f))
    {
        CurrElement = 1;
        m_RedrawShadows = 1;
    }

    pos.y += textSize.y + textPadding;
    uSetElementFocused(CurrElement == 2);
    if (uFloatField("Sun Angle", pos, &g_CurrentScene.m_SunAngle, -PI, PI+0.15f, 0.04f))
    {
        m_RedrawShadows = 1;
        CurrElement = 2;
    }

    pos.y += textSize.y + textPadding;
    uSetElementFocused(CurrElement == 3);
    if (uFloatField("Far Plane", pos, &ShadowSettings::FarPlane, 8.0f, 256.0f, 0.04f))
    {
        m_RedrawShadows = 1;
        CurrElement = 3;
    }

    HBAOEdit(pos, &CurrElement, textSize.y + textPadding);

    uPopFloat(ufTextScale);
    uPopFloat(ufFieldWidth);
    uPopFloat(ufContentStart);

    // uSprite(MakeVec2(1100.0f, 500.0f), MakeVec2(800.0f, 550.0f), &m_ShadowTexture);

    if (GetKeyPressed('K'))
    {
        DeleteShaders();
        CreateShaders();
    }
}

} // scene renderer namespace endndndnd