
/********************************************************************************
*    Purpose:                                                                   *
*        Simple graphics interface that runs in varius different platforms.     *
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

inline void* stbi_realloc(void* p, size_t newsz)
{
    delete[] (char*)p;
    p = (void*)new char[newsz];
    return p;
}

#define STBI_MALLOC(sz) (void*)new char[sz]
#define STBI_REALLOC(p,newsz) stbi_realloc(p, newsz)
#define STBI_FREE(p) delete[] (char*)p;

#define STBI_ASSERT(x) ASSERT(x)
#define STB_IMAGE_IMPLEMENTATION

#include "../External/stb_image.h"

#include "Renderer.hpp"
#include "Platform.hpp"

GLuint g_DefaultTexture;

unsigned char* g_TextureLoadBuffer = nullptr;
uint64_t g_TextureLoadBufferSize = 1024 * 1024 * 4;

namespace
{
    GLuint m_EmptyVAO;
    Shader m_DefaultFragShader;
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

// https://www.khronos.org/opengles/sdk/docs/man31/html/glTexImage2D.xhtml
static const TextureFormat TextureFormatTable[] =
{
    {   GL_R8             , GL_RED,             GL_UNSIGNED_BYTE                 }, // TextureType_R8           = 0,
    {   GL_R8_SNORM       , GL_RED,             GL_BYTE                          }, // TextureType_R8_SNORM     = 1,
    {   GL_R16F           , GL_RED,             GL_HALF_FLOAT                    }, // TextureType_R16F         = 2,
    {   GL_R32F           , GL_RED,             GL_FLOAT                         }, // TextureType_R32F         = 3,
    {   GL_R8UI           , GL_RED_INTEGER ,    GL_UNSIGNED_BYTE                 }, // TextureType_R8UI         = 4,
    {   GL_R16UI          , GL_RED_INTEGER ,    GL_UNSIGNED_SHORT                }, // TextureType_R16UI        = 5,
    {   GL_R32UI          , GL_RED_INTEGER ,    GL_UNSIGNED_INT                  }, // TextureType_R32UI        = 6,
    {   GL_RG8            , GL_RG,              GL_UNSIGNED_BYTE                 }, // TextureType_RG8	         = 7,
    {   GL_RG8_SNORM      , GL_RG,              GL_BYTE                          }, // TextureType_RG8_SNORM    = 8,
    {   GL_RG16F          , GL_RG,              GL_HALF_FLOAT                    }, // TextureType_RG16F        = 9,
    {   GL_RG32F          , GL_RG,              GL_FLOAT                         }, // TextureType_RG32F        = 10,
    {   GL_RG16UI         , GL_RG_INTEGER,      GL_UNSIGNED_SHORT                }, // TextureType_RG16UI       = 11,
    {   GL_RG32UI         , GL_RG_INTEGER,      GL_UNSIGNED_INT                  }, // TextureType_RG32UI       = 12,
    {   GL_RGB8           , GL_RGB,             GL_UNSIGNED_BYTE                 }, // TextureType_RGB8         = 13,
    {   GL_SRGB8          , GL_RGB,             GL_UNSIGNED_BYTE                 }, // TextureType_SRGB8        = 14,
    {   GL_RGB8_SNORM     , GL_RGB,             GL_BYTE                          }, // TextureType_RGB8_SNORM   = 15,
    {   GL_R11F_G11F_B10F , GL_RGB,             GL_HALF_FLOAT                    }, // TextureType_R11F_G11F_B1 = 16,
    {   GL_RGB9_E5        , GL_RGB,             GL_HALF_FLOAT                    }, // TextureType_RGB9_E5      = 17,
    {   GL_RGB16F         , GL_RGB,             GL_HALF_FLOAT                    }, // TextureType_RGB16F       = 18,
    {   GL_RGB32F         , GL_RGB,             GL_FLOAT                         }, // TextureType_RGB32F       = 19,
    {   GL_RGB8UI         , GL_RGB_INTEGER,     GL_UNSIGNED_BYTE                 }, // TextureType_RGB8UI       = 20,
    {   GL_RGB16UI        , GL_RGB_INTEGER,     GL_UNSIGNED_SHORT                }, // TextureType_RGB16UI      = 21,
    {   GL_RGB32UI        , GL_RGB_INTEGER,     GL_UNSIGNED_INT                  }, // TextureType_RGB32UI      = 22,
    {   GL_RGBA8          , GL_RGBA,            GL_UNSIGNED_BYTE                 }, // TextureType_RGBA8        = 23,
    {   GL_SRGB8_ALPHA8   , GL_RGBA,            GL_UNSIGNED_BYTE                 }, // TextureType_SRGB8_ALPHA8 = 24,
    {   GL_RGBA8_SNORM    , GL_RGBA,            GL_BYTE                          }, // TextureType_RGBA8_SNORM  = 25,
    {   GL_RGB5_A1        , GL_RGBA,            GL_UNSIGNED_BYTE                 }, // TextureType_RGB5_A1      = 26,
    {   GL_RGBA4          , GL_RGBA,            GL_UNSIGNED_BYTE                 }, // TextureType_RGBA4        = 27,
    {   GL_RGB10_A2       , GL_RGBA,            GL_UNSIGNED_INT_2_10_10_10_REV   }, // TextureType_RGB10_A2     = 28,
    {   GL_RGBA16F        , GL_RGBA,            GL_HALF_FLOAT                    }, // TextureType_RGBA16F      = 29,
    {   GL_RGBA32F        , GL_RGBA,            GL_FLOAT                         }, // TextureType_RGBA32F      = 30,
    {   GL_RGBA8UI        , GL_RGBA_INTEGER,    GL_UNSIGNED_BYTE                 }, // TextureType_RGBA8UI      = 31,
    {   GL_RGBA16UI       , GL_RGBA_INTEGER,    GL_UNSIGNED_SHORT                }, // TextureType_RGBA16UI     = 33,
    {   GL_RGBA32UI       , GL_RGBA_INTEGER,    GL_UNSIGNED_INT                  }, // TextureType_RGBA32UI     = 34,
    {}, {}, {}, {},                                                                 // Compressed Formats
    { GL_DEPTH24_STENCIL8  , GL_DEPTH_STENCIL,   GL_UNSIGNED_INT_24_8             }, // TextureType_DepthStencil24
    { GL_DEPTH32F_STENCIL8 , GL_DEPTH_STENCIL,   GL_DEPTH32F_STENCIL8             }, // TextureType_DepthStencil32
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
        default: AX_ERROR("Unknown GL error: %d\n", error); break;
    }
    return "UNKNOWN_GL_ERROR";
}

Texture CreateShadowTexture(int shadowmapSize)
{
    Texture texture;
    glGenTextures(1, &texture.handle);
    glBindTexture(GL_TEXTURE_2D, texture.handle);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_DEPTH_COMPONENT16, shadowmapSize, shadowmapSize);
    CHECK_GL_ERROR();
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);

    const float borderColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

    texture.width  = shadowmapSize;
    texture.height = shadowmapSize;
    texture.buffer = nullptr;
    texture.type   = TextureType_DepthStencil24;

    CHECK_GL_ERROR();
    return texture;
}

// type is either 0 or 1 if compressed. 1 means has alpha
Texture CreateTexture(int width, int height, void* data, TextureType type, bool mipmap, bool compressed)
{
    Texture texture;
    glGenTextures(1, &texture.handle);
    glBindTexture(GL_TEXTURE_2D, texture.handle);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, IsAndroid() ? GL_NEAREST : GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, mipmap ? GL_LINEAR_MIPMAP_NEAREST : GL_LINEAR);

    texture.width  = width;
    texture.height = height;
    texture.buffer = (unsigned char*)data;
    texture.type   = type;

    if (!compressed)
    {
        TextureFormat format = TextureFormatTable[type];
        glTexImage2D(GL_TEXTURE_2D, 0, format.internalFormat, width, height, 0, format.format, format.type, data);
    }
    else
#ifndef __ANDROID__ 
    {
        int blockSize = width * height;
        blockSize >>= type == TextureType_CompressedR; // bc4 is 0.5 byte per pixel

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
        int numMips = MAX((int)Log2((unsigned)width) >> 1, 1) - 1;

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, numMips);
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

static bool IsCompressed(const char* path, int pathLen)
{
#ifndef __ANDROID__ 
    return path[pathLen-1] == 't' && path[pathLen-2] == 'x' && path[pathLen-3] == 'd';
#else
    return path[pathLen-1] == 'c' && path[pathLen-2] == 't' && path[pathLen-3] == 's' && path[pathLen-4] == 'a';
    
#endif
}

void ResizeTextureLoadBufferIfNecessarry(unsigned long long size)
{
    if (g_TextureLoadBufferSize < size)
    {
        delete[] g_TextureLoadBuffer;
        g_TextureLoadBufferSize = size;
        g_TextureLoadBuffer = new unsigned char[g_TextureLoadBufferSize];
    }
}

Texture LoadTexture(const char* path, bool mipmap)
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

    ResizeTextureLoadBufferIfNecessarry(size);
    
    bool compressed = IsCompressed(path, StringLength(path));
    if (!compressed)
    {
        AFileRead(g_TextureLoadBuffer, size, asset);
        image = stbi_load_from_memory(g_TextureLoadBuffer , size, &width, &height, &channels, 0);
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
    Texture texture = CreateTexture(width, height, image, numCompToFormat[channels], mipmap, compressed);
    if (!compressed)
    {
        stbi_image_free(image);
    }
    return texture;
}

void DeleteTexture(Texture texture) 
{ 
    glDeleteTextures(1, &texture.handle); 
}

/*//////////////////////////////////////////////////////////////////////////*/
/*                                 Frame Buffer                             */
/*//////////////////////////////////////////////////////////////////////////*/

FrameBuffer CreateFrameBuffer()
{
    FrameBuffer frameBuffer;
    glGenFramebuffers(1, &frameBuffer.handle);
    return frameBuffer;
}

void DeleteFrameBuffer(FrameBuffer frameBuffer)
{
    glDeleteFramebuffers(1, &frameBuffer.handle);
}

bool CheckFrameBuffer(FrameBuffer framebuffer)
{
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        AX_ERROR("framebuffer incomplate");
        return false;
    }
    return true;
}

Texture CreateRenderTexture(int width, int height, TextureType type)
{
    return CreateTexture(width, height, nullptr, type, false, false);
}

void BindFrameBuffer(FrameBuffer frameBuffer)
{
    glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer.handle);
}

void UnbindFrameBuffer()
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void FrameBufferAttachDepth(Texture texture)
{
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, texture.handle, 0);
    CHECK_GL_ERROR();
}

void FrameBufferAttachDepthStencil(Texture texture)
{
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, texture.handle, 0);
    CHECK_GL_ERROR();
}

void FrameBufferAttachColor(Texture texture, int index)
{
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + index,
                           GL_TEXTURE_2D, texture.handle, 0);
    CHECK_GL_ERROR();
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

inline GLenum GLTypeToSize(GraphicType type)
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
    ASSERT(wrap < 5 && "wrong or undefined sampler type!"); 
    return values[wrap];
}

GPUMesh CreateMesh(void* vertexBuffer, void* indexBuffer, int numVertex, int numIndex, GraphicType indexType, const InputLayoutDesc* layoutDesc)
{
    GPUMesh mesh;
    glGenBuffers(1, &mesh.vertexHandle);
    glGenBuffers(1, &mesh.indexHandle);

    glBindBuffer(GL_ARRAY_BUFFER, mesh.vertexHandle);
    glBufferData(GL_ARRAY_BUFFER, (uint64)layoutDesc->stride * numVertex, vertexBuffer, GL_STATIC_DRAW);
    CHECK_GL_ERROR();

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.indexHandle);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, numIndex * GLTypeToSize(indexType), indexBuffer, GL_STATIC_DRAW);
    CHECK_GL_ERROR();
    
    mesh.numIndex = numIndex; 
    mesh.numVertex = numVertex; 
    mesh.indexType = GL_BYTE + indexType;
    
    ASSERT(layoutDesc && layoutDesc->numLayout && layoutDesc->layout && layoutDesc->stride);

    glGenVertexArrays(1, &mesh.vertexLayoutHandle);
    glBindVertexArray(mesh.vertexLayoutHandle);

    char* offset = 0;
    for (int i = 0; i < layoutDesc->numLayout; ++i)
    {
        InputLayout layout = layoutDesc->layout[i];
        bool isNormalized = !!(layout.type & GraphicTypeNormalizeBit);
        layout.type &= ~GraphicTypeNormalizeBit;

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
        offset += layout.numComp * GLTypeToSize(layout.type);
    }
    CHECK_GL_ERROR();
    return mesh;
}

void CreateMeshFromPrimitive(APrimitive* primitive, GPUMesh* mesh)
{
    InputLayoutDesc desc;
    const int NumLayout = 4;

    InputLayout inputLayout[NumLayout] = 
    {
        { 3, GraphicType_Float   },
        { 3, GraphicType_XYZ10W2 },
        { 4, GraphicType_Half    },
        { 2, GraphicType_Float   }
    };

    desc.layout = inputLayout;
    desc.stride = sizeof(AVertex); 
    desc.numLayout = ArraySize(inputLayout);

    *mesh = CreateMesh(primitive->vertices, primitive->indices, primitive->numVertices, primitive->numIndices, primitive->indexType, &desc);
}

void BindMesh(GPUMesh mesh)
{
    glBindVertexArray(mesh.vertexLayoutHandle);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.indexHandle);
    CHECK_GL_ERROR();
}

void RenderMeshIndexOffset(GPUMesh mesh, int numIndex, int offset)
{
    glDrawElements(GL_TRIANGLES, numIndex, mesh.indexType, (void*)((size_t)offset * sizeof(uint32)));
    CHECK_GL_ERROR();
}

void RenderMesh(GPUMesh mesh)
{
    BindMesh(mesh);
    glDrawElements(GL_TRIANGLES, mesh.numIndex, mesh.indexType, nullptr);
    CHECK_GL_ERROR();
}

void DeleteMesh(GPUMesh mesh)
{
    glDeleteVertexArrays(1, &mesh.vertexLayoutHandle);
    glDeleteBuffers(1, &mesh.vertexHandle);
    glDeleteBuffers(1, &mesh.indexHandle);
}

/*//////////////////////////////////////////////////////////////////////////*/
/*                                 Shader                                   */
/*//////////////////////////////////////////////////////////////////////////*/

static unsigned int currentShader = 0;

Shader GetCurrentShader()
{
    return {currentShader};
}

unsigned int GetUniformLocation(const char* name)
{
    return glGetUniformLocation(currentShader, name);
}

unsigned int GetUniformLocation(Shader shader, const char* name)
{
    return glGetUniformLocation(shader.handle, name);
}

void SetShaderValue(int   value, unsigned int location)
{ 
    glUniform1i(location, value);
}

void SetShaderValue(float value, unsigned int location)
{ 
    glUniform1f(location, value);
}

void SetShaderValue(const void* value, unsigned int location, GraphicType type)
{
    switch (type)
    {
        case GraphicType_Int:         glUniform1i(location , *(const int*)value);          break;
        case GraphicType_UnsignedInt: glUniform1ui(location, *(const unsigned int*)value); break;
        case GraphicType_Float:       glUniform1f(location , *(const float*)value);        break;

        case GraphicType_Vector2f:    glUniform2fv(location, 1, (const float*)value); break;
        case GraphicType_Vector3f:    glUniform3fv(location, 1, (const float*)value); break;
        case GraphicType_Vector4f:    glUniform4fv(location, 1, (const float*)value); break;
     
        case GraphicType_Matrix2: glUniformMatrix2fv(location, 1, GL_FALSE, (const float*)value); break;
        case GraphicType_Matrix3: glUniformMatrix3fv(location, 1, GL_FALSE, (const float*)value); break;
        case GraphicType_Matrix4: glUniformMatrix4fv(location, 1, GL_FALSE, (const float*)value); break;
        default:
            ASSERT(0 && "Shader set value Graphic type invalid. type:");
            break;
    }
}

void SetMaterial(AMaterial* material)
{
    // TODO: set material
}

static void CheckShaderError(uint shader)
{
    GLint isCompiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &isCompiled);
    if (isCompiled == GL_FALSE)
    {
        GLint maxLength = 1024;
        char infoLog[1024]{};
        glGetShaderInfoLog(shader, maxLength, &maxLength, infoLog);
        AX_ERROR("shader compile error: %s", infoLog);
        glDeleteShader(shader);
        DestroyRenderer();
    }
}

static void CheckLinkerError(uint shader)
{
    GLint isCompiled = 0;
    glGetProgramiv(shader, GL_LINK_STATUS, &isCompiled);
    if (isCompiled == GL_FALSE)
    {
        GLint maxLength = 1024;
        char infoLog[1024]{};
        glGetProgramInfoLog(shader, maxLength, &maxLength, infoLog);
        AX_ERROR("shader compile error: %s", infoLog);
        glDeleteShader(shader);
        DestroyRenderer();
    }
}

Shader CreateShader(const char* vertexSource, const char* fragmentSource)
{
    // Vertex shader
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexSource, NULL);
    glCompileShader(vertexShader);
    CheckShaderError(vertexShader);
    // Check for compile time errors
    GLint success;
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success); ASSERT(success);
    // Fragment shader
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentSource, NULL);
    glCompileShader(fragmentShader);
    CheckShaderError(fragmentShader);
    // Check for compile time errors
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success); ASSERT(success);
    // Link shaders
    uint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    // Check for linking errors
    CheckLinkerError(shaderProgram);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    glUseProgram(shaderProgram);
    CHECK_GL_ERROR();
    return {shaderProgram};
}

Shader CreateFullScreenShader(const char* fragmentSource)
{
    const GLchar* vertexShaderSource =
    AX_SHADER_VERSION_PRECISION()
    "out vec2 texCoord;\n\
    void main(){\n\
    	float x = -1.0 + float((gl_VertexID & 1) << 2);\n\
    	float y = -1.0 + float((gl_VertexID & 2) << 1);\n\
    	texCoord.x = (x + 1.0) * 0.5;\n\
    	texCoord.y = (y + 1.0) * 0.5;\n\
    	texCoord.y = 1.0 - texCoord.y;\n\
        gl_Position = vec4(x, y, 0, 1);\n\
    }";
    return CreateShader(vertexShaderSource, fragmentSource);
}

Shader ImportShader(const char* vertexPath, const char* fragmentPath)
{
    char* vertexText   = ReadAllText(vertexPath, nullptr, nullptr, AX_SHADER_VERSION_PRECISION());
    char* fragmentText = ReadAllText(fragmentPath, nullptr, nullptr, AX_SHADER_VERSION_PRECISION());
    
    Shader shader = CreateShader(vertexText, fragmentText);
    FreeAllText(vertexText);
    FreeAllText(fragmentText);
    return shader;
}

void DeleteShader(Shader shader)    
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
        img[i * 2 + 1] = 125; // roughness
    }
    g_DefaultTexture = CreateTexture(32, 32, img, TextureType_RG8, false).handle;
}

static void CreateDefaultScreenShader()
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
    m_DefaultFragShader = CreateFullScreenShader(fragmentShaderSource);
}

void InitRenderer()
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
    CreateDefaultScreenShader();

    g_TextureLoadBuffer = new unsigned char[g_TextureLoadBufferSize];
}

void SetDepthTest(bool val)
{
    void(*EnableDisable[2])(unsigned int) = { glDisable, glEnable};
    EnableDisable[val](GL_DEPTH_TEST);
}

void SetDepthWrite(bool val) 
{
    glDepthMask(val); 
}

void ClearDepth()
{
    glClear(GL_DEPTH_BUFFER_BIT); //| GL_STENCIL_BUFFER_BIT
    CHECK_GL_ERROR();
}

void BeginShadow()
{
    glReadBuffer(GL_NONE);
    glCullFace(GL_FRONT);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    CHECK_GL_ERROR();
}

void EndShadow()
{
    glCullFace(GL_BACK);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    CHECK_GL_ERROR();
}

void ClearColor(float r, float g, float b, float a)
{
    glClearColor(r, g, b, a);
}

void RenderFullScreen(Shader fullScreenShader, unsigned int texture)
{
    glUseProgram(fullScreenShader.handle);
    glBindVertexArray(m_EmptyVAO);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glUniform1i(0, 0);// glUniform1i(glGetUniformLocation(fullScreenShader.handle, "tex"), 0);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    CHECK_GL_ERROR();
}

void RenderFullScreen(unsigned int texture)
{
    RenderFullScreen(m_DefaultFragShader, texture);
}

void BindShader(Shader shader)
{
    glUseProgram(shader.handle);
    currentShader = shader.handle;
    CHECK_GL_ERROR();
}

void SetTexture(Texture texture, int index, unsigned int location)
{
    glActiveTexture(GL_TEXTURE0 + index);
    glBindTexture(GL_TEXTURE_2D, texture.handle);
    glUniform1i(location, index);
}

void SetViewportSize(int width, int height)
{
    glViewport(0, 0, width, height);
}

void DestroyRenderer()
{
    glDeleteTextures(1, &g_DefaultTexture);
    DeleteShader(m_DefaultFragShader);

    delete[] g_TextureLoadBuffer;
    g_TextureLoadBuffer = nullptr;
}
