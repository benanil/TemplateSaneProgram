
/************************************************************************** 
*    Purpose: Render interface that works with OpenGL 4.2 and OpenGL ES3  *
*             Notice that each function has R prefix to indicate Renderer *
*             I saw it in Doom source code I think makes sense.           *
*    Author : Anilcan Gulkaya 2023 anilcangulkaya7@gmail.com              *
**************************************************************************/

#pragma once

#include "../../ASTL/Additional/GLTFParser.hpp"
#include "../../ASTL/Common.hpp"
#include "../../ASTL/Math/Vector.hpp"
#include "../../ASTL/Math/Half.hpp"
#include "../../ASTL/Math/SIMDVectorMath.hpp"

#ifdef __ANDROID__  
#define AX_SHADER_VERSION_PRECISION() "#version 320 es\n"              \
                                      "precision highp float;\n"       \
                                      "precision mediump sampler2D;\n" \
                                      "precision mediump int;\n"       \
                                      "#define ANDROID 1\n"        \
                                      "bool IsAndroid() { return true; }\n"
#else
#define AX_SHADER_VERSION_PRECISION() "#version 430 core \n"          \
                                      "bool IsAndroid() { return false; }\n"
#endif

typedef int TextureType;

struct Texture
{
    int width, height, depth;
    unsigned int handle;
    TextureType type;
    unsigned char* buffer;
};

enum GraphicType_
{
    GraphicType_Byte, // -> 0x1400 in opengl 
    GraphicType_UnsignedByte, 
    GraphicType_Short,
    GraphicType_UnsignedShort, 
    GraphicType_Int,
    GraphicType_UnsignedInt,
    GraphicType_Float,
    GraphicType_TwoByte,
    GraphicType_ThreeByte,
    GraphicType_FourByte,
    GraphicType_Double,
    GraphicType_Half, // -> 0x140B in opengl
    GraphicType_XYZ10W2, // GL_INT_2_10_10_10_REV

    GraphicType_Vector2f,
    GraphicType_Vector3f,
    GraphicType_Vector4f,

    GraphicType_Vector2i,
    GraphicType_Vector3i,
    GraphicType_Vector4i,

    GraphicType_Matrix2,
    GraphicType_Matrix3,
    GraphicType_Matrix4,

    GraphicType_NormalizeBit = 1 << 31
};
typedef int GraphicType;

inline Vector3f Unpack_INT_2_10_10_10_REV(uint32_t p) 
{
    Vector3f result;
    const uint32_t tenMask = (1 << 10)-1;
    result.x = 255.0f / ((p >>  0) & tenMask);
    result.y = 255.0f / ((p >> 10) & tenMask);
    result.z = 255.0f / ((p >> 20) & tenMask);
    return result;
}

struct GPUMesh
{
    int numVertex, numIndex;
    // unsigned because opengl accepts unsigned
    unsigned int vertexLayoutHandle;
    unsigned int indexHandle;
    unsigned int indexType;  // uint32, uint64. GL_BYTE + indexType
    unsigned int vertexHandle; // opengl handles for, POSITION, TexCoord...
    // usefull for knowing which attributes are there
    // POSITION, TexCoord... AAttribType_ bitmask
    int attributes;
    int stride; // size of an vertex of the mesh
    
    void* vertices;
    void* indices;
    
    // w value is undefined, it could be anything or trash data
    vec_t GetPosition(int index)
    {
        const char* bytePtr = (const char*)vertices;
        bytePtr += stride * index;
        return VecLoad((const float*)bytePtr);
    }

    // todo GPUMesh GetNormal
    vec_t GetNormal(int index)
    {
        const char* bytePtr = (const char*)vertices;
        bytePtr += stride * index + sizeof(Vector3f); // skip position
        uint32_t normalPacked = *(uint32_t *)bytePtr;
        union S { vec_t v; float3 s; };
        S s = {};
        s.s = Unpack_INT_2_10_10_10_REV(normalPacked);
        return s.v; // VecLoad(bytePtr);
    }

    // todo GPUMesh GetNormal
    Vector2f GetUV(int index)
    {
        const char* bytePtr = (const char*)vertices;
        bytePtr += stride * index + sizeof(Vector3f) + sizeof(uint) + sizeof(uint); // skip normal and tangent
        uint32_t uvPacked = *(uint32_t *)bytePtr;
        Vector2f result;
        ConvertHalf2ToFloat2(result.arr, uvPacked); // VecLoad(bytePtr);
        return result;
    }
};

struct InputLayout
{
    int numComp;
    GraphicType type; // |= GraphicTypeNormalizeBit if type is normalized
};

struct InputLayoutDesc
{
    int numLayout;
    int stride;
    const InputLayout* layout; 
    bool dynamic;
};

// https://www.yosoygames.com.ar/wp/2018/03/vertex-formats-part-1-compression/
struct AVertex
{
    Vector3f position;
    uint     normal;
    uint     tangent;
    half2    texCoord;
};

struct alignas(16) ASkinedVertex
{
    Vector3f position;
    uint     normal;
    uint     tangent;
    half2    texCoord;
    uint     joints;  // rgb8u
    uint     weights; // rgb8u
};

/*//////////////////////////////////////////////////////////////////////////*/
/*                                 Mesh                                     */
/*//////////////////////////////////////////////////////////////////////////*/

// only uint32 indices accepted, indexbuffer can be null if you want only vertex rendering
GPUMesh rCreateMesh(const void* vertexBuffer, const void* indexBuffer, int numVertex, int numIndex, GraphicType indexType, const InputLayoutDesc* layoutDesc);

void rUpdateMesh(GPUMesh* mesh, void* data, size_t size);

void rDeleteMesh(GPUMesh mesh);

void rCreateMeshFromPrimitive(APrimitive* primitive, GPUMesh* mesh, bool skined);

void rBindMesh(GPUMesh mesh);

void rRenderMeshIndexed(GPUMesh mesh);

void rRenderMesh(int numVertex);

void rRenderMeshNoVertex(int numIndex);

int GraphicsTypeToSize(GraphicType type);

/*//////////////////////////////////////////////////////////////////////////*/
/*                                 Renderer                                 */
/*//////////////////////////////////////////////////////////////////////////*/

enum rBlendFunc_  { 
    rBlendFunc_Zero, 
    rBlendFunc_One,
    rBlendFunc_Alpha,
    rBlendFunc_OneMinusAlpha
};

enum rCompare_ {
    rNEVER, 
    rLESS,
    rLEQUAL, 
    rGREATER, 
    rGEQUAL, 
    rEQUAL, 
    rNOTEQUAL,
    rALWAYS
};

enum rStencilOp_ {
    rKEEP,
    rZERO,
    rREPLACE,
    rINCR,
    rINCR_WRAP,
    rDECR,
    rDECR_WRAP,
    rINVERT
};

typedef int rBlendFunc;
typedef int rCompare;
typedef int rStencilOp;

struct Shader {
    unsigned int handle; 
};

// renders an texture to screen with given shader
void rRenderFullScreen(Shader fullScreenShader, unsigned int texture);

// renders an texture to screen
void rRenderFullScreen(unsigned int texture);

// draws full screen quad, don't forget to set sahaders and uniforms
void rRenderFullScreen();

void rSetViewportSize(int width, int height);

void rRenderMeshIndexOffset(GPUMesh mesh, int numIndex, int offset);

void rInitRenderer();

void rDestroyRenderer();

void rSetBlending(bool val);

void rSetBlendingFunction(rBlendFunc src, rBlendFunc dst);

void rDrawLine(Vector3f start, Vector3f end, uint color);

void rDrawLineCube(const Vector3f corners[8], uint color);

void rDrawAllLines(float* viewProj);

void rSetClockWise(bool val);

void rClearColor(float r, float g, float b, float a);

//------------------------------------------------------------------------
// Depth
void rToggleDepthTest(bool val);

void rSetDepthWrite(bool val);

void rClearDepth();

void rClearDepthStencil();


//------------------------------------------------------------------------
// Shadow
void rBeginShadow();

void rEndShadow();

//------------------------------------------------------------------------
// Stencil
void rStencilMask(uint mask);

void rClearStencil();

void rStencilFunc(rCompare compare, uint ref, uint mask);

void rStencilOperation(rStencilOp op, rStencilOp fail, rStencilOp pass);

void rStencilToggle(bool active);

//------------------------------------------------------------------------
// Scissor
void rScissorToggle(bool active);

void rScissorToggle(bool active);

void rScissor(int x, int y, int width, int height);

/*//////////////////////////////////////////////////////////////////////////*/
/*                                 Texture                                  */
/*//////////////////////////////////////////////////////////////////////////*/

enum TexFlags_
{
    TexFlags_None        = 0,
    TexFlags_MipMap      = 1,
    TexFlags_Compressed  = 2,
    TexFlags_ClampToEdge = 4,
    TexFlags_Nearest     = 8,
    TexFlags_Linear      = 16, // default linear in desktop platforms
    // no filtering or wrapping
    TexFlags_RawData     = TexFlags_Nearest | TexFlags_ClampToEdge
};

typedef int TexFlags;

enum DepthType_ { 
    DepthType_16, 
    DepthType_24, 
    DepthType_32 
};
typedef int DepthType;

void rUnpackAlignment(int n);

uint8_t rTextureTypeToBytesPerPixel(TextureType type);

// type is either 0 or 1 if compressed. 1 means has alpha
Texture rCreateTexture(int width, int height, void* data, TextureType type, TexFlags flags = TexFlags_None);

Texture rCreateTexture2DArray(int width, int height, int depth, void* data, TextureType type, TexFlags flags = TexFlags_None);

Texture rCreateDepthTexture(int width, int height, DepthType depthType);

// Imports texture from disk and loads to GPU
Texture rImportTexture(const char* path, TexFlags flags = TexFlags_None);

// upload image data to GPU, warning: texture should be created with TexFlaags_RawData
// for now you can only update with same format and same width and height
void rUpdateTexture(Texture texture, void* data);

void rDeleteTexture(Texture texture);

void rSetTexture(Texture texture, int index, unsigned int loc);

void rSetTexture(Texture* texture, int index, unsigned int location);

void rSetTexture2DArray(Texture texture, int index, unsigned int loc);

void rSetTexture(unsigned int textureHandle, int index, unsigned int loc);

void rCopyTexture(Texture dst, Texture src);

/*//////////////////////////////////////////////////////////////////////////*/
/*                                 Frame Buffer                             */
/*//////////////////////////////////////////////////////////////////////////*/

struct FrameBuffer {
    unsigned int handle;
};

// don't forget to bind framebuffer after you create it
FrameBuffer rCreateFrameBuffer(bool bind = false);

void rDeleteFrameBuffer(FrameBuffer frameBuffer);
     
bool rFrameBufferCheck();
     
void rBindFrameBuffer(FrameBuffer frameBuffer);
     
void rUnbindFrameBuffer();
     
void rFrameBufferAttachDepth(Texture texture);

void rFrameBufferAttachDepthStencil(Texture texture);

void rFrameBufferAttachColor(Texture texture, int index = 0);
     
void rFrameBufferAttachColorFrom2DArray(Texture texture, int attachmentIdx, int layerIdx);

void rFrameBufferSetNumColorBuffers(int numBuffers);
     
void rFrameBufferInvalidate(int numAttachments);

/*//////////////////////////////////////////////////////////////////////////*/
/*                                 Shader                                   */
/*//////////////////////////////////////////////////////////////////////////*/
struct ComputeBuffer {
    unsigned int handle; 
    int index; 
    bool dynamic;
};

enum TextureAccess_ { 
    TextureAccess_ReadOnly, 
    TextureAccess_WriteOnly, 
    TextureAccess_ReadWrite 
};
typedef int TextureAccess;

void rBindShader(Shader shader);
// you don't have to specify names, it is for debug purpose only
Shader rCreateShader(const char* vertexSource, const char* fragmentSource, const char* vertexFile = nullptr, const char* fragFile = nullptr);
Shader rImportShader(const char* vertexPath, const char* fragmentPath);
Shader rCreateFullScreenShader(const char* fragmentSource, const char* name = nullptr);
Shader rImportFullScreenShader(const char* path);

// ComputeShader
// note: use rBindShader function to bind compute shader
Shader rCreateComputeShader(const char* source);
Shader rImportComputeShader(const char* path);
ComputeBuffer rComputeCreateBuffer(Shader compute, int size, const char* name, const void* data = nullptr, bool dynamic = false);
void rComputeUpdateBuffer(ComputeBuffer buffer, void* data, size_t size);

void rComputeBindBuffer(int binding, int buffer);

void rComputeBindTexture(Texture texture, int unit, TextureAccess access);
// Don't forget bind before this function
void rDispatchCompute(Shader shader, int workGroupsX, int workGroupsY, int workGroupsZ);

void rComputeBarier();

void rDeleteShader(Shader shader);

// Todo(Anil): lookup uniforms
int rGetUniformLocation(const char* name);

int rGetUniformLocation(Shader shader, const char* name);

// usage:
// char arrayText[32] = {};
// int begin = sizeof("uPointLights");
// rGetUniformArrayLocations(begin, arrayText, lPointLightPositions, "uPointLights[0].position");
void rGetUniformArrayLocations(int begin, char* arrayText, int* outUniformLocations, int numLocations, const char* uniformName);

Shader rGetCurrentShader();

void rSetMaterial(AMaterial* material);

// sets uniform to binded shader
void rSetShaderValue(const void* value, int location, GraphicType type);

// sets uniform to binded shader
void rSetShaderValue(int value, int location);

// sets uniform to binded shader
void rSetShaderValue(uint value, int location);

// sets uniform to binded shader
void rSetShaderValue(float value, int location);


// Warning! Order is important
enum TextureType_ 
{
    TextureType_R8             = 0,
    TextureType_R8_SNORM       = 1,
    TextureType_R16F           = 2,
    TextureType_R16_SNORM      = 3,
    TextureType_R32F           = 4,
    TextureType_R8UI           = 5,
    TextureType_R16UI          = 6,
    TextureType_R32UI          = 7,
    TextureType_RG8            = 8,
    TextureType_RG8_SNORM      = 9,
    TextureType_RG16F          = 10,
    TextureType_RG32F          = 11,
    TextureType_RG16UI         = 12,
    TextureType_RG16_SNORM     = 13,
    TextureType_RG32UI         = 14,
    TextureType_RGB8           = 15,
    TextureType_SRGB8          = 16,
    TextureType_RGB8_SNORM	   = 17,
    TextureType_R11F_G11F_B10  = 18,
    TextureType_RGB9_E5        = 19,
    TextureType_RGB565         = 20,
    TextureType_RGB16F         = 21,
    TextureType_RGB32F         = 22,
    TextureType_RGB8UI         = 23,
    TextureType_RGB16UI        = 24,
    TextureType_RGB32UI        = 25,
    TextureType_RGBA8          = 26,
    TextureType_SRGB8_ALPHA8   = 27,
    TextureType_RGBA8_SNORM    = 28,
    TextureType_RGB5_A1        = 29,
    TextureType_RGBA4          = 30,
    TextureType_RGB10_A2       = 31,
    TextureType_RGBA16F        = 32,
    TextureType_RGBA32F        = 33,
    TextureType_RGBA8UI        = 34,
    TextureType_RGBA16UI       = 35,
    TextureType_RGBA32UI       = 36,
    TextureType_RGBA16_SNORM   = 37,
    // Compressed Formats
    TextureType_CompressedR    = 38,
    TextureType_CompressedRG   = 39,
	TextureType_CompressedRGB  = 40,
	TextureType_CompressedRGBA = 41,
    // Depth Formats
    TextureType_Depth24Stencil8 = 42,
    TextureType_Depth32Stencil8 = 43
};
