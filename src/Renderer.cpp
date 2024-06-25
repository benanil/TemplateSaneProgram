
/********************************************************************************
*    Purpose:                                                                   *
*        Simple graphics interface that runs in varius different platforms.     *
*        OpenGL code is in this file                                            *
*    Author:                                                                    *
*        Anilcan Gulkaya 2023 anilcangulkaya7@gmail.com github @benanil         *
********************************************************************************/


#ifdef __ANDROID__
    #include <game-activity/native_app_glue/android_native_app_glue.h>
    #include <android/log.h>
    #include <GLES3/gl32.h>

    #if defined(_DEBUG) || defined(DEBUG)
        #define CHECK_GL_ERROR() if (GLenum error = glGetError()) {\
                                __android_log_print(ANDROID_LOG_FATAL, "AX-GL_ERROR", "%s -line:%i message: %s", \
                                    __FILE__, __LINE__, GetGLErrorString(error)); ASSERT(0);}
    #else
        #define CHECK_GL_ERROR()
    #endif

    #define STBI_NO_STDIO
    #define STBI_NEON
#else
    #define GLAD_GL_IMPLEMENTATION
    #include "../External/glad.hpp"

    #if defined(_DEBUG) || defined(DEBUG)
        #define CHECK_GL_ERROR() if (GLenum error = glGetError()) \
                                 { FatalError("%s -line:%i message: %s" , __FILE__, __LINE__, GetGLErrorString(error)); ASSERT(0); }
    #else
        #define CHECK_GL_ERROR()
    #endif

#endif

#include "../ASTL/Common.hpp"
#include "../ASTL/IO.hpp"
#include "../ASTL/Math/Matrix.hpp"
#include "../ASTL/String.hpp"

#define STBI_ASSERT(x) ASSERT(x)
#define STB_IMAGE_IMPLEMENTATION

#include "../External/stb_image.h"

#include "include/Renderer.hpp"
#include "include/Platform.hpp"

GLuint g_DefaultTexture;

unsigned char* g_TextureLoadBuffer = nullptr;
uint64_t g_TextureLoadBufferSize = 1024 * 1024 * 4;

namespace
{
    GLuint m_EmptyVAO;
    Shader m_DefaultFragShader;
    Shader m_TextureCopyShader;
}

/*//////////////////////////////////////////////////////////////////////////*/
/*                                 Texture                                  */
/*//////////////////////////////////////////////////////////////////////////*/

struct TextureFormat
{
    GLint  internalFormat;
    GLenum format;
    GLenum type;
};

#ifndef GL_RG16_SNORM
    #define GL_RG16_SNORM 0x8F99
    #define GL_R16_SNORM 0x8F98
#endif

// https://www.khronos.org/opengles/sdk/docs/man31/html/glTexImage2D.xhtml
static const TextureFormat TextureFormatTable[] =
{
    {   GL_R8             , GL_RED,             GL_UNSIGNED_BYTE                 }, // TextureType_R8            = 0,
    {   GL_R8_SNORM       , GL_RED,             GL_BYTE                          }, // TextureType_R8_SNORM      = 1,
    {   GL_R16F           , GL_RED,             GL_HALF_FLOAT                    }, // TextureType_R16F          = 2,
    {   GL_R16_SNORM      , GL_RED,             GL_SHORT                         }, // TextureType_R16_SNORM     = 3         
    {   GL_R32F           , GL_RED,             GL_FLOAT                         }, // TextureType_R32F          = 4,
    {   GL_R8UI           , GL_RED_INTEGER ,    GL_UNSIGNED_BYTE                 }, // TextureType_R8UI          = 5,
    {   GL_R16UI          , GL_RED_INTEGER ,    GL_UNSIGNED_SHORT                }, // TextureType_R16UI         = 6,
    {   GL_R32UI          , GL_RED_INTEGER ,    GL_UNSIGNED_INT                  }, // TextureType_R32UI         = 7,
    {   GL_RG8            , GL_RG,              GL_UNSIGNED_BYTE                 }, // TextureType_RG8	         = 8,
    {   GL_RG8_SNORM      , GL_RG,              GL_BYTE                          }, // TextureType_RG8_SNORM     = 9,
    {   GL_RG16F          , GL_RG,              GL_HALF_FLOAT                    }, // TextureType_RG16F         = 10,
    {   GL_RG32F          , GL_RG,              GL_FLOAT                         }, // TextureType_RG32F         = 11,
    {   GL_RG16UI         , GL_RG_INTEGER,      GL_UNSIGNED_SHORT                }, // TextureType_RG16UI        = 12,
    {   GL_RG16_SNORM     , GL_RG_INTEGER,      GL_SHORT                         }, // TextureType_RG16_SNORM    = 13,
    {   GL_RG32UI         , GL_RG_INTEGER,      GL_UNSIGNED_INT                  }, // TextureType_RG32UI        = 14,
    {   GL_RGB8           , GL_RGB,             GL_UNSIGNED_BYTE                 }, // TextureType_RGB8          = 15,
    {   GL_SRGB8          , GL_RGB,             GL_UNSIGNED_BYTE                 }, // TextureType_SRGB8         = 16,
    {   GL_RGB8_SNORM     , GL_RGB,             GL_BYTE                          }, // TextureType_RGB8_SNORM    = 17,
    {   GL_R11F_G11F_B10F , GL_RGB,             GL_FLOAT                         }, // TextureType_R11F_G11F_B10 = 18,
    {   GL_RGB9_E5        , GL_RGB,             GL_HALF_FLOAT                    }, // TextureType_RGB9_E5       = 19,
    {   GL_RGB565         , GL_RGB,             GL_UNSIGNED_SHORT_5_6_5          }, // TextureType_RGB565        = 20,
    {   GL_RGB16F         , GL_RGB,             GL_HALF_FLOAT                    }, // TextureType_RGB16F        = 21,
    {   GL_RGB32F         , GL_RGB,             GL_FLOAT                         }, // TextureType_RGB32F        = 22,
    {   GL_RGB8UI         , GL_RGB_INTEGER,     GL_UNSIGNED_BYTE                 }, // TextureType_RGB8UI        = 23,
    {   GL_RGB16UI        , GL_RGB_INTEGER,     GL_UNSIGNED_SHORT                }, // TextureType_RGB16UI       = 24,
    {   GL_RGB32UI        , GL_RGB_INTEGER,     GL_UNSIGNED_INT                  }, // TextureType_RGB32UI       = 25,
    {   GL_RGBA8          , GL_RGBA,            GL_UNSIGNED_BYTE                 }, // TextureType_RGBA8         = 26,
    {   GL_SRGB8_ALPHA8   , GL_RGBA,            GL_UNSIGNED_BYTE                 }, // TextureType_SRGB8_ALPHA8  = 27,
    {   GL_RGBA8_SNORM    , GL_RGBA,            GL_BYTE                          }, // TextureType_RGBA8_SNORM   = 28,
    {   GL_RGB5_A1        , GL_RGBA,            GL_UNSIGNED_BYTE                 }, // TextureType_RGB5_A1       = 29,
    {   GL_RGBA4          , GL_RGBA,            GL_UNSIGNED_BYTE                 }, // TextureType_RGBA4         = 30,
    {   GL_RGB10_A2       , GL_RGBA,            GL_UNSIGNED_INT_2_10_10_10_REV   }, // TextureType_RGB10_A2      = 31,
    {   GL_RGBA16F        , GL_RGBA,            GL_HALF_FLOAT                    }, // TextureType_RGBA16F       = 32,
    {   GL_RGBA32F        , GL_RGBA,            GL_FLOAT                         }, // TextureType_RGBA32F       = 33,
    {   GL_RGBA8UI        , GL_RGBA_INTEGER,    GL_UNSIGNED_BYTE                 }, // TextureType_RGBA8UI       = 34,
    {   GL_RGBA16UI       , GL_RGBA_INTEGER,    GL_UNSIGNED_SHORT                }, // TextureType_RGBA16UI      = 35,
    {   GL_RGBA32UI       , GL_RGBA_INTEGER,    GL_UNSIGNED_INT                  }, // TextureType_RGBA32UI      = 36,
    {}, {}, {}, {},{},                                                              // Compressed Formats
    { GL_DEPTH24_STENCIL8  , GL_DEPTH_STENCIL,   GL_UNSIGNED_INT_24_8            }, // TextureType_DepthStencil24 = 41,
    { GL_DEPTH32F_STENCIL8 , GL_DEPTH_STENCIL,   GL_DEPTH32F_STENCIL8            }, // TextureType_DepthStencil32 = 42,
};

const char* GetGLErrorString(GLenum error) 
{
    switch (error) {
        case GL_NO_ERROR: break;
        case GL_INVALID_ENUM:                  return "GL_INVALID_ENUM\n";
        case GL_INVALID_VALUE:                 return "GL_INVALID_VALUE\n";
        case GL_INVALID_OPERATION:             return "GL_INVALID_OPERATION\n";
        case GL_INVALID_FRAMEBUFFER_OPERATION: return "GL_INVALID_FRAMEBUFFER_OPERATION\n";
        case GL_OUT_OF_MEMORY:                 return "GL_OUT_OF_MEMORY\n";
        default: AX_ERROR("Unknown GL error: %d\n", error);
    }
    return "UNKNOWN_GL_ERROR";
}

void rCopyTexture(Texture dst, Texture src)
{
    // note: todo??
    glBindTexture(GL_TEXTURE_2D, dst.handle);
    glCopyTexSubImage2D(src.handle, 
                        0, // mip
                        0, // xoffset
                        0, // yoffset
                        0, // x
                        0, // y
                        dst.width, dst.height);
    CHECK_GL_ERROR();
}

Texture rCreateDepthTexture(int width, int height, DepthType depthType)
{
    Texture texture;
    texture.width  = width;
    texture.height = height;
    texture.buffer = nullptr;
    texture.type   = TextureType_DepthStencil24; // < ??
    glGenTextures(1, &texture.handle);
    glBindTexture(GL_TEXTURE_2D, texture.handle);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_DEPTH_COMPONENT16 + depthType, width, height);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    const float borderColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
    CHECK_GL_ERROR();
    return texture;
}

void rUnpackAlignment(int n)
{
    glPixelStorei(GL_UNPACK_ALIGNMENT, n);
}

// type is either 0 or 1 if compressed. 1 means has alpha
Texture rCreateTexture(int width, int height, void* data, TextureType type, TexFlags flags)
{
    Texture texture;
    glGenTextures(1, &texture.handle);
    glBindTexture(GL_TEXTURE_2D, texture.handle);
    int wrapMode = (flags & TexFlags_ClampToEdge) ? GL_CLAMP_TO_EDGE : GL_REPEAT;
    bool mipmap     = !!(flags & TexFlags_MipMap);
    bool nearest    = !!(flags & TexFlags_Nearest);
    bool compressed = !!(flags & TexFlags_Compressed);

    __const int mipmapFilter = GL_LINEAR_MIPMAP_NEAREST; // < higher | lower quality > GL_NEAREST_MIPMAP_NEAREST
    int defaultMagFilter = IsAndroid() ? GL_NEAREST : GL_LINEAR;
    defaultMagFilter = (flags & TexFlags_Linear) ? GL_LINEAR : defaultMagFilter;

    int minFilter = nearest ? GL_NEAREST : GL_LINEAR;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapMode);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapMode);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, nearest ? GL_NEAREST : defaultMagFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, mipmap ? mipmapFilter : minFilter);

    int numMips = MAX((int)Log2((unsigned)width) >> 1, 1) - 1;
    if (mipmap) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, numMips);
        // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, 4);
    }

    texture.width  = width;
    texture.height = height;
    texture.buffer = (unsigned char*)data;
    texture.type   = type;

    if (!compressed)
    {
        TextureFormat format = TextureFormatTable[type];
        if (flags == TexFlags_RawData)
        {
            glTexStorage2D(GL_TEXTURE_2D, 1, format.internalFormat, width, height);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, format.format, format.type, data);
        }
        else glTexImage2D(GL_TEXTURE_2D, 0, format.internalFormat, width, height, 0, format.format, format.type, data);
    }
    else
    #ifndef __ANDROID__ 
    {

        int blockSize = width * height;
        blockSize >>= int(type == TextureType_CompressedR); // bc4 is 0.5 byte per pixel

        const int compressedMap[] =
        {
            GL_COMPRESSED_RED_RGTC1, // BC4 
            GL_COMPRESSED_RG_RGTC2,  // BC5 
            GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, // I'm using dxt5 for rgb textures because it has better quality compared to dxt1
            GL_COMPRESSED_RGBA_S3TC_DXT5_EXT
        };
        int arrIndex = type-TextureType_CompressedR;
        glCompressedTexImage2D(GL_TEXTURE_2D, 0, compressedMap[arrIndex], width, height, 0, blockSize, data);
    }
    #else
    {
        int mip = 0;
        do
        {
            glCompressedTexImage2D(GL_TEXTURE_2D, mip, GL_COMPRESSED_RGBA_ASTC_4x4, width, height, 0, width * height, data);
            data = ((char*)data) + (width * height);
            width >>= 1;
            height >>= 1;
            mip++;
        } while(numMips-- > 0);
    }
    #endif
    
    if (!IsAndroid() && mipmap)
        glGenerateMipmap(GL_TEXTURE_2D);

    CHECK_GL_ERROR();
    return texture;
}

//-
void rUpdateTexture(Texture texture, void* data)
{
    TextureFormat format = TextureFormatTable[texture.type];
    glBindTexture(GL_TEXTURE_2D, texture.handle);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, texture.width, texture.height, format.format, format.type, data);
    CHECK_GL_ERROR();
}

static bool IsCompressed(const char* path, int pathLen)
{
#ifndef __ANDROID__ 
    return path[pathLen-1] == 't' && path[pathLen-2] == 'x' && path[pathLen-3] == 'd';
#else
    return path[pathLen-1] == 'c' && path[pathLen-2] == 't' && path[pathLen-3] == 's' && path[pathLen-4] == 'a';
    
#endif
}

static void rResizeTextureLoadBufferIfNecessarry(unsigned long long size)
{
    if (g_TextureLoadBufferSize < size)
    {
        delete[] g_TextureLoadBuffer;
        g_TextureLoadBufferSize = size;
        g_TextureLoadBuffer = new unsigned char[g_TextureLoadBufferSize];
    }
}

Texture rLoadTexture(const char* path, bool mipmap)
{
    int width, height, channels;
    unsigned char* image = nullptr;
    Texture defTexture;
    defTexture.width  = 32;
    defTexture.height = 32;
    defTexture.handle = g_DefaultTexture;

    if (!FileExist(path))
    {
        AX_ERROR("image is not exist, using default texture! %s", path);
        return defTexture;
    }

    AFile asset = AFileOpen(path, AOpenFlag_Read);
    uint64_t size = AFileSize(asset);

    rResizeTextureLoadBufferIfNecessarry(size);
    
    int compressed = IsCompressed(path, StringLength(path));
    if (!compressed)
    {
        AFileRead(g_TextureLoadBuffer, size, asset);
        image = stbi_load_from_memory(g_TextureLoadBuffer, (int)size, &width, &height, &channels, 0);
    }
    else
    {
        ASSERT(0 && "not implemented");
    }
    
    AFileClose(asset);
    
    if (image == nullptr)
    {
        AX_ERROR("image load failed! %s", path);
        return defTexture;
    }
    const TextureType numCompToFormat[5] = { 0, TextureType_R8, TextureType_RG8, TextureType_RGB8, TextureType_RGBA8 };
    TexFlags flags = (int)mipmap | (compressed << 1);
    Texture texture = rCreateTexture(width, height, image, numCompToFormat[channels], flags);
    if (!compressed)
    {
        stbi_image_free(image);
    }
    return texture;
}

void rDeleteTexture(Texture texture) 
{ 
    glDeleteTextures(1, &texture.handle); 
}

/*//////////////////////////////////////////////////////////////////////////*/
/*                                 Frame Buffer                             */
/*//////////////////////////////////////////////////////////////////////////*/

FrameBuffer rCreateFrameBuffer()
{
    FrameBuffer frameBuffer;
    glGenFramebuffers(1, &frameBuffer.handle);
    return frameBuffer;
}

void rDeleteFrameBuffer(FrameBuffer frameBuffer)
{
    glDeleteFramebuffers(1, &frameBuffer.handle);
}

bool rFrameBufferCheck()
{
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        AX_ERROR("framebuffer incomplate");
        return false;
    }
    return true;
}

void rBindFrameBuffer(FrameBuffer frameBuffer)
{
    glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer.handle);
}

void rUnbindFrameBuffer()
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void rFrameBufferAttachDepth(Texture texture)
{
    // glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, texture.handle);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, texture.handle, 0);
    CHECK_GL_ERROR();
}

void rFrameBufferAttachDepthStencil(Texture texture)
{
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, texture.handle, 0);
    CHECK_GL_ERROR();
}

void rFrameBufferAttachColor(Texture texture, int index)
{
    glBindTexture(GL_TEXTURE_2D, texture.handle);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + index,
                           GL_TEXTURE_2D, texture.handle, 0);
    CHECK_GL_ERROR();
}

static const int glAtt = GL_COLOR_ATTACHMENT0;
static const unsigned int glAttachments[8] = {glAtt, glAtt + 1, glAtt+2, glAtt+3, glAtt+4, glAtt+5, glAtt+6, glAtt+7};

void rFrameBufferInvalidate(int numAttachments)
{
    glInvalidateFramebuffer(GL_FRAMEBUFFER, numAttachments, glAttachments);
    CHECK_GL_ERROR();
}

void rFrameBufferSetNumColorBuffers(int numBuffers)
{
    glDrawBuffers(numBuffers, glAttachments);
}

/*//////////////////////////////////////////////////////////////////////////*/
/*                                 Mesh                                     */
/*//////////////////////////////////////////////////////////////////////////*/

inline GLenum ToGLType(GraphicType type)
{
    return GL_BYTE + (GLenum)type;
}

inline bool IsTypeInt(GraphicType type)
{
    return type != GraphicType_Float && type != GraphicType_Double && type != GraphicType_Half;
}

int GraphicsTypeToSize(GraphicType type)
{
    // BYTE, UNSIGNED_BYTE, SHORT, UNSIGNED_SHORT, INT, UNSIGNED_INT, FLOAT           
    const int TypeToSize[12]={ 1, 1, 2, 2, 4, 4, 4, 2, 4, 4, 8, 2 };
    return TypeToSize[type];
}

inline char GLTFFilterToOGLFilter(char filter) {
    return (int)filter + 0x2600; // // GL_NEAREST 0x2600 9728, GL_LINEAR 0x2601 9729
}

inline unsigned int GLTFWrapToOGLWrap(int wrap) {
    unsigned int values[] = { 0x2901, 0x812F, 0x812D, 0x8370 }; // GL_REPEAT GL_CLAMP_TO_EDGE, GL_CLAMP_TO_BORDER, GL_MIRRORED_REPEAT
    ASSERTR(wrap < 5 && "wrong or undefined sampler type!", return values[0]);
    AX_ASSUME(wrap < 5);
    return values[wrap];
}

GPUMesh rCreateMesh(const void* vertexBuffer, const void* indexBuffer, int numVertex, int numIndex, GraphicType indexType, const InputLayoutDesc* layoutDesc)
{
    GPUMesh mesh;
    mesh.indexHandle = -1;
    int bufferUsage = !layoutDesc->dynamic ? GL_STATIC_DRAW : GL_DYNAMIC_DRAW;
    // generate vertex buffer
    {
        glGenBuffers(1, &mesh.vertexHandle);
        glBindBuffer(GL_ARRAY_BUFFER, mesh.vertexHandle);
        glBufferData(GL_ARRAY_BUFFER, (uint64)layoutDesc->stride * numVertex, vertexBuffer, bufferUsage);
        CHECK_GL_ERROR();
    }

    // generate indexBuffer
    if (indexBuffer != nullptr)
    {
        glGenBuffers(1, &mesh.indexHandle);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.indexHandle);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, numIndex * GraphicsTypeToSize(indexType), indexBuffer, bufferUsage);
        CHECK_GL_ERROR();
    }
    
    mesh.numIndex = numIndex; 
    mesh.numVertex = numVertex; 
    mesh.indexType = GL_BYTE + indexType;
    
    ASSERT(layoutDesc && layoutDesc->numLayout && layoutDesc->layout && layoutDesc->stride);

    // Generate vertex attribute
    glGenVertexArrays(1, &mesh.vertexLayoutHandle);
    glBindVertexArray(mesh.vertexLayoutHandle);

    char* offset = 0;
    for (int i = 0; i < layoutDesc->numLayout; ++i)
    {
        InputLayout layout = layoutDesc->layout[i];
        bool isNormalized = !!(layout.type & GraphicType_NormalizeBit);
        layout.type &= ~GraphicType_NormalizeBit;

        if (layout.type == GraphicType_XYZ10W2)
        {
            glVertexAttribPointer(i, 4, GL_INT_2_10_10_10_REV, true, layoutDesc->stride, offset);
            glEnableVertexAttribArray(i);
            offset += 4; //sizeof(int);
            continue;
        }

        if (!IsTypeInt(layout.type) || isNormalized)
            glVertexAttribPointer(i, layout.numComp, GL_BYTE + layout.type, isNormalized, layoutDesc->stride, offset);
        else 
            glVertexAttribIPointer(i, layout.numComp, GL_BYTE + layout.type, layoutDesc->stride, offset);

        glEnableVertexAttribArray(i);
        offset += layout.numComp * GraphicsTypeToSize(layout.type);
    }
    CHECK_GL_ERROR();
    return mesh;
}

void rUpdateMesh(GPUMesh* mesh, void* data, size_t size)
{
    glBindBuffer(GL_ARRAY_BUFFER, mesh->vertexHandle);
    glBufferSubData(GL_ARRAY_BUFFER, 0, size, data);
}

void rCreateMeshFromPrimitive(APrimitive* primitive, GPUMesh* mesh, bool skined)
{
    InputLayoutDesc desc;

    const InputLayout inputLayout[] = 
    {
        { 3, GraphicType_Float   },
        { 3, GraphicType_XYZ10W2 },
        { 4, GraphicType_XYZ10W2 },
        { 2, GraphicType_Half    },
        // below is for skined vertices
        { 4, GraphicType_UnsignedByte },
        { 4, GraphicType_UnsignedByte | GraphicType_NormalizeBit }
    };

    desc.layout = inputLayout;
    desc.stride = skined ? sizeof(ASkinedVertex) : sizeof(AVertex); 
    desc.numLayout = skined ? ArraySize(inputLayout) : ArraySize(inputLayout) - 2;
    desc.dynamic = false;
    *mesh = rCreateMesh(primitive->vertices, primitive->indices, primitive->numVertices, primitive->numIndices, primitive->indexType, &desc);
}

void rBindMesh(GPUMesh mesh)
{
    glBindVertexArray(mesh.vertexLayoutHandle);
    if (mesh.indexHandle != -1) glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.indexHandle);
    CHECK_GL_ERROR();
}

void rRenderMeshIndexOffset(GPUMesh mesh, int numIndex, int offset)
{
    glDrawElements(GL_TRIANGLES, numIndex, mesh.indexType, (void*)((size_t)offset * sizeof(uint32)));
    CHECK_GL_ERROR();
}

void rRenderMeshIndexed(GPUMesh mesh)
{
    glDrawElements(GL_TRIANGLES, mesh.numIndex, mesh.indexType, nullptr);
    CHECK_GL_ERROR();
}

void rRenderMesh(int numVertex)
{
    glDrawArrays(GL_TRIANGLES, 0, numVertex);
    CHECK_GL_ERROR();
}

void rRenderMeshNoVertex(int numIndex)
{ 
    glDrawArrays(GL_TRIANGLES, 0, numIndex);
    CHECK_GL_ERROR();
}

void rDeleteMesh(GPUMesh mesh)
{
    glDeleteVertexArrays(1, &mesh.vertexLayoutHandle);
    glDeleteBuffers(1, &mesh.vertexHandle);
    glDeleteBuffers(1, &mesh.indexHandle);
}

/*//////////////////////////////////////////////////////////////////////////*/
/*                                 Shader                                   */
/*//////////////////////////////////////////////////////////////////////////*/

static unsigned int currentShader = 0;

Shader rGetCurrentShader()
{
    return {currentShader};
}

int rGetUniformLocation(const char* name)
{
    return glGetUniformLocation(currentShader, name);
}

int rGetUniformLocation(Shader shader, const char* name)
{
    return glGetUniformLocation(shader.handle, name);
}

void rSetShaderValue(int   value, int location) { glUniform1i(location, value); CHECK_GL_ERROR(); }
void rSetShaderValue(uint  value, int location) { glUniform1ui(location, value); CHECK_GL_ERROR(); }
void rSetShaderValue(float value, int location) { glUniform1f(location, value); CHECK_GL_ERROR();  }

// zero dynamic memory allocation
void rGetUniformArrayLocations(int begin, char* arrayText, int* locations, int numLocations, const char* uniformName)
{
    MemsetZero(arrayText, 32);
    int nameLen = (int)StringLength(uniformName);
    ASSERT(nameLen < 32);
    AX_ASSUME(nameLen < 32);
    SmallMemCpy(arrayText, uniformName, nameLen);
    // read uniform locations between 0-9
    for (int i = 0; i < 10; i++)
    {
        arrayText[begin + 0] = '0' + (i % 10);
        locations[i] = rGetUniformLocation(arrayText);
    }
    
    nameLen += 1;

    while (nameLen > begin) // move to right by one character
    {
        arrayText[nameLen] = arrayText[nameLen-1];
        nameLen--;
    }
    // read uniform locations between 10-99
    for (int i = 10; i < numLocations; i++)
    {
        arrayText[begin + 0] = '0' + (i / 10);
        arrayText[begin + 1] = '0' + (i % 10);
        locations[i] = rGetUniformLocation(arrayText);
    }

    if (numLocations < 100) 
        return;

    nameLen += 1;
    while (nameLen > begin){
        arrayText[nameLen] = arrayText[nameLen-1];
        nameLen--;
    }

    ASSERT(numLocations < 1000);
    // read uniform locations between 100-999
    for (int i = 100; i < numLocations; i++)
    {
        int x = i / 100;
        arrayText[begin + 0] = '0' + x;
        arrayText[begin + 1] = '0' + ((i - (x * 100)) / 10);
        arrayText[begin + 2] = '0' + (i % 10);
        locations[i] = rGetUniformLocation(arrayText);
    }
}

void rSetShaderValue(const void* value, int location, GraphicType type)
{
    switch (type)
    {
        case GraphicType_Int:         glUniform1i(location , *(const int*)value);          break;
        case GraphicType_UnsignedInt: glUniform1ui(location, *(const unsigned int*)value); break;
        case GraphicType_Float:       glUniform1f(location , *(const float*)value);        break;

        case GraphicType_Vector2f:    glUniform2fv(location, 1, (const float*)value); break;
        case GraphicType_Vector3f:    glUniform3fv(location, 1, (const float*)value); break;
        case GraphicType_Vector4f:    glUniform4fv(location, 1, (const float*)value); break;
     
        case GraphicType_Vector2i:    glUniform2iv(location, 1, (const int*)value); break;
        case GraphicType_Vector3i:    glUniform3iv(location, 1, (const int*)value); break;
        case GraphicType_Vector4i:    glUniform4iv(location, 1, (const int*)value); break;

        case GraphicType_Matrix2: glUniformMatrix2fv(location, 1, GL_FALSE, (const float*)value); break;
        case GraphicType_Matrix3: glUniformMatrix3fv(location, 1, GL_FALSE, (const float*)value); break;
        case GraphicType_Matrix4: glUniformMatrix4fv(location, 1, GL_FALSE, (const float*)value); break;
        default:
            ASSERT(0 && "Shader set value Graphic type invalid. type:");
            break;
    }
    CHECK_GL_ERROR();
}

void rSetMaterial(AMaterial* material)
{
    // TODO: set material
}

static void CheckShaderError(uint shader)
{
    GLint isCompiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &isCompiled);
    if (isCompiled == GL_FALSE)
    {
        GLint maxLength = 2048;
        char infoLog[2048]={};
        glGetShaderInfoLog(shader, maxLength, &maxLength, infoLog);
        AX_ERROR("shader compile error: %s", infoLog);
        glDeleteShader(shader);
    }
}

static void CheckLinkerError(uint shader)
{
    GLint isCompiled = 0;
    glGetProgramiv(shader, GL_LINK_STATUS, &isCompiled);
    if (isCompiled == GL_FALSE)
    {
        GLint maxLength = 2048;
        char infoLog[2048]={};
        glGetProgramInfoLog(shader, maxLength, &maxLength, infoLog);
        AX_ERROR("shader compile error: %s", infoLog);
        glDeleteShader(shader);
    }
}

Shader rCreateComputeShader(const char* source)
{
    // Vertex shader
    GLuint computeShader = glCreateShader(GL_COMPUTE_SHADER);
    glShaderSource(computeShader, 1, &source, NULL);
    glCompileShader(computeShader);
    CheckShaderError(computeShader);
    
    uint program = glCreateProgram();
    glAttachShader(program, computeShader);
    glLinkProgram(program);
    CheckLinkerError(program);
    glDeleteShader(computeShader);
    return { program };
}

Shader rImportComputeShader(const char* path)
{
    ScopedText text = ReadAllText(path, nullptr, nullptr, AX_SHADER_VERSION_PRECISION());
    Shader shader = rCreateComputeShader(text.text);
    return shader;
}

ComputeBuffer rComputeShaderCreateBuffer(Shader compute, int size, const char* name, const void* data)
{
    ComputeBuffer res;
    glGenBuffers(1, &res.handle);
    glBindBuffer(GL_UNIFORM_BUFFER, res.handle);
    glBufferData(GL_UNIFORM_BUFFER, size, data, GL_STATIC_DRAW);
    
    res.index = glGetUniformBlockIndex(compute.handle, name);
    glUniformBlockBinding(compute.handle, res.index, 0);
    return res;
}

void rComputeShaderBindBuffer(int binding, int buffer)
{
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding, buffer);
    CHECK_GL_ERROR();
}

void rBindTextureToCompute(Texture texture, int unit, TextureAccess access)
{
    const int accessMap[3] = { GL_READ_ONLY, GL_WRITE_ONLY, GL_READ_WRITE };
    // For example
    glBindImageTexture(unit,          /* unit, note that we're not offsetting GL_TEXTURE0 */
                       texture.handle,/* a 2D texture for example */
                       0,             /* miplevel */
                       GL_FALSE,      /* bool layered: we cannot use layered */
                       0,             /* int layer: this is ignored */
                       accessMap[access], /* we're only writing to it */
                       TextureFormatTable[texture.type].internalFormat); /* ie: rgb8, r32f */
    CHECK_GL_ERROR();
}

void rDispatchComputeShader(Shader shader, int workGroupsX, int workGroupsY, int workGroupsZ)
{
    glDispatchCompute(workGroupsX, workGroupsY, workGroupsZ);
    CHECK_GL_ERROR();
}

void rComputeShaderBarier() {
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}

Shader rCreateShader(const char* vertexSource, const char* fragmentSource)
{
    // Vertex shader
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexSource, NULL);
    glCompileShader(vertexShader);
    CheckShaderError(vertexShader);
    // Fragment shader
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentSource, NULL);
    glCompileShader(fragmentShader);
    CheckShaderError(fragmentShader);
    // Link shaders
    uint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    CheckLinkerError(shaderProgram);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    glUseProgram(shaderProgram);
    CHECK_GL_ERROR();
    return {shaderProgram};
}

Shader rCreateFullScreenShader(const char* fragmentSource)
{
    const GLchar* vertexShaderSource =
    AX_SHADER_VERSION_PRECISION()
    "out vec2 texCoord;\n\
    void main(){\n\
        float x = -1.0 + float((gl_VertexID & 1) << 2);\n\
        float y = -1.0 + float((gl_VertexID & 2) << 1);\n\
        texCoord.x = (x + 1.0) * 0.5;\n\
        texCoord.y = (y + 1.0) * 0.5;\n\
        gl_Position = vec4(x, y, 0.0, 1.0);\n\
    }";
    return rCreateShader(vertexShaderSource, fragmentSource);
}

Shader rImportShader(const char* vertexPath, const char* fragmentPath)
{
    ScopedText vertexText   = ReadAllText(vertexPath, nullptr, nullptr, AX_SHADER_VERSION_PRECISION());
    ScopedText fragmentText = ReadAllText(fragmentPath, nullptr, nullptr, AX_SHADER_VERSION_PRECISION());
    Shader shader = rCreateShader(vertexText.text, fragmentText.text);
    return shader;
}

void rDeleteShader(Shader shader)    
{
    glDeleteProgram(shader.handle);       
}

/*//////////////////////////////////////////////////////////////////////////*/
/*                                 Renderer                                 */
/*//////////////////////////////////////////////////////////////////////////*/

void GLDebugMessageCallback(GLenum source, GLenum type, GLuint id, GLenum severity,
                            GLsizei length, const GLchar* msg, const void* data) {
    AX_ERROR("OpenGL error: %s", msg);
    AX_LOG("\n");
}

static void CreateDefaultTexture()
{
    unsigned char img[32*32*2]{};
    
    for (int i = 0; i < (32 * 32); i++)
    {
        img[i * 2 + 0] = 85;  // metallic 
        img[i * 2 + 1] = 155; // roughness
    }
    g_DefaultTexture = rCreateTexture(32, 32, img, TextureType_RG8, TexFlags_None).handle;
}

typedef struct {
    Vector3f pos;
    uint color;
} LineVertex;

static int numLines;
static const int TotalLines = 128;
static LineVertex lineVertices[TotalLines];
static GLuint lineVao, lineVbo;
static Shader lineShader;

static void SetupLineRenderer()
{
    numLines = 0;
    glGenVertexArrays(1, &lineVao);
    glGenBuffers(1, &lineVbo);

    glBindVertexArray(lineVao);

    glBindBuffer(GL_ARRAY_BUFFER, lineVbo);
    glBufferData(GL_ARRAY_BUFFER, TotalLines * sizeof(LineVertex), lineVertices, GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(LineVertex), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribIPointer(1, 1, GL_UNSIGNED_INT, sizeof(LineVertex), (GLvoid*)(sizeof(Vector3f)));
    glEnableVertexAttribArray(1);

    const char* VertexShaderSource =
    AX_SHADER_VERSION_PRECISION()
    R"(
        layout(location = 0) in highp vec3 aPos;
        layout(location = 1) in highp uint aColor;
        out lowp vec4 vColor;
        uniform mat4 uViewProj;

        void main() { 
            vColor = unpackUnorm4x8(aColor);
            gl_Position = uViewProj * vec4(aPos, 1.0);
        }
    )";

    const char* fragmentShaderSource =
    AX_SHADER_VERSION_PRECISION()
    R"(
        in lowp vec4 vColor;
        out lowp vec4 color;
        void main() { 
            color = vec4(vColor.xyz, 1.0);
        }
    )";
    lineShader = rCreateShader(VertexShaderSource, fragmentShaderSource);
    glLineWidth(10.0f);
}

void rDrawLine(Vector3f start, Vector3f end, uint color)
{
    lineVertices[numLines].pos     = start;
    lineVertices[numLines++].color = color;

    lineVertices[numLines].pos     = end;
    lineVertices[numLines++].color = color;
}

void rDrawAllLines(float* viewProj)
{
    if (numLines <= 0) return;
    // Todo: android does not support glMapBuffer
    glBindBuffer(GL_ARRAY_BUFFER, lineVbo);

    glBindBuffer(GL_ARRAY_BUFFER, lineVbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, numLines * sizeof(LineVertex), lineVertices);

    rBindShader(lineShader);
    rSetShaderValue(viewProj, 0, GraphicType_Matrix4);
    glBindVertexArray(lineVao);
    glDrawArrays(GL_LINES, 0, numLines);
    numLines = 0;
}

static void CreateDefaultShaders()
{
    const char* fragmentShaderSource =
    AX_SHADER_VERSION_PRECISION()
    R"(
        in vec2 texCoord;
        out vec4 color;
        uniform sampler2D tex;
        void main() {
            color = texture(tex, texCoord);
        }
    )";
    m_DefaultFragShader = rCreateFullScreenShader(fragmentShaderSource);
}

void rSetClockWise(bool val)
{
    glFrontFace(val ? GL_CW : GL_CCW);
}

void rInitRenderer()
{
    glEnable(GL_CULL_FACE);
    glFrontFace(GL_CCW);
    glDepthFunc(GL_LEQUAL);

#if defined(_DEBUG) || defined(DEBUG)
    // enable debug message
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageControl(GL_DEBUG_SOURCE_API, GL_DEBUG_TYPE_ERROR, GL_DEBUG_SEVERITY_LOW, 0, NULL, GL_TRUE);
    glDebugMessageCallback(GLDebugMessageCallback, nullptr);
#endif
    // create empty vao unfortunately this step is necessary for ogl 3.2
    glGenVertexArrays(1, &m_EmptyVAO);

    glClearColor(0.3f, 0.3f, 0.3f, 1.0f); 
    CreateDefaultTexture();
    CreateDefaultShaders();
    SetupLineRenderer();

    g_TextureLoadBuffer = new unsigned char[g_TextureLoadBufferSize];
}

void rSetDepthTest(bool val)
{
    void(*EnableDisable[2])(unsigned int) = { glDisable, glEnable};
    EnableDisable[val](GL_DEPTH_TEST);
}

void rSetDepthWrite(bool val) 
{
    glDepthMask(val); 
}

void rSetBlending(bool val)
{
    void(*EnableDisable[2])(unsigned int) = { glDisable, glEnable};
    EnableDisable[val](GL_BLEND);
}

void rSetBlendingFunction(rBlendFunc src, rBlendFunc dst)
{
    int map[] = {
        GL_ZERO, 
        GL_ONE,
        GL_SRC_ALPHA,
        GL_ONE_MINUS_SRC_ALPHA
    };
    glBlendFunc(map[src], map[dst]);
}

void rClearDepth()
{
    glClear(GL_DEPTH_BUFFER_BIT);
}

void rClearDepthStencil()
{
    glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT); 
}

void rStencilMask(unsigned char mask)
{
    glStencilMask(mask);
}

void rStencilFunc(rCompare compare, uint ref, uint mask)
{
    compare += GL_NEVER; //, GL_LESS, GL_LEQUAL, GL_GREATER, GL_GEQUAL, GL_EQUAL, GL_NOTEQUAL, GL_ALWAYS
    glStencilFunc(compare, ref, mask);
}

void rStencilOperation(rStencilOp op, rStencilOp fail, rStencilOp pass)
{
    const uint opMap[] = {
        GL_KEEP,
        GL_ZERO,
        GL_REPLACE,
        GL_INCR,
        GL_INCR_WRAP,
        GL_DECR,
        GL_DECR_WRAP,
        GL_INVERT
    };
    glStencilOp(opMap[op], opMap[fail], opMap[pass]);
}

void rScissorToggle(bool active)
{
    if (active) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
}

void rScissor(int x, int y, int width, int height)
{
    glScissor(x, y, width, height);
}

void rBeginShadow()
{
    glReadBuffer(GL_NONE);
    glCullFace(GL_FRONT);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
}

void rEndShadow()
{
    glCullFace(GL_BACK);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
}

void rClearColor(float r, float g, float b, float a)
{
    glClearColor(r, g, b, a);
}

void rRenderFullScreen(Shader fullScreenShader, unsigned int texture)
{
    glUseProgram(fullScreenShader.handle);
    glBindVertexArray(m_EmptyVAO);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glUniform1i(0, 0);// glUniform1i(glGetUniformLocation(fullScreenShader.handle, "tex"), 0);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    CHECK_GL_ERROR();
}

void rRenderFullScreen(unsigned int texture)
{
    rRenderFullScreen(m_DefaultFragShader, texture);
    CHECK_GL_ERROR();
}

void rRenderFullScreen()
{
    glBindVertexArray(m_EmptyVAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

void rBindShader(Shader shader)
{
    glUseProgram(shader.handle);
    currentShader = shader.handle;
    CHECK_GL_ERROR();
}

void rSetTexture(Texture texture, int index, unsigned int location)
{
    glActiveTexture(GL_TEXTURE0 + index);
    glBindTexture(GL_TEXTURE_2D, texture.handle);
    glUniform1i(location, index);
    CHECK_GL_ERROR();
}

void rSetTexture(unsigned int textureHandle, int index, unsigned int loc)
{
    Texture texture;
    texture.handle = textureHandle;
    rSetTexture(texture, index, loc);
}

void rSetViewportSize(int width, int height)
{
    glViewport(0, 0, width, height);
}

void rDestroyRenderer()
{
    glDeleteTextures(1, &g_DefaultTexture);
    rDeleteShader(m_DefaultFragShader);
    rDeleteShader(lineShader);
    glDeleteVertexArrays(1, &lineVao);
    glDeleteBuffers(1, &lineVbo);

    delete[] g_TextureLoadBuffer;
    g_TextureLoadBuffer = nullptr;
}
