
#include "include/Renderer.hpp"
#include "include/Platform.hpp"
#include "include/UI.hpp"
#include "include/Camera.hpp"

#include "../ASTL/Math/Matrix.hpp"
#include "../ASTL/IO.hpp"

// From Texture.cpp
extern void CompressSaveImages(char* path, const char** images, int numImages);
extern void LoadSceneImages(char* path, Texture* textures, int numImages);

static Texture mLayers[3 * 3];

static Shader mTerrainShader;
static Shader mHeightShader;
static Shader mCalculateNormalShader;
static Texture mHeightTexture;
static Texture mNormalTexture;
static Texture mGreyNoiseTexture;

static FrameBuffer mHeightFrameBuffer;
static bool mFirstInit = true;

// adjustables
static int mNumChunks = 32; // on x and z axis
static int mChunkNumSegments = 64;
static float mChunkSize = 20.0f; // < in meters. each segment has (chunkSize / numSegment) width and height

// uniform locations
static int uChunkNumSegments, uChunkSize, uNumChunks, uViewProj, uCameraPos, uCameraDir;

static void SetUniforms()
{
    rBindShader(mTerrainShader);
    rSetShaderValue(mNumChunks, uNumChunks); 
    rSetShaderValue(mChunkNumSegments, uChunkNumSegments); 
    rSetShaderValue(mChunkSize, uChunkSize);
}

void TerrainCreateShaders()
{
    if (mFirstInit == false)
    {
        rDeleteShader(mTerrainShader);
        rDeleteShader(mHeightShader);
        rDeleteShader(mCalculateNormalShader);
    }
    else
    {
        mGreyNoiseTexture = rImportTexture("Assets/Textures/ShadertoyGreyNoise.png");

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

    mHeightShader = rImportFullScreenShader("Assets/Shaders/PerlinNoise.glsl");
    mCalculateNormalShader = rImportFullScreenShader("Assets/Shaders/TerrainGenNormals.glsl");
    mTerrainShader = rImportShader("Assets/Shaders/EmptyVert.glsl", "Assets/Shaders/TerrainFrag.glsl", "Assets/Shaders/TerrainGeom.glsl");

    uChunkNumSegments = rGetUniformLocation(mTerrainShader, "mChunkNumSegments"); 
    uChunkSize = rGetUniformLocation(mTerrainShader, "mChunkSize"); 
    uNumChunks = rGetUniformLocation(mTerrainShader, "mNumChunks");
    uViewProj = rGetUniformLocation(mTerrainShader, "mViewProj");
    uCameraDir = rGetUniformLocation(mTerrainShader, "mCameraDir");
    uCameraPos = rGetUniformLocation(mTerrainShader, "mCameraPos");
}

static void CreateTextures() {
    if (mFirstInit == false) rDeleteTexture(mHeightTexture);
    int segmentPerChunk = mNumChunks * mChunkNumSegments;
    mHeightTexture = rCreateTexture(segmentPerChunk, segmentPerChunk, nullptr, TextureType_R8, TexFlags_RawData);
    mNormalTexture = rCreateTexture(segmentPerChunk, segmentPerChunk, nullptr, TextureType_RGBA8, TexFlags_RawData);
    mHeightFrameBuffer = rCreateFrameBuffer(true);
    rFrameBufferAttachColor(mHeightTexture, 0);
    rFrameBufferCheck();
}

static void GenerateHeightTexture() {
    rFrameBufferAttachColor(mHeightTexture, 0);
    rBindFrameBuffer(mHeightFrameBuffer);
    rSetViewportSize(mNumChunks * mChunkNumSegments, mNumChunks * mChunkNumSegments);
    rBindShader(mHeightShader);
    rRenderFullScreen();
    
    rFrameBufferAttachColor(mNormalTexture, 0);
    rSetTexture(mHeightTexture, 0, rGetUniformLocation("mPerlinNoise"));
    rBindShader(mCalculateNormalShader);
    rRenderFullScreen();
}

void InitTerrain()
{
    TerrainCreateShaders();
    SetUniforms();
    CreateTextures();
    GenerateHeightTexture();

    mFirstInit = false;
}

void RenderTerrain(CameraBase* camera)
{
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

    Vector3f cameraDir = Vector3f::Cross(camera->Right, Vector3f::Up());

    rSetShaderValue(cameraDir.arr       , uCameraDir, GraphicType_Vector3f);
    rSetShaderValue(camera->position.arr, uCameraPos, GraphicType_Vector3f);
    rSetShaderValue(viewProj.GetPtr()   , uViewProj , GraphicType_Matrix4);

    rRenderGeomPoint(mNumChunks * mNumChunks, 0);
}

void TerrainShowEditor()
{
    if (uIntFieldW("Num Chunks", &mNumChunks, 4, 128)) InitTerrain(); 
    if (uIntFieldW("Chunk Num Segments", &mChunkNumSegments, 4, 64)) InitTerrain(); 
    if (uFloatFieldW("Chunk Size", &mChunkSize, 1.0f, 64.0f)) InitTerrain();
}
