
#include "Scene.hpp"
#include "AssetManager.hpp"
#include "Renderer.hpp"
#include "Platform.hpp"
#include "Camera.hpp"

#include "../ASTL/IO.hpp"
#include "../ASTL/Array.hpp"
#include "../ASTL/String.hpp"

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
    
    Shader      m_GBufferShader;
    Shader      m_GBufferShaderAlpha;

    struct MainFrameBuffer
    {
        FrameBuffer Buffer;
        Texture     ColorTexture;
        Texture     DepthTexture;
        Texture     NormalTexture;
        Texture     ShadowMetallicRoughnessTex;
        int width, height;
    };

    MainFrameBuffer m_MainFrameBuffer;
    MainFrameBuffer m_MainFrameBufferHalf;
    Shader m_MainFrameBufferCopyShader;
    
    Shader      m_SSAOShader; 
    Shader      m_RedUpsampleShader; 
    FrameBuffer m_SSAOFrameBuffer;
    Texture     m_SSAOHalfTexture;
    Texture     m_SSAOTexture;
    
    // deferred rendering
    Shader      m_DeferredPBRShader;

    // Gbuffer uniform locations
    int lAlbedo, lNormalMap, lHasNormalMap, lMetallicMap, lShadowMap, lLightMatrix, lModel, lHasSkin, lSunDirG, lViewProj;
    const int NumJoints = 32;
    int lInvBindMatrices[NumJoints];
    int lJointMatrices[NumJoints];
    
    // Deferred uniform locations
    int lSunDir, lAlbedoTex, lShadowMetallicRoughnessTex, lNormalTex, lDepthMap, lInvView, lInvProj, lAmbientOclussionTex;
    const int NumLights = 16;

    // SSAO uniform locations
    int sDepthMap, sNormalTex, sView;
    
    // SSAO downsample uniform locations
    int dColorTex, dNormalTex, dDepthTex;

    struct LightUniforms
    {
        int lIntensities[NumLights];
        int lDirections[NumLights];
        int lPositions[NumLights];
        int lCutoffs[NumLights];
        int lColors[NumLights];
        int lRanges[NumLights];
    };

    LightUniforms m_PointLightUniforms;
    LightUniforms m_SpotLightUniforms;
    
    int lNumPointLights;
    int lNumSpotLights;

    // Shadow uniform locations
    int lShadowModel, lShadowLightMatrix;
    
    Array<KeyValuePair<ANode*, APrimitive*>> m_DelayedAlphaCutoffs;
    AMaterial m_defaultMaterial;
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
    frameBuffer.NormalTexture = rCreateTexture(width, height, nullptr, TextureType_RGB8, TexFlags_Nearest);
    frameBuffer.ShadowMetallicRoughnessTex = rCreateTexture(width, height, nullptr, TextureType_RGB565, TexFlags_Nearest);

    rFrameBufferAttachColor(frameBuffer.ColorTexture , 0);
    rFrameBufferAttachColor(frameBuffer.NormalTexture, 1);
    rFrameBufferAttachColor(frameBuffer.ShadowMetallicRoughnessTex, 2);
    
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
    rDeleteTexture(frameBuffer.ShadowMetallicRoughnessTex);
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
    m_SSAOHalfTexture = rCreateTexture(width / 2, height / 2, nullptr, TextureType_R8, TexFlags_ClampToEdge | TexFlags_Nearest);
    m_SSAOTexture     = rCreateTexture(width    , height    , nullptr, TextureType_R8, TexFlags_ClampToEdge | TexFlags_Nearest);
}

static void CreateFrameBuffers(int width, int height)
{
    CreateMainFrameBuffer(m_MainFrameBuffer, width, height, false);
    CreateMainFrameBuffer(m_MainFrameBufferHalf, width / 2, height / 2, true); // last arg ishalf true
    CreateSSAOFrameBuffer(width, height);
}

static void DeleteFrameBuffers()
{
    DeleteMainFrameBuffer(m_MainFrameBuffer);
    DeleteMainFrameBuffer(m_MainFrameBufferHalf);
    DeleteSSAOFrameBuffer();
}

static void WindowResizeCallback(int width, int height)
{
    width = MAX(width, 16);
    height = MAX(height, 16);
    rSetViewportSize(width, height);
    m_Camera.RecalculateProjection(width, height);
    DeleteFrameBuffers();
    CreateFrameBuffers(width, height);
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
    lHasSkin        = rGetUniformLocation("uHasSkin");
    lViewProj       = rGetUniformLocation("uViewProj");
    arrBegin = sizeof("uInvBindMatrices");
    rGetUniformArrayLocations(arrBegin, lightText, lInvBindMatrices, NumJoints, "uInvBindMatrices[0]");
    
    arrBegin = sizeof("uJointMatrices");
    rGetUniformArrayLocations(arrBegin, lightText, lJointMatrices, NumJoints, "uJointMatrices[0]");
    
    // SSAO uniform locations
    sDepthMap  = rGetUniformLocation(m_SSAOShader, "uDepthMap");
    sNormalTex = rGetUniformLocation(m_SSAOShader, "uNormalTex");
    sView      = rGetUniformLocation(m_SSAOShader, "uView");
    
    // SSAO downsample uniform locations
    rBindShader(m_MainFrameBufferCopyShader);
    dColorTex  = rGetUniformLocation("uColorTex");
    dNormalTex = rGetUniformLocation("uNormalTex"); 
    dDepthTex  = rGetUniformLocation("uDepthTex");

    rBindShader(m_DeferredPBRShader);
    lSunDir                     = rGetUniformLocation("uSunDir");
    lAlbedoTex                  = rGetUniformLocation("uAlbedoTex");
    lShadowMetallicRoughnessTex = rGetUniformLocation("uShadowMetallicRoughnessTex");
    lNormalTex                  = rGetUniformLocation("uNormalTex");
    lDepthMap                   = rGetUniformLocation("uDepthMap");
    lInvView                    = rGetUniformLocation("uInvView");
    lInvProj                    = rGetUniformLocation("uInvProj");
    lNumPointLights             = rGetUniformLocation("uNumPointLights");
    lNumSpotLights              = rGetUniformLocation("uNumSpotLights");
    lAmbientOclussionTex        = rGetUniformLocation("uAmbientOclussionTex");

    arrBegin = sizeof("uPointLights");
    // we get all of the array uniform locations not just first uniform
    rGetUniformArrayLocations(arrBegin, lightText, m_PointLightUniforms.lPositions  , NumLights, "uPointLights[0].position");
    rGetUniformArrayLocations(arrBegin, lightText, m_PointLightUniforms.lColors     , NumLights, "uPointLights[0].color");
    rGetUniformArrayLocations(arrBegin, lightText, m_PointLightUniforms.lDirections , NumLights, "uPointLights[0].direction");
    rGetUniformArrayLocations(arrBegin, lightText, m_PointLightUniforms.lIntensities, NumLights, "uPointLights[0].intensity");
    rGetUniformArrayLocations(arrBegin, lightText, m_PointLightUniforms.lCutoffs    , NumLights, "uPointLights[0].cutoff");
    rGetUniformArrayLocations(arrBegin, lightText, m_PointLightUniforms.lRanges     , NumLights, "uPointLights[0].range");
    
    arrBegin = sizeof("uSpotLights");
    rGetUniformArrayLocations(arrBegin, lightText, m_SpotLightUniforms.lPositions  , NumLights, "uSpotLights[0].position");
    rGetUniformArrayLocations(arrBegin, lightText, m_SpotLightUniforms.lColors     , NumLights, "uSpotLights[0].color");
    rGetUniformArrayLocations(arrBegin, lightText, m_SpotLightUniforms.lDirections , NumLights, "uSpotLights[0].direction");
    rGetUniformArrayLocations(arrBegin, lightText, m_SpotLightUniforms.lIntensities, NumLights, "uSpotLights[0].intensity");
    rGetUniformArrayLocations(arrBegin, lightText, m_SpotLightUniforms.lCutoffs    , NumLights, "uSpotLights[0].cutoff");
    rGetUniformArrayLocations(arrBegin, lightText, m_SpotLightUniforms.lRanges     , NumLights, "uSpotLights[0].range");
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
    
    // Compile alpha cutoff version of the gbuffer shader, I really don't want an branch for discard statement. to make sure early z test
    int alphaIndex = StringContains(gbufferFragmentShader.text, "ALPHA_CUTOFF");
    ASSERT(alphaIndex != -1);
    gbufferFragmentShader.text[alphaIndex + sizeof("ALPHA_CUTOFF")] = '1';
    m_GBufferShaderAlpha = rCreateShader(gbufferVertexShader.text, gbufferFragmentShader.text);

    GetUniformLocations();
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
    const char* vertexShaderSource =
    AX_SHADER_VERSION_PRECISION()
    R"(
        layout(location = 0) in vec3 aPosition;
        uniform mat4 model, lightMatrix;
        
        void main() {
            gl_Position = model * lightMatrix * vec4(aPosition, 1.0);
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

void Init()
{
    CreateSkyTexture();
    CreateShaders();

    Vector2i windowStartSize;
    wGetMonitorSize(&windowStartSize.x, &windowStartSize.y);
    
    CreateFrameBuffers(windowStartSize.x, windowStartSize.y);
    
    m_Camera.Init(windowStartSize);
    wSetWindowResizeCallback(WindowResizeCallback);

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
    rClearDepth();

    // looks like a skybox
    {
        rSetDepthTest(false);
        rRenderFullScreen(m_SkyTexture.handle);
        rSetDepthTest(true);
    }
    
    m_Camera.Update();
    rBindShader(m_GBufferShader);
   
    // shadow uniforms
    rSetShaderValue(m_LightMatrix.GetPtr(), lLightMatrix, GraphicType_Matrix4);

    rSetTexture(m_ShadowTexture, 3, lShadowMap);
}

static void RenderShadowOfNode(ANode* node, Prefab* prefab, Matrix4 parentMat)
{
    Matrix4 model = parentMat * Matrix4::PositionRotationScale(node->translation, node->rotation, node->scale);
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

static void RenderShadows(Prefab* prefab, DirectionalLight& sunLight)
{
    rBindShader(m_ShadowShader);
    rBindFrameBuffer(m_ShadowFrameBuffer);
    
    rBeginShadow();
    rClearDepth();

    rSetViewportSize(ShadowSettings::ShadowMapSize, ShadowSettings::ShadowMapSize);
    
    Matrix4 view  = Matrix4::LookAtRH(sunLight.dir * 50.0f + m_Camera.position, -sunLight.dir, Vector3f::Up());
    Matrix4 ortho = ShadowSettings::GetOrthoMatrix();
    m_LightMatrix = view * ortho;

    rSetShaderValue(m_LightMatrix.GetPtr(), lShadowLightMatrix, GraphicType_Matrix4);
    
    bool hasScene = prefab->numScenes > 0;
    
    if (!hasScene)
    {
        Matrix4 model = Matrix4::CreateScale(prefab->scale);
        rSetShaderValue(model.GetPtr(), lShadowModel, GraphicType_Matrix4);
        rRenderMesh(prefab->bigMesh); // render all scene with one draw call
    }
    else
    {
        AScene defaultScene = prefab->scenes[prefab->defaultSceneIndex];
        int numNodes = defaultScene.numNodes;
        for (int i = 0; i < numNodes; i++)
        {
            rBindMesh(prefab->bigMesh);
            RenderShadowOfNode(&prefab->nodes[defaultScene.nodes[i]], prefab, Matrix4::Identity());
        }
    }

    rEndShadow();
    rUnbindFrameBuffer();

    Vector2i windowSize;
    wGetWindowSize(&windowSize.x, &windowSize.y);
    rSetViewportSize(windowSize.x, windowSize.y);
    rBindFrameBuffer(m_MainFrameBuffer.Buffer);
}

static void RenderPrimitive(AMaterial& material, Prefab* prefab, APrimitive& primitive)
{
    int baseColorIndex = material.baseColorTexture.index;
    if (prefab->numTextures > 0 && baseColorIndex != -1)
        rSetTexture(prefab->textures[baseColorIndex], 0, lAlbedo);
    
    int normalIndex  = material.GetNormalTexture().index;
    int hasNormalMap = EnumHasBit(primitive.attributes, AAttribType_TANGENT) && normalIndex != -1;
    
    if (prefab->numTextures > 0 && hasNormalMap)
        rSetTexture(prefab->textures[normalIndex], 1, lNormalMap);
    
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
    rRenderMeshIndexOffset(prefab->bigMesh, primitive.numIndices, offset);
}

static void RecurseNodeMatrices(ANode* node, ANode* nodes, Matrix4 parentMatrix, Matrix4* matrices)
{
    for (int c = 0; c < node->numChildren; c++)
    {
        int childIndex = node->children[c];
        ANode* children = &nodes[childIndex];
        Matrix4 childTranslation = Matrix4::PositionRotationScale(children->translation, children->rotation, children->scale) * parentMatrix;
        matrices[childIndex] = childTranslation;

        RecurseNodeMatrices(children, nodes, childTranslation, matrices);
    }
}

static void EvaluateAnim(Prefab* prefab, int animIndex, float animTime)
{
    ASkin& skin = prefab->skins[0];
    AAnimation& animation = prefab->animations[0];
    ASSERT(skin.numJoints < 32);

    for (int i = 0; i < skin.numJoints; i++)
    {
        rSetShaderValue(skin.inverseBindMatrices + (i * 16), lInvBindMatrices[i], GraphicType_Matrix4);
    }

    for (int c = 0; c < animation.numChannels; c++)
    {
        AAnimChannel& channel = animation.channels[c];
        ANode* targetNode = &prefab->nodes[channel.targetNode];
        AAnimSampler& sampler = animation.samplers[channel.sampler];

        float realTime = FMod(animTime, animation.duration);
        
        int beginIdx = 0;
        while (realTime > sampler.input[beginIdx + 1])
            beginIdx++;
        
        vec_t begin = ((vec_t*)sampler.output)[beginIdx]; 
        vec_t end   = ((vec_t*)sampler.output)[beginIdx + 1];
        float t = (realTime - sampler.input[beginIdx]) / MAX(sampler.input[beginIdx + 1] - sampler.input[beginIdx], 0.0001f);
        t = Clamp(t, 0.0f, 1.0f);

        switch (channel.targetPath)
        {
            case AAnimTargetPath_Scale:
                Vec3Store(targetNode->scale, VecLerp(begin, end, t));
                break;
            case AAnimTargetPath_Translation:
                Vec3Store(targetNode->translation, VecLerp(begin, end, t));
                break;
            case AAnimTargetPath_Rotation:
                vec_t rot = QSlerp(begin, end, t);
                VecStore(targetNode->rotation, rot);
                ASSERT(Abs(1.0f - VecLenf(rot)) < 0.01f); // is normalized ?
                break;
        };
    }
    Matrix4 jointMatrices[32];

    if (skin.skeleton == -1)
    {
        ASSERT(prefab->numNodes < 64);
        uint64_t isChildren = 0ull;

        for (int n = 0; n < prefab->numNodes; n++)
        {
            for (int c = 0; c < prefab->nodes[n].numChildren; c++)
            { 
                int children = prefab->nodes[n].children[c];
                isChildren |= (1ull << children);
            }
        }
    
        uint64_t mask = ~0ull >> (64 - prefab->numNodes);
        uint64_t rootNodes = mask & ~isChildren;

        uint64_t rootIndex = 63ull - LeadingZeroCount(rootNodes);
        rootNodes >>= rootIndex;

        for (/**/; rootNodes > 0; rootIndex += NextSetBit(&rootNodes))
        {
            ANode* rootNode = &prefab->nodes[rootIndex];
            Matrix4 rootMatrix = Matrix4::PositionRotationScale(rootNode->translation, rootNode->rotation, rootNode->scale);
            jointMatrices[rootIndex] = rootMatrix;
            RecurseNodeMatrices(rootNode, prefab->nodes, rootMatrix, jointMatrices);
        }
    }
    else
    {
        ANode* rootNode = &prefab->nodes[skin.skeleton];
        Matrix4 rootMatrix = Matrix4::PositionRotationScale(rootNode->translation, rootNode->rotation, rootNode->scale);
        jointMatrices[skin.skeleton] = rootMatrix;
        RecurseNodeMatrices(rootNode, prefab->nodes, rootMatrix, jointMatrices);
    }

    Matrix4* invMatrices = (Matrix4*)skin.inverseBindMatrices;
    for (int i = 0; i < skin.numJoints; i++)
    {
        Matrix4 mat = invMatrices[i] * jointMatrices[skin.joints[i]];
        rSetShaderValue(mat.GetPtr(), lJointMatrices[i], GraphicType_Matrix4);
    }
}

static void RenderNodeRec(ANode& node, Prefab* prefab, Matrix4 parentMat, Matrix4 viewProjection, bool recurse, bool isAlpha)
{
    Matrix4 model = parentMat * Matrix4::PositionRotationScale(node.translation, node.rotation, node.scale);

    // if node is not mesh skip (camera or empty node)
    if (node.type != 0 || node.index == -1)
    {
        for (int i = 0; i < node.numChildren && recurse; i++)
        {
            RenderNodeRec(prefab->nodes[node.children[i]], prefab, model, viewProjection, recurse, isAlpha);
        }
        return;
    }
    
    rSetShaderValue(model.GetPtr(), lModel, GraphicType_Matrix4);
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
        shouldDraw &= primitive.numIndices != 0;
    
        // if (!isAlpha && material.alphaMode == AMaterialAlphaMode_Mask) { 
        //     m_DelayedAlphaCutoffs.EmplaceBack(&node, mesh.primitives + j);
        //     //shouldDraw = false;
        // }
    
        if (shouldDraw) {
            // SetMaterial(&material);
            RenderPrimitive(material, prefab, primitive);
        }

        for (int i = 0; i < node.numChildren && recurse; i++)
        {
            RenderNodeRec(prefab->nodes[node.children[i]], prefab, model, viewProjection, recurse, isAlpha);
        }
    }
}

// Renders to gbuffer
void RenderPrefab(Scene* scene, PrefabID prefabID, int animIndex, float animTime)
{
    Prefab* prefab = scene->GetPrefab(prefabID);
    DirectionalLight sunLight = scene->m_SunLight;

    // todo: fix this
    {
        static int first = 1; // render shadows only once if not dynamic
        if (first == 1) 
            if (IsAndroid()) RenderShadows(prefab, sunLight);

        if (!IsAndroid()) 
            RenderShadows(prefab, sunLight); // realtime shadows

        first = 0;
    }
   
    rBindShader(m_GBufferShader);
    rSetShaderValue(&sunLight.dir.x, lSunDirG, GraphicType_Vector3f);
    int skined = (int)(prefab->numSkins > 0);
    rSetShaderValue(skined, lHasSkin);
    if (skined)
    {
        EvaluateAnim(prefab, animIndex, animTime);
    }

    rBindMesh(prefab->bigMesh);

    Matrix4 viewProjection = m_Camera.view * m_Camera.projection;
    rSetShaderValue(viewProjection.GetPtr(), lViewProj, GraphicType_Matrix4);

    int numNodes  = prefab->numNodes;
    bool hasScene = prefab->numScenes > 0;
    AScene defaultScene;
    if (hasScene)
    {
        defaultScene = prefab->scenes[prefab->defaultSceneIndex];
        numNodes = defaultScene.numNodes;
    }
   
    for (int i = 0; i < numNodes; i++)
    {
        ANode& node = hasScene ? prefab->nodes[defaultScene.nodes[i]] : prefab->nodes[i];
        bool shouldRecurse = hasScene, isAlpha = false;
        RenderNodeRec(node, prefab, Matrix4::Identity(), viewProjection, shouldRecurse, isAlpha);
    }

    // Render alpha masked meshes after opaque meshes, to make sure performance is good. (early z)
    #if 0
    if (false) //(m_DelayedAlphaCutoffs.Size() > 0)
    {
        rBindShader(m_GBufferShaderAlpha);
        // shadow uniforms
        rSetShaderValue(m_LightMatrix.GetPtr(), lLightMatrix, GraphicType_Matrix4);
        rSetTexture(m_ShadowTexture, 3, lShadowMap);

        QuickSort<KeyValuePair<ANode*, APrimitive*>>(m_DelayedAlphaCutoffs.Data(), 0, m_DelayedAlphaCutoffs.Size() - 1);
        // todo we need parent matrix instead of primitive pointer
        for (int i = 0; i < m_DelayedAlphaCutoffs.Size(); i++)
        {
            KeyValuePair<ANode*, APrimitive*> alphaCutoff = m_DelayedAlphaCutoffs[i];
            bool recurse = false, isAlpha = true;
            RenderNodeRec(*alphaCutoff.key, prefab, Matrix4::Identity(), viewProjection, recurse, isAlpha);
        }
        m_DelayedAlphaCutoffs.Resize(0);
    }
    #endif
    rDrawAllLines(viewProjection.GetPtr());
}

static void SSAOPass()
{
    // Downsample main frame buffer
    rBindFrameBuffer(m_MainFrameBufferHalf.Buffer);
    rSetViewportSize(m_MainFrameBufferHalf.width, m_MainFrameBufferHalf.height);
    {
        rClearDepth();
        rBindShader(m_MainFrameBufferCopyShader);

        rSetTexture(m_MainFrameBuffer.ColorTexture , 0, dColorTex);
        rSetTexture(m_MainFrameBuffer.NormalTexture, 1, dNormalTex);
        rSetTexture(m_MainFrameBuffer.DepthTexture , 2, dDepthTex);
        rRenderFullScreen();
    }

    rSetDepthTest(false);
    rSetDepthWrite(false);

    // SSAO pass
    rBindFrameBuffer(m_SSAOFrameBuffer);
    rFrameBufferAttachColor(m_SSAOHalfTexture, 0);
    rBindShader(m_SSAOShader);
    {
        rSetTexture(m_MainFrameBufferHalf.DepthTexture , 0, sDepthMap); // m_MainFrameBufferHalf.DepthTexture
        rSetTexture(m_MainFrameBufferHalf.NormalTexture, 1, sNormalTex);
        rSetShaderValue(m_Camera.view.GetPtr(), sView, GraphicType_Matrix4);
        
        rRenderFullScreen();
    }
    // Upsample SSAO
    rSetViewportSize(m_MainFrameBuffer.width, m_MainFrameBuffer.height);
    rFrameBufferAttachColor(m_SSAOTexture, 0);
    rBindShader(m_RedUpsampleShader);
    {
        rSetTexture(m_SSAOHalfTexture, 0, rGetUniformLocation("halfTex"));
        rRenderFullScreen();
    }
}

static void LightingPass()
{
    DirectionalLight sunLight = g_CurrentScene.m_SunLight;

    rBindShader(m_DeferredPBRShader);
    {
        Matrix4 invView = Matrix4::Inverse(m_Camera.view);
        Matrix4 invProj = Matrix4::Inverse(m_Camera.projection);

        rSetShaderValue(&sunLight.dir.x, lSunDir, GraphicType_Vector3f);
    
        rSetShaderValue(invView.GetPtr(), lInvView, GraphicType_Matrix4);
        rSetShaderValue(invProj.GetPtr(), lInvProj, GraphicType_Matrix4);
    
        rSetTexture(m_MainFrameBuffer.ColorTexture              , 0, lAlbedoTex);
        rSetTexture(m_MainFrameBuffer.ShadowMetallicRoughnessTex, 1, lShadowMetallicRoughnessTex);
        rSetTexture(m_MainFrameBuffer.NormalTexture             , 2, lNormalTex);
        rSetTexture(m_MainFrameBuffer.DepthTexture              , 3, lDepthMap);
        rSetTexture(m_SSAOTexture                               , 4, lAmbientOclussionTex);
    }

    rUnbindFrameBuffer(); // < draw to backbuffer after this line
    rSetViewportSize(m_MainFrameBuffer.width, m_MainFrameBuffer.height);
    {
        rSetShaderValue(g_CurrentScene.m_PointLights.Size(), lNumPointLights);
        rSetShaderValue(g_CurrentScene.m_SpotLights.Size(), lNumSpotLights);
        rRenderFullScreen();
    }

    rBindFrameBuffer(m_MainFrameBuffer.Buffer);
    rFrameBufferInvalidate(3); // color, normal, ShadowMetallicRoughness

    rSetDepthTest(true);
    rSetDepthWrite(true);
}

void EndRendering()
{
    SSAOPass();
    
    LightingPass();
}

void Destroy()
{
    DeleteFrameBuffers();

    DeleteShaders();

    rDeleteTexture(m_ShadowTexture);
    rDeleteFrameBuffer(m_ShadowFrameBuffer);
}

} // scene renderer namespace endndndnd