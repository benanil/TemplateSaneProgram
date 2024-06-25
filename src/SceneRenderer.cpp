
#include "include/SceneRenderer.hpp"

#include "include/Scene.hpp"
#include "include/Animation.hpp"
#include "include/AssetManager.hpp"
#include "include/Renderer.hpp"
#include "include/Platform.hpp"
#include "include/Camera.hpp"
#include "include/UI.hpp"

#include "../ASTL/IO.hpp"
#include "../ASTL/Array.hpp"
#include "../ASTL/String.hpp"

// from Renderer.cpp
extern unsigned int g_DefaultTexture;

namespace SceneRenderer
{
    Camera      m_Camera;
    Texture     m_ShadowTexture;
    Matrix4     m_ViewProjection;
    Matrix4     m_LightMatrix;

    Texture     m_SkyTexture;
    FrameBuffer m_ShadowFrameBuffer;
    Shader      m_ShadowShader;
    
    Shader      m_GBufferShader;
    Shader      m_GBufferShaderAlpha;
    Shader      m_SkyboxShader;
    Shader      m_DepthCopyShader;
    GPUMesh     m_BoxMesh; // -0.5, 0.5 scaled

    struct MainFrameBuffer
    {
        FrameBuffer Buffer;
        Texture     ColorTexture; // < a component is shadow
        Texture     DepthTexture; 
        Texture     NormalTexture;// < a component is metallic
        Texture     RoughnessTexture;
        int width, height;
    };

    MainFrameBuffer m_MainFrameBuffer;
    // MainFrameBuffer m_MainFrameBufferHalf;
    Shader m_MainFrameBufferCopyShader;
    
    Shader      m_SSAOShader; 
    Shader      m_RedUpsampleShader; 
    FrameBuffer m_SSAOFrameBuffer;
    Texture     m_SSAOHalfTexture;
    Texture     m_SSAOTexture;

    FrameBuffer m_ResultFrameBuffer;
    Texture m_ResultTexture;
    Texture m_ResultDepthTexture;

    // deferred rendering
    Shader      m_DeferredPBRShader;

    // Gbuffer uniform locations
    int lAlbedo, lNormalMap, lHasNormalMap, lMetallicMap, lShadowMap, lLightMatrix, 
        lModel , lHasAnimation, lSunDirG, lViewProj, lAnimTex;

    // Deferred uniform locations
    int lSunDir, lPlayerPos, lAlbedoTex, lRoughnessTex, lNormalTex, lDepthMap, lInvView, lInvProj, lAmbientOclussionTex;

    // SSAO uniform locations
    int sDepthMap, sNormalTex, sView;
    
    // SSAO downsample uniform locations
    int dColorTex, dNormalTex, dDepthTex;

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
    const int ShadowMapSize = 1 << (11 + !IsAndroid()); // mobile 2k, pc 4k
    const float OrthoSize   = 32.0f; 
    const float NearPlane   = 1.0f;
    const float FarPlane    = 192.0f;
    
    const float Bias = 0.001f;
    const Vector3f OrthoOffset{};

    inline Matrix4 GetOrthoMatrix()
    {
        return Matrix4::OrthoRH(-OrthoSize, OrthoSize, -OrthoSize, OrthoSize, NearPlane, FarPlane);
    }
}

namespace SceneRenderer 
{

void DrawLastRenderedFrame()
{
    rClearDepth();
    rRenderFullScreen(m_DepthCopyShader, m_MainFrameBuffer.DepthTexture.handle);
    rRenderFullScreen(m_ResultTexture.handle);
}

Camera* GetCamera()
{
    return &m_Camera;
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

static void CreateMainFrameBuffer(MainFrameBuffer& frameBuffer, int width, int height, bool half)
{
    width  = GetRBWidth(width, height);
    height = GetRBHeight(width, height);
    frameBuffer.Buffer = rCreateFrameBuffer();
    frameBuffer.width  = width;
    frameBuffer.height = height;
    rBindFrameBuffer(frameBuffer.Buffer);
    frameBuffer.ColorTexture  = rCreateTexture(width, height, nullptr, TextureType_RGBA8, TexFlags_Nearest);
    frameBuffer.NormalTexture = rCreateTexture(width, height, nullptr, TextureType_RGBA8, TexFlags_Nearest);
    frameBuffer.RoughnessTexture = rCreateTexture(width, height, nullptr, TextureType_R8, TexFlags_Nearest);

    rFrameBufferAttachColor(frameBuffer.ColorTexture , 0);
    rFrameBufferAttachColor(frameBuffer.NormalTexture, 1);
    rFrameBufferAttachColor(frameBuffer.RoughnessTexture, 2);

    __const DepthType depthType = IsAndroid() ? DepthType_24 : DepthType_32;
    frameBuffer.DepthTexture  = rCreateDepthTexture(width, height, depthType);
    rFrameBufferAttachDepth(frameBuffer.DepthTexture);

    rFrameBufferSetNumColorBuffers(3);
    rFrameBufferCheck();
}

static void DeleteMainFrameBuffer(MainFrameBuffer& frameBuffer)
{
    rDeleteTexture(frameBuffer.ColorTexture);
    rDeleteTexture(frameBuffer.DepthTexture);
    rDeleteTexture(frameBuffer.NormalTexture);
    rDeleteTexture(frameBuffer.RoughnessTexture);
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
    width  = GetRBWidth(width, height);
    height = GetRBHeight(width, height);
    m_SSAOFrameBuffer = rCreateFrameBuffer();
    m_SSAOHalfTexture = rCreateTexture(width / 2, height / 2, nullptr, TextureType_R8, TexFlags_ClampToEdge | TexFlags_Nearest);
    m_SSAOTexture     = rCreateTexture(width    , height    , nullptr, TextureType_R8, TexFlags_ClampToEdge | TexFlags_Nearest);
}

static void CreateFrameBuffers(int width, int height)
{
    __const DepthType depthType = IsAndroid() ? DepthType_24 : DepthType_32;

    m_ResultFrameBuffer = rCreateFrameBuffer();
    m_ResultTexture = rCreateTexture(width, height, nullptr, TextureType_RGBA8, TexFlags_ClampToEdge | TexFlags_Nearest);
    m_ResultDepthTexture = rCreateDepthTexture(width, height, depthType);
    rBindFrameBuffer(m_ResultFrameBuffer);
    rFrameBufferAttachColor(m_ResultTexture, 0);
    rFrameBufferSetNumColorBuffers(1);
    rFrameBufferAttachDepth(m_ResultDepthTexture);
    rFrameBufferCheck();
    
    CreateMainFrameBuffer(m_MainFrameBuffer, width, height, false);
    // CreateMainFrameBuffer(m_MainFrameBufferHalf, width / 2, height / 2, true); // last arg ishalf true
    CreateSSAOFrameBuffer(width, height);
}

static void DeleteFrameBuffers()
{
    rDeleteTexture(m_ResultTexture);
    rDeleteTexture(m_ResultDepthTexture);
    rDeleteFrameBuffer(m_ResultFrameBuffer);

    DeleteMainFrameBuffer(m_MainFrameBuffer);
    // DeleteMainFrameBuffer(m_MainFrameBufferHalf);
    DeleteSSAOFrameBuffer();
}

void WindowResizeCallback(int width, int height)
{
    int smallerWidth  = GetRBWidth(width, height);
    int smallerHeight = GetRBHeight(width, height);
    width = MAX(width, 16);
    height = MAX(height, 16);
    rSetViewportSize(smallerWidth, smallerHeight);
    m_Camera.RecalculateProjection(width, height);
    DeleteFrameBuffers();
    CreateFrameBuffers(width, height);
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

    // SSAO uniform locations
    sDepthMap  = rGetUniformLocation(m_SSAOShader, "uDepthMap");
    sNormalTex = rGetUniformLocation(m_SSAOShader, "uNormalTex");
    sView      = rGetUniformLocation(m_SSAOShader, "uView");

    // SSAO downsample uniform locations
    rBindShader(m_MainFrameBufferCopyShader);
    dNormalTex = rGetUniformLocation("uNormalTex"); 
    dDepthTex  = rGetUniformLocation("uDepthTex");

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
}

inline Shader FullScreenShaderFromPath(const char* path)
{
    ScopedText fragSource = ReadAllText(path, nullptr, nullptr, AX_SHADER_VERSION_PRECISION());
    Shader shader = rCreateFullScreenShader(fragSource);
    return shader;
}

static void CreateShaders()
{
    m_SSAOShader                = FullScreenShaderFromPath("Shaders/SSAO.glsl");
    m_RedUpsampleShader         = FullScreenShaderFromPath("Shaders/UpscaleRed.glsl");
    m_MainFrameBufferCopyShader = FullScreenShaderFromPath("Shaders/MainFrameBufferCopy.glsl");
    m_DeferredPBRShader         = FullScreenShaderFromPath("Shaders/DeferredPBR.glsl");

    ScopedText gbufferVertexShader   = ReadAllText("Shaders/3DVert.glsl", nullptr, nullptr, AX_SHADER_VERSION_PRECISION());
    ScopedText gbufferFragmentShader = ReadAllText("Shaders/GBuffer.glsl", nullptr, nullptr, AX_SHADER_VERSION_PRECISION());
    m_GBufferShader = rCreateShader(gbufferVertexShader.text, gbufferFragmentShader.text);

    // Compile alpha cutoff version of the gbuffer shader, to make sure early z test. I really don't want an branch for discard statement. 
    int alphaIndex = StringContains(gbufferFragmentShader.text, "ALPHA_CUTOFF");
    ASSERT(alphaIndex != -1);
    gbufferFragmentShader.text[alphaIndex + sizeof("ALPHA_CUTOFF")] = '1';
    m_GBufferShaderAlpha = rCreateShader(gbufferVertexShader.text, gbufferFragmentShader.text);

    GetUniformLocations();
    m_DepthCopyShader = rCreateFullScreenShader(
                            AX_SHADER_VERSION_PRECISION()
                            "in vec2 texCoord; uniform sampler2D DepthTex;"
                            "void main() { gl_FragDepth = texture(DepthTex, texCoord).r; }"
                        );
}

static void DeleteShaders()
{
    rDeleteShader(m_GBufferShader);
    rDeleteShader(m_SSAOShader);
    rDeleteShader(m_RedUpsampleShader);
    rDeleteShader(m_MainFrameBufferCopyShader);
    rDeleteShader(m_DeferredPBRShader);
    rDeleteShader(m_ShadowShader);
}

static void SetupShadowRendering()
{
    ScopedText shadowVertexShader = ReadAllText("Shaders/ShadowVert.glsl", nullptr, nullptr, AX_SHADER_VERSION_PRECISION());
    const char* shadowFragmentShader = AX_SHADER_VERSION_PRECISION() "void main() { }";

    m_ShadowShader      = rCreateShader(shadowVertexShader.text, shadowFragmentShader);
    m_ShadowFrameBuffer = rCreateFrameBuffer();
    m_ShadowTexture     = rCreateDepthTexture(ShadowSettings::ShadowMapSize, ShadowSettings::ShadowMapSize, DepthType_16);

    lShadowModel       = rGetUniformLocation(m_ShadowShader, "model");
    lShadowLightMatrix = rGetUniformLocation(m_ShadowShader, "lightMatrix");

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
        5, 4, 7, 5, 7, 6 // face back
        // 0, 6, 7, 0, 1, 6  // face bottom
    };
    
    const InputLayout layout = { 3, GraphicType_Float };
    const InputLayoutDesc desc = { 1, sizeof(Vector3f), &layout, false };

    m_BoxMesh = rCreateMesh(vertices, triangles, ArraySize(vertices), ArraySize(triangles), GraphicType_UnsignedShort, &desc);
    m_SkyboxShader = rImportShader("Shaders/SkyboxVert.glsl", "Shaders/SkyboxFrag.glsl");
}

void Init()
{
    PrepareSkybox();
    CreateShaders();

    Vector2i windowStartSize;
    wGetMonitorSize(&windowStartSize.x, &windowStartSize.y);

    CreateFrameBuffers(windowStartSize.x, windowStartSize.y);

    m_Camera.Init(windowStartSize);

    SetupShadowRendering();

    MemsetZero(&m_defaultMaterial, sizeof(AMaterial));
    m_defaultMaterial.metallicFactor  = 1256;
    m_defaultMaterial.roughnessFactor = 1256;
    m_defaultMaterial.diffuseColor    = 0xFCBD8733; 
    m_defaultMaterial.specularColor   = 0xFCBD8733; 
    m_defaultMaterial.baseColorFactor = 0xFFFFFFFF;
    m_defaultMaterial.baseColorTexture.index = -1;
    m_defaultMaterial.GetNormalTexture().index = -1;
}

void BeginRendering()
{
    rBindFrameBuffer(m_MainFrameBuffer.Buffer);
    rSetViewportSize(m_MainFrameBuffer.width, m_MainFrameBuffer.height);
    rClearColor(0.2f, 0.2f, 0.2f, 1.0);
    rClearDepth();

    m_Camera.Update();
    m_ViewProjection = m_Camera.view * m_Camera.projection;

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
        rRenderMeshIndexOffset(prefab->bigMesh, primitive->numIndices, primitive->indexOffset); // render all scene with one draw call
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
        Matrix4 model = Matrix4::CreateScale(prefab->scale);
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

    Vector3f offset = m_ShadowFollowCamera ? m_Camera.targetPos : Vector3f::Zero();
    Matrix4 view  = Matrix4::LookAtRH(sunLight.dir * 50.0f + offset, -sunLight.dir, Vector3f::Up());
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
    if (prefab->numTextures > 0 && baseColorIndex != -1)
        rSetTexture(prefab->GetGPUTexture(baseColorIndex), 0, lAlbedo);

    int normalIndex  = material.GetNormalTexture().index;
    int hasNormalMap = EnumHasBit(primitive.attributes, AAttribType_TANGENT) && normalIndex != -1;

    if (prefab->numTextures > 0 && hasNormalMap)
        rSetTexture(prefab->GetGPUTexture(normalIndex), 1, lNormalMap);

    rSetShaderValue(hasNormalMap, lHasNormalMap);

    int metalicRoughnessIndex = material.metallicRoughnessTexture.index;
    if (prefab->textures && metalicRoughnessIndex != -1 && prefab->gpuTextures[metalicRoughnessIndex].width != 0)
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

static void RenderNodeRec(ANode& node, Prefab* prefab, Matrix4 parentMat, bool recurse)
{
    Matrix4 model = Matrix4::PositionRotationScale(node.translation, node.rotation, node.scale) * parentMat;

    // if node is not mesh skip (camera or empty node)
    if (node.type != 0 || node.index == -1)
    {
        for (int i = 0; i < node.numChildren && recurse; i++)
        {
            RenderNodeRec(prefab->nodes[node.children[i]], prefab, model, recurse);
        }
        return;
    }

    AMesh mesh = prefab->meshes[node.index];

    for (int j = 0; j < mesh.numPrimitives; ++j)
    {
        APrimitive& primitive = mesh.primitives[j];
        bool hasMaterial = prefab->materials && primitive.material != -1;
        AMaterial material = hasMaterial ? prefab->materials[primitive.material] : m_defaultMaterial;
        vec_t aabbMin = VecLoadA(primitive.min);
        vec_t aabbMax = VecLoadA(primitive.max);
        bool shouldDraw = true;

        shouldDraw &= CheckAABBCulled(aabbMin, aabbMax, m_Camera.frustumPlanes, model);
        // shouldDraw &= primitive.numIndices != 0;

        // if alpha blended render after all meshes for performance
        if (shouldDraw && material.alphaMode == AMaterialAlphaMode_Mask) { 
            m_DelayedAlphaCutoffs.EmplaceBack(&mesh.primitives[j], model);
            shouldDraw = false;
        }

        if (shouldDraw) {
            // SetMaterial(&material);
            rSetShaderValue(model.GetPtr(), lModel, GraphicType_Matrix4);
            RenderPrimitive(material, prefab, primitive);
        }

        for (int i = 0; i < node.numChildren && recurse; i++)
        {
            RenderNodeRec(prefab->nodes[node.children[i]], prefab, model, recurse);
        }
    }
}

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
        
    for (int i = 0; i < numNodes; i++)
    {
        ANode& node = hasScene ? prefab->nodes[defaultScene.nodes[i]] : prefab->nodes[i];
        bool shouldRecurse = hasScene;
        RenderNodeRec(node, prefab, Matrix4::Identity(), shouldRecurse);
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
            bool hasMaterial = prefab->materials && primitive.material != -1;
            AMaterial material = hasMaterial ? prefab->materials[primitive.material] : m_defaultMaterial;

            rSetShaderValue(alphaCutoff.value.GetPtr(), lModel, GraphicType_Matrix4); // < set model matrix
            RenderPrimitive(material, prefab, primitive);
        }
        m_DelayedAlphaCutoffs.Resize(0);
    }
    rDrawAllLines(m_ViewProjection.GetPtr());
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

static void DownscaleMainFrameBuffer()
{
    // Downscale main frame buffer
    // rBindFrameBuffer(m_MainFrameBufferHalf.Buffer);
    // rSetViewportSize(m_MainFrameBufferHalf.width, m_MainFrameBufferHalf.height);
    // {
    //     rClearDepth();
    //     rBindShader(m_MainFrameBufferCopyShader);
    // 
    //     rSetTexture(m_MainFrameBuffer.ColorTexture , 0, dColorTex);
    //     rSetTexture(m_MainFrameBuffer.NormalTexture, 1, dNormalTex);
    //     rSetTexture(m_MainFrameBuffer.DepthTexture , 2, dDepthTex);
    //     rRenderFullScreen();
    // }
}

static void SSAOPass()
{
    // // SSAO pass
    // rBindFrameBuffer(m_SSAOFrameBuffer);
    // rFrameBufferAttachColor(m_SSAOHalfTexture, 0);
    // rBindShader(m_SSAOShader);
    // {
    //     rSetTexture(m_MainFrameBufferHalf.DepthTexture , 0, sDepthMap); // m_MainFrameBufferHalf.DepthTexture
    //     rSetTexture(m_MainFrameBufferHalf.NormalTexture, 1, sNormalTex);
    //     rRenderFullScreen();
    // }
    // // Upsample SSAO
    // rSetViewportSize(m_MainFrameBuffer.width, m_MainFrameBuffer.height);
    // rFrameBufferAttachColor(m_SSAOTexture, 0);
    // rBindShader(m_RedUpsampleShader);
    // {
    //     rSetTexture(m_SSAOHalfTexture, 0, rGetUniformLocation("halfTex"));
    //     rRenderFullScreen();
    // }
}

static void LightingPass()
{
    DirectionalLight sunLight = g_CurrentScene.m_SunLight;

    rBindShader(m_DeferredPBRShader);
    {
        Matrix4 invView = Matrix4::Inverse(m_Camera.view);
        Matrix4 invProj = Matrix4::Inverse(m_Camera.projection);

        rSetShaderValue(&sunLight.dir.x  , lSunDir, GraphicType_Vector3f);
        rSetShaderValue(&m_CharacterPos.x, lPlayerPos, GraphicType_Vector3f);

        rSetShaderValue(invView.GetPtr(), lInvView, GraphicType_Matrix4);
        rSetShaderValue(invProj.GetPtr(), lInvProj, GraphicType_Matrix4);

        rSetTexture(m_MainFrameBuffer.ColorTexture              , 0, lAlbedoTex);
        rSetTexture(m_MainFrameBuffer.RoughnessTexture, 1, lRoughnessTex);
        rSetTexture(m_MainFrameBuffer.NormalTexture             , 2, lNormalTex);
        rSetTexture(m_MainFrameBuffer.DepthTexture              , 3, lDepthMap);
        rSetTexture(m_SSAOTexture                               , 4, lAmbientOclussionTex);

        rSetShaderValue(g_CurrentScene.m_PointLights.Size(), lNumPointLights);
        rSetShaderValue(g_CurrentScene.m_SpotLights.Size(), lNumSpotLights);
        rRenderFullScreen();
    }

    // rBindFrameBuffer(m_MainFrameBuffer.Buffer);
    // rFrameBufferInvalidate(3); // color, normal, ShadowMetallicRoughness
}

static void DrawSkybox()
{
    rSetClockWise(true);
        rBindShader(m_SkyboxShader);
        rSetShaderValue(m_ViewProjection.GetPtr(), 0, GraphicType_Matrix4);
        rBindMesh(m_BoxMesh);
        rRenderMeshIndexed(m_BoxMesh);
    rSetClockWise(false);
}

void EndRendering(bool renderToBackBuffer)
{
    Vector2i windowSize;
    wGetWindowSize(&windowSize.x, &windowSize.y);
    rSetViewportSize(windowSize.x, windowSize.y);

    if (renderToBackBuffer)
    {
        rUnbindFrameBuffer(); // < draw to backbuffer after this line
    }
    else
    {
        rBindFrameBuffer(m_ResultFrameBuffer);
    }

    // copy depth buffer to back buffer
    rRenderFullScreen(m_DepthCopyShader, m_MainFrameBuffer.DepthTexture.handle);

    // DownscaleMainFrameBuffer();
    //SSAOPass();
    
    // screen space passes, no need depth
    rSetDepthTest(false);
    rSetDepthWrite(false);
    
    {
        LightingPass();
    }

    rSetDepthTest(true);
    {
        DrawSkybox();
    }
    rSetDepthWrite(true);
    
    if (!renderToBackBuffer)
    {
        rUnbindFrameBuffer();
    }
}

void Destroy()
{
    DeleteFrameBuffers();

    DeleteShaders();

    rDeleteTexture(m_ShadowTexture);
    rDeleteFrameBuffer(m_ShadowFrameBuffer);
}

} // scene renderer namespace endndndnd