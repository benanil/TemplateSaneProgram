
#include "include/Renderer.hpp"
#include "include/Platform.hpp"
#include "include/UI.hpp"
#include "include/Camera.hpp"

#include "../ASTL/Math/Matrix.hpp"
#include "../ASTL/IO.hpp"
#include <math.h>
#include <stdio.h>

struct TreeVertex
{
    Vector3f position;
    half2 uv;
    Vector3f normal;
    float padding;
};

static TreeVertex* mTreeVertices = nullptr;
static uint* mTreeIndices = nullptr;
static GPUMesh mTreeMesh;

// From Texture.cpp
extern void CompressSaveImages(char* path, const char** images, int numImages);
extern void LoadSceneImages(char* path, Texture* textures, int numImages);

static Texture mLayers[3 * 3];

static Shader mTerrainShader;
static Shader mHeightShader;
static Shader mMoveShader;
static Shader mCalculateNormalShader;
static Shader mGrassShader;
static Shader mTreeShader;

static Texture mTreeLogTextures[3]; // diffuse, (ao,metallic,roughness), normal

static Texture mHeightTexture;
static Texture mNormalTexture;
// double buffering
static Texture mHeightTexture1;
static Texture mNormalTexture1;

static Texture mTestTexture2d;
static Texture mGreyNoiseTexture;

static FrameBuffer mHeightFrameBuffer;
static FrameBuffer mFrameBuffer2D;
static bool mFirstInit = true;

static Vector2i mChunkOffset = { 0, 0 };

static bool mShuldUpdate = false;

// Tree
static uint mTreeVertexOffset;
static uint mTreeIndexOffset;

// Constants
static const int mNumQuads = 64; // on x and z axis  (512/8)
static const int mChunkNumSegments = 64;
static const float mQuadSize   = 20.0f; // < in meters. each segment has (chunkSize / numSegment) width and height
static const float mChunkSize  = mQuadSize * (float)mNumQuads;
static const float mOffsetSize = mChunkSize / 8.0; // < in meters. each segment has (chunkSize / numSegment) width and height

static void DeleteShaders()
{
    rDeleteShader(mTerrainShader);
    rDeleteShader(mHeightShader);
    rDeleteShader(mMoveShader);
    rDeleteShader(mCalculateNormalShader);
    rDeleteShader(mGrassShader);
    rDeleteShader(mTreeShader);
}

static void LoadTerrainLayerTextures()
{
    const char* images[] = {
        "Assets/Textures/Terrain/brown_mud_leaves_01_arm_2k.png",     // mLayers[0].AORoughnessMetallic
        "Assets/Textures/Terrain/brown_mud_leaves_01_diff_2k.png",    // mLayers[0].Diffuse            
        "Assets/Textures/Terrain/brown_mud_leaves_01_nor_dx_1k.png",  // mLayers[0].Normal             
        // 
        "Assets/Textures/Terrain/rocky_terrain_02_arm_1k.png",        // mLayers[1].AORoughnessMetallic
        "Assets/Textures/Terrain/rocky_terrain_02_diff_2k.png",       // mLayers[1].Diffuse            
        "Assets/Textures/Terrain/rocky_terrain_02_nor_dx_1k.png",     // mLayers[1].Normal             
        // 
        "Assets/Textures/Terrain/rocky_terrain_arm_1k.png",           // mLayers[2].AORoughnessMetallic
        "Assets/Textures/Terrain/rocky_terrain_diff_2k.png",          // mLayers[2].Diffuse            
        "Assets/Textures/Terrain/rocky_terrain_nor_dx_1k.png"         // mLayers[2].Normal             
    };

    char path[512] = "Assets/Textures/Terrain/Compressed.dxt";

    if (!FileExist(path)) 
        CompressSaveImages(path, images, ArraySize(images));

    LoadSceneImages(path, mLayers, ArraySize(images));
}

static void LoadTreeTextures()
{
    const char* images[] = {
        "Assets/Textures/Tree/pine_bark_arm_1k.png",    
        "Assets/Textures/Tree/pine_bark_diff_1k.png",   
        "Assets/Textures/Tree/pine_bark_nor_gl_1k.png"
    };

    char path[512] = "Assets/Textures/Tree/LogCompressed.dxt";

    if (!FileExist(path)) 
        CompressSaveImages(path, images, ArraySize(images));

    LoadSceneImages(path, mTreeLogTextures, ArraySize(images));
}

void TerrainCreateShaders()
{
    if (mFirstInit == false) {
        DeleteShaders();
    }
    else {
        mGreyNoiseTexture = rImportTexture("Assets/Textures/ShadertoyGreyNoise.png");
        LoadTerrainLayerTextures();
        LoadTreeTextures();
    }

    rImportFullScreenShaderSafe("Assets/Shaders/PerlinNoise.glsl", &mHeightShader);
    rImportFullScreenShaderSafe("Assets/Shaders/MovePixels.glsl", &mMoveShader);
    rImportFullScreenShaderSafe("Assets/Shaders/TerrainGenNormals.glsl", &mCalculateNormalShader);
    
    rImportShaderSafe("Assets/Shaders/TreeVert.glsl" , "Assets/Shaders/TreeFrag.glsl", nullptr, &mTreeShader);
    rImportShaderSafe("Assets/Shaders/EmptyVert.glsl", "Assets/Shaders/TerrainFrag.glsl", "Assets/Shaders/TerrainGeom.glsl", &mTerrainShader);
    rImportShaderSafe("Assets/Shaders/EmptyVert.glsl", "Assets/Shaders/GrassFrag.glsl", "Assets/Shaders/GrassGeom.glsl", &mGrassShader);
}

static void CreateTextures() 
{
    if (mFirstInit == false) return; 
    
    mHeightTexture  = rCreateTexture(512, 512, nullptr, TextureType_R16F , TexFlags_RawData);
    mHeightTexture1 = rCreateTexture(512, 512, nullptr, TextureType_R16F , TexFlags_RawData);
    mNormalTexture  = rCreateTexture(512, 512, nullptr, TextureType_RGBA8, TexFlags_RawData);
    mNormalTexture1 = rCreateTexture(512, 512, nullptr, TextureType_RGBA8, TexFlags_RawData);

    mTestTexture2d     = rCreateTexture(512, 512, nullptr, TextureType_R8, TexFlags_RawData);
    mHeightFrameBuffer = rCreateFrameBuffer(true);
}

static void CylinderDraw(
    float numHorSegments, // = 16.0f
    float numVerSegments, // = 6.0f (min 2.0)
    float treeHeight,     // = 10.0f
    float logWidth,       // = 1.0
    float heightStart,
    bool  reuseVertex
)
{
    const uint uNumHorSegments = uint(numHorSegments);
    const uint uNumVerSegments = uint(numVerSegments);
    uint uReuseVertex = uint(reuseVertex);

    uint numVertex = (uNumHorSegments + 1u) * (uNumVerSegments - uReuseVertex);
    uint numIndex  = (uNumVerSegments-1u) * uNumHorSegments * 6u;

    TreeVertex* vertices = mTreeVertices + mTreeVertexOffset;
    uint* indices  = mTreeIndices + mTreeIndexOffset; 

    uint indexOffset  = 0u;
    uint vertexOffset = 0u;

    for (uint v = 0u + uReuseVertex; v < uNumVerSegments; v++)
    {
        float vt = float(v) / (numVerSegments - 1.0f);
        half heightf16 = ConvertFloatToHalf(vt * treeHeight);

        for (uint h = 0u; h < uNumHorSegments + 1u; h++)
        {
            float ht = float(h) / numHorSegments;
            float sn = Sin(ht * TwoPI);
            float cs = Cos(ht * TwoPI);
        
            vertices[vertexOffset + h].position = { sn * logWidth, heightStart + vt * treeHeight, cs * logWidth };
            vertices[vertexOffset + h].uv       = MakeHalf2(ConvertFloatToHalf(ht), heightf16);
            vertices[vertexOffset + h].normal   = { sn, 0.0f, cs };
        }
        vertexOffset += uNumHorSegments + 1u;
    }

    indexOffset  = 0u;
    vertexOffset = mTreeVertexOffset - uReuseVertex * (uNumHorSegments + 1u);
    for (uint v = 0u; v < uNumVerSegments-1u; v++)
    {
        for (uint h = 0u; h < uNumHorSegments; h++, indexOffset += 6u)
        {
            indices[indexOffset + 0u] = h + vertexOffset + 1u;
            indices[indexOffset + 1u] = h + vertexOffset + uNumHorSegments + 1u;
            indices[indexOffset + 2u] = h + vertexOffset;
                                    
            indices[indexOffset + 3u] = h + vertexOffset + 1u;
            indices[indexOffset + 4u] = h + vertexOffset + uNumHorSegments + 2u;
            indices[indexOffset + 5u] = h + vertexOffset + uNumHorSegments + 1u;
        }
        vertexOffset += uNumHorSegments + 1u;
    }

    mTreeVertexOffset += numVertex;
    mTreeIndexOffset += numIndex;
}

static void CreateTrees()
{
    float numHorSegments = 16.0f;
    float numVerSegments = 6.0f; 
    float treeHeight     = 3.0f;
    float logWidth       = 1.0f;

    uint numVertex = uint(numHorSegments + 1u) * (uint)numVerSegments;
    uint numIndex  = uint(numVerSegments-1u) * (uint)numHorSegments * 6u;

    mTreeVertices = new TreeVertex[numVertex];
    mTreeIndices = new uint[numIndex];

    CylinderDraw(
        numHorSegments,
        2.0f, // numVerSegments
        treeHeight,
        logWidth,
        0.0f, // heightStart
        false
    );

    CylinderDraw(
        numHorSegments,
        2.0f, // numVerSegments
        treeHeight,
        logWidth * 1.2f,
        3.0f, // heightStart
        true
    );

    const InputLayout layout[] = {
        {3, GraphicType_Float },
        {2, GraphicType_Half  },
        {3, GraphicType_Float }
    };
    InputLayoutDesc inputDesc;
    inputDesc.numLayout = ArraySize(layout);
    inputDesc.stride    = sizeof(TreeVertex);
    inputDesc.layout    = layout;
    inputDesc.dynamic   = false;
    mTreeMesh = rCreateMesh(mTreeVertices, mTreeIndices, numVertex, numIndex, GraphicType_UnsignedInt, &inputDesc);
}

static void DestroyTrees()
{
    delete[] mTreeVertices;
    delete[] mTreeIndices;
}

inline float fract(float x) {
    return x - floorf(x);
}

inline float mix(float a, float b, float t) {
    return a * (1.0f - t) + b * t;
}

static float noise(Vector2f p) 
{
    Vector2f f = { fract(p.x), fract(p.y) };
    p = { floorf(p.x), floorf(p.y) };
	float v = p.x + p.y * 1000.0f;
    xyzw r = { v, v + 1.0f, v + 1000.0f, v + 1001.0f };
	r.x = fract(10000.0f * sinf(r.x * .001f));
	r.y = fract(10000.0f * sinf(r.y * .001f));
	r.z = fract(10000.0f * sinf(r.z * .001f));
	r.w = fract(10000.0f * sinf(r.w * .001f));
    f.x = f.x * f.x * (3.0f - 2.0f * f.x);
    f.y = f.y * f.y * (3.0f - 2.0f * f.y);
	return 2.0f * (mix(mix(r.x, r.y, f.x), mix(r.z, r.w, f.x), f.y))-1.0f;
}

//generate terrain using above noise algorithm
static float terrain(Vector2f p, int freq, float h, float w, float m) 
{	
    for (int i = 0; i < freq; i++) {
		h += w * noise((p * m)); // adjust height based on noise algorithm
		w *= 0.5f;
		m *= 2.0f;
	}
	return h;
}

static Vector3f characterPos;
float GetTerrainHeight(Vector3f position) 
{
    characterPos = position;
    position += mChunkSize * 0.5f; // start from center
    position /= (float)mNumQuads * mQuadSize;
    position *= 20.0f;
    
    const Vector2f seed = { 5.0f, 5.0f };
    Vector2f pos2 = Vec2(position.x - seed.x, position.z - seed.y);
    float height = Clamp(terrain(pos2, 8, 0.250f, 0.5f, 0.250f), 0.00f, 1.0f);
    height      += Clamp(terrain(pos2, 8, 0.198f, 1.6f, 0.210f),-0.16f, 2.7f);
    return height * 36.0f;
}

enum eMoveMask_
{
    eMove_Hor    = 1,
    eMove_Ver    = 2,
    eMove_HorNeg = 4, // negative
    eMove_VerNeg = 8  // negative
};

typedef uint eMoveMask;

static void GenerateHeightTexture(eMoveMask move)
{
    if (false) // (mFirstInit || GetKeyPressed('T'))
    {
        uint8 buffer[512][512];
        Vector2f startPos = ToVector2f(mChunkOffset) * mOffsetSize;
        Vector2f targetPos = Vec2(characterPos.x, characterPos.z) + (mChunkSize * 0.5f);
        float segmentSize = mQuadSize / 8.0f;
        
        #pragma omp parallel for
        for (int i = 0; i < 512; i++)
        {
            for (int j = 0; j < 512; j++)
            {
                Vector2f pos = startPos;
                pos.x += segmentSize * j;
                pos.y += segmentSize * i;

                bool isCloser = Vector2f::DistanceSq(pos, targetPos) < 600.0f;
                buffer[i][j] = (uint8)(GetTerrainHeight(Vec3(pos.x, 0.0f, pos.y)) / 36.0f / 3.7f * 255.0f);
                if (isCloser) buffer[i][j] = 0;
            }
        }
        rUpdateTexture(mTestTexture2d, buffer);
    }

    Vector2i moveSize     = { 512, 512 };
    Vector2i renderSize   = { 512, 512 };
    Vector2i moveOffset   = { 0, 0 };
    Vector2i renderOffset = { 0, 0 };
    Vector2i moveDir      = { 0, 0 };

    if (move > 0) // has offset
    {
        bool isHorizontal = !!(move & eMove_Hor);
        bool isVertical = !!(move & eMove_Ver);
        bool verNeg = !!(move & eMove_VerNeg);
        bool horNeg = !!(move & eMove_HorNeg);

        moveSize.x = isHorizontal ? 512 - 64 : 512;
        moveSize.y = isHorizontal ? 512 : 512 - 64;

        moveOffset.x = isHorizontal && horNeg ? 64 : 0; // 64 if negative otherwise 0
        moveOffset.y = isVertical   && verNeg ? 64 : 0; // 64 if negative otherwise 0

        renderSize.x = isHorizontal ? 64 : 512;
        renderSize.y = isVertical ? 64 : 512;
        
        renderOffset.x = isHorizontal && horNeg == false ? 512 - 64 : 0; // 512 - 64 if positive 
        renderOffset.y = isVertical   && verNeg == false ? 512 - 64 : 0; // 512 - 64 if positive 
        
        moveDir.x = isHorizontal;
        moveDir.y = isVertical;
        if (isHorizontal && horNeg) moveDir.x *= -1; // negate if negative
        if (isVertical && verNeg)   moveDir.y *= -1; // negate if negative
    }

    rBindFrameBuffer(mHeightFrameBuffer);

    rBindShader(mHeightShader);
    rSetShaderValue(mChunkOffset.arr, rGetUniformLocation("mChunkOffset"), GraphicType_Vector2i);
    rSetShaderValue(moveDir.arr     , rGetUniformLocation("mMoveDir"), GraphicType_Vector2i);
    
    rFrameBufferAttachColor(mHeightTexture);
    if (move > 0) {
        rSetViewportSizeAndOffset(moveSize, Max(moveOffset, Vec2(0)));
        rFrameBufferAttachColor(mHeightTexture1);
        rBindShader(mMoveShader);
        rSetShaderValue(moveDir.arr, rGetUniformLocation("mMoveDir"), GraphicType_Vector2i);
        rSetTexture(mHeightTexture, 0, rGetUniformLocation("mSource"));
        rRenderFullScreen();
        rBindShader(mHeightShader);
        Swap(mHeightTexture, mHeightTexture1);
    }
    rSetViewportSizeAndOffset(renderSize, renderOffset);
    rRenderFullScreen();
    
    rFrameBufferAttachColor(mNormalTexture);
    rBindShader(mCalculateNormalShader);
    rSetTexture(mHeightTexture, 0, rGetUniformLocation("mPerlinNoise"));

    // if (move > 0) {
    //     rSetViewportSizeAndOffset(moveSize, Max(moveOffset, Vec2(0)));
    //     rFrameBufferAttachColor(mNormalTexture1);
    //     rBindShader(mMoveShader);
    //     rSetShaderValue(moveDir.arr, rGetUniformLocation("mMoveDir"), GraphicType_Vector2i);
    //     rSetTexture(mNormalTexture, 0, rGetUniformLocation("mSource"));
    //     rRenderFullScreen();
    //     rBindShader(mCalculateNormalShader);
    //     Swap(mNormalTexture1, mNormalTexture);
    // }
 
    rSetViewportSizeAndOffset(Vec2(512), Vector2i::Zero());
    rRenderFullScreen();
}

void InitTerrain()
{
    TerrainCreateShaders();
    CreateTextures();
    CreateTrees();
    GenerateHeightTexture(0);

    mFirstInit = false;
    mShuldUpdate = false;
}

void UpdateTerrain(CameraBase* camera)
{
    if (GetKeyPressed('T') || mShuldUpdate) InitTerrain();
    mShuldUpdate |= GetKeyPressed('L');

    Vector2i oldOffset = mChunkOffset;
    float halfOffset = mOffsetSize * 0.5f;
    
    mChunkOffset ={
        int((camera->position.x + camera->Front.x * halfOffset) / mOffsetSize), 
        int((camera->position.z + camera->Front.z * halfOffset) / mOffsetSize)
    };
    
    if (mChunkOffset.x > oldOffset.x) { GenerateHeightTexture(eMove_Hor); }
    if (mChunkOffset.x < oldOffset.x) { GenerateHeightTexture(eMove_Hor | eMove_HorNeg); }
    if (mChunkOffset.y > oldOffset.y) { GenerateHeightTexture(eMove_Ver ); }
    if (mChunkOffset.y < oldOffset.y) { GenerateHeightTexture(eMove_Ver| eMove_VerNeg); }
}

static void SetCameraUniforms(CameraBase* camera, Matrix4 viewProj)
{
    Vector3f cameraDir = Vector3f::Cross(camera->Right, Vector3f::Up());
    rSetShaderValue(mChunkOffset.arr    , rGetUniformLocation("mChunkOffset"), GraphicType_Vector2i);
    rSetShaderValue(cameraDir.arr       , rGetUniformLocation("mCameraDir"), GraphicType_Vector3f);
    rSetShaderValue(camera->position.arr, rGetUniformLocation("mCameraPos"), GraphicType_Vector3f);
    rSetShaderValue(viewProj.GetPtr()   , rGetUniformLocation("mViewProj") , GraphicType_Matrix4);
}

void RenderTerrain(CameraBase* camera)
{
    if (GetKeyPressed('T')) camera->position = characterPos;

    // uSprite(Vec2( 30.0f, 700.0f), Vec2(300.0f), &mNormalTexture);
    // uSprite(Vec2(330.0f, 700.0f), Vec2(300.0f), &mNormalTexture1);
    Matrix4 viewProj = camera->view * camera->projection;
    rBindShader(mTerrainShader);
    rSetTexture(mHeightTexture, 0, rGetUniformLocation("mPerlinNoise"));
    rSetTexture(mNormalTexture, 1, rGetUniformLocation("mNormalTex"));

    rSetTexture(mLayers[0], 2, rGetUniformLocation("mLayer0ARM"));
    rSetTexture(mLayers[1], 3, rGetUniformLocation("mLayer0Diff"));

    rSetTexture(mLayers[3], 4, rGetUniformLocation("mLayer1ARM"));
    rSetTexture(mLayers[4], 5, rGetUniformLocation("mLayer1Diff"));
    
    rSetTexture(mLayers[6], 6, rGetUniformLocation("mLayer2ARM"));
    rSetTexture(mLayers[7], 7, rGetUniformLocation("mLayer2Diff"));
    
    rSetTexture(mGreyNoiseTexture, 8, rGetUniformLocation("mGrayNoise"));

    SetCameraUniforms(camera, viewProj);
    rRenderGeomPoint(64 * 64);

    rBindShader(mGrassShader);
    rSetTexture(mHeightTexture, 0, rGetUniformLocation("mPerlinNoise"));
    rSetTexture(mNormalTexture, 1, rGetUniformLocation("mNormalTex"));
    rSetShaderValue((float)TimeSinceStartup(), rGetUniformLocation("mTime"));
    SetCameraUniforms(camera, viewProj);
    rRenderGeomPoint(512 * 512);

    if (false)
    {
        rBindShader(mTreeShader);
        rBindMesh(mTreeMesh);
        Matrix4 model = Matrix4::Identity();
        // we will use instancing!
        rSetShaderValue(model.GetPtr(), rGetUniformLocation("uModel"), GraphicType_Matrix4);
        rSetShaderValue(viewProj.GetPtr(), rGetUniformLocation("uViewProj"), GraphicType_Matrix4);
        rSetTexture(mTreeLogTextures[0], 0, rGetUniformLocation("uBarkAORoughnessMetallic"));
        rSetTexture(mTreeLogTextures[1], 1, rGetUniformLocation("uBarkDiffuse"));
        rSetTexture(mTreeLogTextures[2], 2, rGetUniformLocation("uBarkNormal"));
        rRenderMeshIndexed(mTreeMesh);
    }
}

void TerrainDestroy()
{
    rDeleteTexture(mHeightTexture); 
    rDeleteTexture(mHeightTexture1);
    rDeleteTexture(mNormalTexture);    
    rDeleteTexture(mNormalTexture1);
    rDeleteTexture(mTestTexture2d);
    DeleteShaders();
}

void TerrainShowEditor()
{
    char characterPosText[256] = {};
    sprintf_s(characterPosText, sizeof(characterPosText), "%f, %f, %f", characterPos.x, characterPos.y, characterPos.z);
    uText(characterPosText, Vec2(1500.0f, 800.0f));

    // if (uFloatFieldW("START_HEIGHT", &START_HEIGHT, 0.0f, 32.0f, 0.05f)) mShuldUpdate = true;
}