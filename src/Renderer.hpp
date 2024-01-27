
/************************************************************************** 
*    Purpose: Render interface that works with OpenGL 4.2 and OpenGL ES2  *
*             Notice that each function has R prefix to indicate Renderer *
*             I saw it in Doom source code I think makes sense.           *
*    Author : Anilcan Gulkaya 2023 anilcangulkaya7@gmail.com              *
**************************************************************************/

#pragma once

#include "../ASTL/Additional/GLTFParser.hpp"
#include "../ASTL/Math/Vector.hpp"
#include "../ASTL/Common.hpp"

#ifdef __ANDROID__  
#define AX_SHADER_VERSION_PRECISION() "#version 300 es\n"              \
                                      "precision highp float;\n"     \
                                      "precision mediump sampler2D;\n" \
                                      "precision mediump int;\n"       \
                                      "#define __ANDROID__ 1\n"        \
                                      "#define ALPHA_CUTOFF 0\n"       \
                                      "bool IsAndroid() { return true; }\n"
#else
#define AX_SHADER_VERSION_PRECISION() "#version 420 \n"          \
                                      "#define ALPHA_CUTOFF 0\n" \
                                      "bool IsAndroid() { return false; }\n"
#endif

typedef int TextureType;

struct Shader { unsigned int handle; };

struct Texture
{
    int width, height;
    unsigned int handle;
    unsigned char* buffer;
    TextureType type;
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
};
typedef int GraphicType;

struct GPUMesh
{
    int numVertex, numIndex;
    // unsigned because opengl accepts unsigned
    unsigned int vertexLayoutHandle;
    unsigned int indexHandle;
    unsigned int indexType;  // uint32, uint64.
    unsigned int vertexHandle; // opengl handles for, POSITION, TexCoord...
    // usefull for knowing which attributes are there
    // POSITION, TexCoord... AAttribType_ bitmask
    int attributes;
};

constexpr int GraphicTypeNormalizeBit = 1 << 31;

struct InputLayout
{
    int numComp;
    GraphicType type; // |= GraphicTypeNormalizeBit if type is normalized
};

struct InputLayoutDesc
{
    int numLayout;
    InputLayout* layout; 
    int stride;
};


// https://www.yosoygames.com.ar/wp/2018/03/vertex-formats-part-1-compression/
struct AVertex
{
    Vector3f position;
    int      normal;
    half     tangent[4];
    Vector2f texCoord;
};

/*//////////////////////////////////////////////////////////////////////////*/
/*                                 Mesh                                     */
/*//////////////////////////////////////////////////////////////////////////*/

// only uint32 indices accepted
GPUMesh rCreateMesh(void* vertexBuffer, void* indexBuffer, int numVertex, int numIndex, int vertexSize, GraphicType indexType, const InputLayoutDesc* layoutDesc);

void rDeleteMesh(GPUMesh mesh);

void rCreateMeshFromPrimitive(APrimitive* primitive, GPUMesh* mesh);

void rBindMesh(GPUMesh mesh);

void rRenderMesh(GPUMesh mesh);

/*//////////////////////////////////////////////////////////////////////////*/
/*                                 Renderer                                 */
/*//////////////////////////////////////////////////////////////////////////*/

// renders an texture to screen with given shader
void rRenderFullScreen(Shader fullScreenShader, unsigned int texture);

// renders an texture to screen
void rRenderFullScreen(unsigned int texture);

void rSetViewportSize(int width, int height);

void rRenderMeshIndexOffset(GPUMesh mesh, int numIndex, int offset);

void rInitRenderer();

void rDestroyRenderer();

void rSetDepthTest(bool val);

void rSetDepthWrite(bool val);

void rClearColor(float r, float g, float b, float a);

void rClearDepth();

void rBeginShadow();
void rEndShadow();


/*//////////////////////////////////////////////////////////////////////////*/
/*                                 Texture                                  */
/*//////////////////////////////////////////////////////////////////////////*/

// type is either 0 or 1 if compressed. 1 means has alpha
Texture rCreateTexture(int width, int height, void* data, TextureType type, bool mipmap, bool compressed = false);
Texture rCreateShadowTexture(int shadowmapSize);

void rResizeTextureLoadBufferIfNecessarry(unsigned long long size);

// Imports texture from disk and loads to GPU
Texture rLoadTexture(const char* path, bool mipmap);

void rDeleteTexture(Texture texture);

void rSetTexture(Texture texture, int index, unsigned int loc);

/*//////////////////////////////////////////////////////////////////////////*/
/*                                 Frame Buffer                             */
/*//////////////////////////////////////////////////////////////////////////*/

struct FrameBuffer
{
    unsigned int handle;
};

FrameBuffer rCreateFrameBuffer();

void        rDeleteFrameBuffer(FrameBuffer frameBuffer);
            
bool        rCheckFrameBuffer(FrameBuffer framebuffer);
            
Texture     rCreateRenderTexture(int width, int height, TextureType type);
            
void        rBindFrameBuffer(FrameBuffer frameBuffer);
            
void        rUnbindFrameBuffer();
            
void        rFrameBufferAttachDepth(Texture texture);
            
void        rFrameBufferAttachColor(Texture texture, int index);


/*//////////////////////////////////////////////////////////////////////////*/
/*                                 Shader                                   */
/*//////////////////////////////////////////////////////////////////////////*/

Shader rCreateShader(const char* vertexSource, const char* fragmentSource);

Shader rCreateFullScreenShader(const char* fragmentSource);

Shader rImportShader(const char* vertexSource, const char* fragmentSource);

void   rDeleteShader(Shader shader);

void   rBindShader(Shader shader);

// Todo(Anil): lookup uniforms
unsigned int rGetUniformLocation(const char* name);

unsigned int rGetUniformLocation(Shader shader, const char* name);

Shader rGetCurrentShader();

void rSetMaterial(AMaterial* material);

// sets uniform to binded shader
void rSetShaderValue(const void* value, unsigned int location, GraphicType type);

// sets uniform to binded shader
void rSetShaderValue(int value, unsigned int location);

// sets uniform to binded shader
void rSetShaderValue(float value, unsigned int location);


// Warning! Order is important
enum TextureType_
{
    TextureType_R8           = 0,
    TextureType_R8_SNORM     = 1,
    TextureType_R16F         = 2,
    TextureType_R32F         = 3,
    TextureType_R8UI         = 4,
    TextureType_R16UI        = 5,
    TextureType_R32UI        = 6,

    TextureType_RG8          = 7,
    TextureType_RG8_SNORM    = 8,
    TextureType_RG16F        = 9,
    TextureType_RG32F        = 10,
    TextureType_RG16UI       = 11,
    TextureType_RG32UI       = 12,
    
    TextureType_RGB8         = 13,
    TextureType_SRGB8        = 14,
    TextureType_RGB8_SNORM	 = 15,
    TextureType_R11F_G11F_B1 = 16,
    TextureType_RGB9_E5      = 17,
    TextureType_RGB16F       = 18,
    TextureType_RGB32F       = 19,
    TextureType_RGB8UI       = 20,
    TextureType_RGB16UI      = 21,
    TextureType_RGB32UI      = 22,
    
    TextureType_RGBA8        = 23,
    TextureType_SRGB8_ALPHA8 = 24,
    TextureType_RGBA8_SNORM	 = 25,
    TextureType_RGB5_A1      = 26,
    TextureType_RGBA4        = 27,
    TextureType_RGB10_A2     = 28,
    TextureType_RGBA16F	     = 29,
    TextureType_RGBA32F	     = 30,
    TextureType_RGBA8UI	     = 31,
    TextureType_RGBA16UI     = 33,
    TextureType_RGBA32UI     = 34,

    TextureType_CompressedR    = 35,
    TextureType_CompressedRG   = 36,
	TextureType_CompressedRGB  = 37,
	TextureType_CompressedRGBA = 38,

    TextureType_DepthStencil24 = 39,
    TextureType_DepthStencil32 = 40
};
