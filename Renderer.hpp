#ifndef AX_RENDERER_H
#define AX_RENDERER_H

#include "ASTL/Additional/GLTFParser.hpp"

#ifdef __ANDROID__  
#define AX_SHADER_VERSION_PRECISION() "#version 300 es\n" "precision highp float;\n" "precision mediump sampler2D;\n"
#else
#define AX_SHADER_VERSION_PRECISION() "#version 330\n"
#endif

struct Shader { unsigned int handle; };

struct Texture
{
    int width, height;
    unsigned int handle;
    unsigned char* buffer;
};

enum GraphicType_
{
    GraphicType_Byte,
    GraphicType_UnsignedByte, 
    GraphicType_Short,
    GraphicType_UnsignedShort, 
    GraphicType_Int,
    GraphicType_UnsignedInt,
    GraphicType_Float,

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

struct Mesh
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

typedef int TextureType;

// only uint32 indices accepted
Mesh CreateMesh(void* vertexBuffer, void* indexBuffer, int numVertex, int numIndex, int vertexSize, GraphicType indexType, const InputLayoutDesc* layoutDesc);

Mesh CreateMeshFromPrimitive(APrimitive* mesh);

Texture CreateTexture(int width, int height, void* data, TextureType type, bool mipmap, bool compressed = false);

Texture LoadTexture(const char* path, bool mipmap);

Shader LoadShader(const char* vertexSource, const char* fragmentSource);

Shader CreateFullScreenShader(const char* fragmentSource);

Shader ImportShader(const char* vertexSource, const char* fragmentSource);

void DeleteTexture(Texture texture);

void DeleteShader(Shader shader);

void DeleteMesh(Mesh mesh);

// renders an texture to screen with given shader
void RenderFullScreen(Shader fullScreenShader, unsigned int texture);

// renders an texture to screen
void RenderFullScreen(unsigned int texture);

void BindShader(Shader shader);

void SetTexture(Texture texture, int index);

void SetModelViewProjection(float* mvp);

void SetModelMatrix(float* model);

void RenderMesh(Mesh mesh);

void InitRenderer();

void DestroyRenderer();

void SetDepthTest(bool val);

void SetDepthWrite(bool val);

// Todo(Anil): lookup uniforms
unsigned int GetUniformLocation(Shader shader, const char* name);

void SetMaterial(AMaterial* material);

// sets uniform to binded shader
void SetShaderValue(const void* value, unsigned int location, GraphicType type);

// sets uniform to binded shader
void SetShaderValue(int value, unsigned int location);

// sets uniform to binded shader
void SetShaderValue(float value, unsigned int location);


// Warning! Order is important
enum TextureType_
{
    TextureType_R8	         = 0,
    TextureType_R8_SNORM	 = 1,
    TextureType_R16F	     = 2,
    TextureType_R32F	     = 3,
    TextureType_R8UI	     = 4,
    TextureType_R16UI	     = 5,
    TextureType_R32UI	     = 6,

    TextureType_RG8	         = 7,
    TextureType_RG8_SNORM	 = 8,
    TextureType_RG16F	     = 9,
    TextureType_RG32F	     = 10,
    TextureType_RG16UI	     = 11,
    TextureType_RG32UI	     = 12,
    
    TextureType_RGB8	     = 13,
    TextureType_SRGB8	     = 14,
    TextureType_RGB8_SNORM	 = 15,
    TextureType_R11F_G11F_B1 = 16,
    TextureType_RGB9_E5	     = 17,
    TextureType_RGB16F	     = 18,
    TextureType_RGB32F	     = 19,
    TextureType_RGB8UI	     = 20,
    TextureType_RGB16UI	     = 21,
    TextureType_RGB32UI	     = 22,
    
    TextureType_RGBA8	     = 23,
    TextureType_SRGB8_ALPHA8 = 24,
    TextureType_RGBA8_SNORM	 = 25,
    TextureType_RGB5_A1	     = 26,
    TextureType_RGBA4	     = 27,
    TextureType_RGB10_A2	 = 28,
    TextureType_RGBA16F	     = 29,
    TextureType_RGBA32F	     = 30,
    TextureType_RGBA8UI	     = 31,
    TextureType_RGBA16UI	 = 33,
    TextureType_RGBA32UI	 = 34,
};

#endif //AX_RENDERER_H