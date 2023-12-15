
/********************************************************************************
*    Purpose:                                                                   *
*        Simple graphics interface that runs in varius different platforms.     *
*    Author:                                                                    *
*        Anilcan Gulkaya 2023 anilcangulkaya7@gmail.com github @benanil         *
********************************************************************************/

#ifdef __ANDROID__
    #include <game-activity/native_app_glue/android_native_app_glue.h>
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
    #include "External/glad.hpp"

    #if defined(_DEBUG) || defined(DEBUG)
        #define CHECK_GL_ERROR() if (GLenum error = glGetError()) \
                                 { FatalError("%s -line:%i message: %s" , __FILE__, __LINE__, GetGLErrorString(error)); ASSERT(0); }
    #else
        #define CHECK_GL_ERROR()
    #endif

#endif

#include "ASTL/Common.hpp"
#include "ASTL/IO.hpp"
#include "ASTL/Math/Matrix.hpp"

#define STBI_ASSERT(x) ASSERT(x)
#define STB_IMAGE_IMPLEMENTATION
#include "External/stb_image.h"

#include "Renderer.hpp"
#include "Platform.hpp"

static GLuint emptyVao;
static GLuint defaultTexture;


/*//////////////////////////////////////////////////////////////////////////*/
/*                                 Texture                                  */
/*//////////////////////////////////////////////////////////////////////////*/

struct TextureFormat
{
    GLint  first;
    GLenum format;
    GLenum type;
};

// https://www.khronos.org/opengles/sdk/docs/man31/html/glTexImage2D.xhtml
static const TextureFormat TextureFormatTable[] =
{
    {   GL_R8             , GL_RED,             GL_UNSIGNED_BYTE	             }, //  TextureType_R8	         = 0,
    {   GL_R8_SNORM       , GL_RED,             GL_BYTE                          }, //  TextureType_R8_SNORM	 = 1,
    {   GL_R16F           , GL_RED,             GL_HALF_FLOAT                    }, //  TextureType_R16F	     = 2,
    {   GL_R32F           , GL_RED,             GL_FLOAT	                     }, //  TextureType_R32F	     = 3,
    {   GL_R8UI           , GL_RED_INTEGER ,    GL_UNSIGNED_BYTE	             }, //  TextureType_R8UI	     = 4,
    {   GL_R16UI          , GL_RED_INTEGER ,    GL_UNSIGNED_SHORT	             }, //  TextureType_R16UI	     = 5,
    {   GL_R32UI          , GL_RED_INTEGER ,    GL_UNSIGNED_INT                  }, //  TextureType_R32UI	     = 6,
    {   GL_RG8            , GL_RG,              GL_UNSIGNED_BYTE	             }, //  TextureType_RG8	         = 7,
    {   GL_RG8_SNORM      , GL_RG,              GL_BYTE                          }, //  TextureType_RG8_SNORM	 = 8,
    {   GL_RG16F          , GL_RG,              GL_HALF_FLOAT                    }, //  TextureType_RG16F	     = 9,
    {   GL_RG32F          , GL_RG,              GL_FLOAT	                     }, //  TextureType_RG32F	     = 10,
    {   GL_RG16UI         , GL_RG_INTEGER,      GL_UNSIGNED_SHORT	             }, //  TextureType_RG16UI	     = 11,
    {   GL_RG32UI         , GL_RG_INTEGER,      GL_UNSIGNED_INT	                 }, //  TextureType_RG32UI	     = 12,
    {   GL_RGB8           , GL_RGB,             GL_UNSIGNED_BYTE	             }, //  TextureType_RGB8	     = 13,
    {   GL_SRGB8          , GL_RGB,             GL_UNSIGNED_BYTE	             }, //  TextureType_SRGB8	     = 14,
    {   GL_RGB8_SNORM     , GL_RGB,             GL_BYTE                          }, //  TextureType_RGB8_SNORM	 = 15,
    {   GL_R11F_G11F_B10F , GL_RGB,             GL_HALF_FLOAT                    }, //  TextureType_R11F_G11F_B1 = 16,
    {   GL_RGB9_E5        , GL_RGB,             GL_HALF_FLOAT                    }, //  TextureType_RGB9_E5	     = 17,
    {   GL_RGB16F         , GL_RGB,             GL_HALF_FLOAT                    }, //  TextureType_RGB16F	     = 18,
    {   GL_RGB32F         , GL_RGB,             GL_FLOAT	                     }, //  TextureType_RGB32F	     = 19,
    {   GL_RGB8UI         , GL_RGB_INTEGER,     GL_UNSIGNED_BYTE                 }, //  TextureType_RGB8UI	     = 20,
    {   GL_RGB16UI        , GL_RGB_INTEGER,     GL_UNSIGNED_SHORT                }, //  TextureType_RGB16UI	     = 21,
    {   GL_RGB32UI        , GL_RGB_INTEGER,     GL_UNSIGNED_INT                  }, //  TextureType_RGB32UI	     = 22,
    {   GL_RGBA8          , GL_RGBA,            GL_UNSIGNED_BYTE                 }, //  TextureType_RGBA8	     = 23,
    {   GL_SRGB8_ALPHA8   , GL_RGBA,            GL_UNSIGNED_BYTE                 }, //  TextureType_SRGB8_ALPHA8 = 24,
    {   GL_RGBA8_SNORM    , GL_RGBA,            GL_BYTE                          }, //  TextureType_RGBA8_SNORM	 = 25,
    {   GL_RGB5_A1        , GL_RGBA,            GL_UNSIGNED_BYTE                 }, //  TextureType_RGB5_A1	     = 26,
    {   GL_RGBA4          , GL_RGBA,            GL_UNSIGNED_BYTE                 }, //  TextureType_RGBA4	     = 27,
    {   GL_RGB10_A2       , GL_RGBA,            GL_UNSIGNED_INT_2_10_10_10_REV   }, //  TextureType_RGB10_A2	 = 28,
    {   GL_RGBA16F        , GL_RGBA,            GL_HALF_FLOAT                    }, //  TextureType_RGBA16F	     = 29,
    {   GL_RGBA32F        , GL_RGBA,            GL_FLOAT                         }, //  TextureType_RGBA32F	     = 30,
    {   GL_RGBA8UI        , GL_RGBA_INTEGER,    GL_UNSIGNED_BYTE                 }, //  TextureType_RGBA8UI	     = 31,
    {   GL_RGBA16UI       , GL_RGBA_INTEGER,    GL_UNSIGNED_SHORT                }, //  TextureType_RGBA16UI	 = 33,
    {   GL_RGBA32UI       , GL_RGBA_INTEGER,    GL_UNSIGNED_INT                  }  //  TextureType_RGBA32UI	 = 34,
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

// for now only .jpg files supported
Texture CreateTexture(int width, int height, void* data, bool mipmap, TextureType type)
{
    Texture texture;
    glGenTextures(1, &texture.handle);
    glBindTexture(GL_TEXTURE_2D, texture.handle);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, mipmap ? GL_LINEAR_MIPMAP_NEAREST : GL_LINEAR);
    texture.width  = width;
    texture.height = height;
    
    TextureFormat format = TextureFormatTable[type];
    glTexImage2D(GL_TEXTURE_2D, 0, format.first, width, height, 0, format.format, format.type, data);
    
    if (mipmap)
        glGenerateMipmap(GL_TEXTURE_2D);
    
    CHECK_GL_ERROR();
    return texture;
}

Texture LoadTexture(const char* path, bool mipmap)
{
    int width, height, channels;
    unsigned char* image = nullptr;
    Texture defTexture;
    defTexture.width  = 32;
    defTexture.height = 32;
    defTexture.handle = defaultTexture;

    if (!FileExist(path))
    {
        AX_ERROR("image is not exist! %s", path);
        return defTexture;
    }
#ifdef __ANDROID__
    AAsset* asset = AAssetManager_open(g_android_app->activity->assetManager, path, 0);
    off_t size = AAsset_getLength(asset);
    unsigned char* buffer = (unsigned char*)malloc(size);
    AAsset_read(asset, buffer, size);
    image = stbi_load_from_memory(buffer, size, &width, &height, &channels, 0);
    free(buffer);
    AAsset_close(asset);
#else
    image = stbi_load(path, &width, &height, &channels, 0);
#endif
    if (image == nullptr)
    {
        AX_ERROR("image load failed! %s", path);
        return defTexture;
    }
    const TextureType numCompToFormat[5] = { 0, TextureType_R8, TextureType_RG8, TextureType_RGB8, TextureType_RGBA8 };
    Texture texture = CreateTexture(width, height, image, mipmap, numCompToFormat[channels]);
    // stbi_image_free(image);
    return texture;
}

void DeleteTexture(Texture texture) { glDeleteTextures(1, &texture.handle); }

/*//////////////////////////////////////////////////////////////////////////*/
/*                                 Mesh                                     */
/*//////////////////////////////////////////////////////////////////////////*/

inline GLenum ToGLType(GraphicType type)
{
    return GL_BYTE + (GLenum)type;
}

inline GLenum GLTypeToSize(GraphicType type)
{
    // BYTE, UNSIGNED_BYTE, SHORT, UNSIGNED_SHORT, INT, UNSIGNED_INT, FLOAT           
    const int TypeToSize[8]={ 1, 1, 2, 2, 4, 4, 4 };
    return TypeToSize[type];
}

inline char GLTFFilterToOGLFilter(char filter) {
    return (int)filter + 0x2600; // // GL_NEAREST 0x2600 9728, GL_LINEAR 0x2601 9729
}

inline unsigned int GLTFWrapToOGLWrap(int wrap) {
    unsigned int values[5] { 0x2901, 0x812F, 0x812D, 0x8370 }; // GL_REPEAT GL_CLAMP_TO_EDGE, GL_CLAMP_TO_BORDER, GL_MIRRORED_REPEAT
    ASSERT(wrap < 5 && "wrong or undefined sampler type!"); 
    return values[wrap];
}

Mesh CreateMesh(void* vertexBuffer, void* indexBuffer, int numVertex, int numIndex, GraphicType indexType, const InputLayoutDesc* layoutDesc)
{
    Mesh mesh;
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
        
        glVertexAttribPointer(i, layout.numComp, GL_BYTE + layout.type, i, layoutDesc->stride, offset); 
        glEnableVertexAttribArray(i);
        offset += layout.numComp * GLTypeToSize(layout.type);
    }
    CHECK_GL_ERROR();
    return mesh;
}

Mesh CreateMeshFromPrimitive(APrimitive* primitive)
{
    InputLayoutDesc desc;
    InputLayout inputLayout[6]{};
    desc.layout = inputLayout;
    desc.stride = 0;

    // Position 3, TexCoord 2, Normal 3, Tangent 3, TexCoord2 2
    const int attribIndexToNumComp[6] = { 3, 2, 3, 3, 2 };     
    int v = 0, attributes = primitive->attributes;
    
    for (int i = 0; attributes > 0; i += NextSetBit(&attributes), v++)
    {
        uint64_t size = sizeof(float) * attribIndexToNumComp[i];
        desc.layout[v].numComp = attribIndexToNumComp[v];
        desc.layout[v].type = GraphicType_Float;
        desc.stride += size;
    }

    desc.numLayout = v;
    Mesh mesh = CreateMesh(primitive->vertices, primitive->indices, primitive->numVertices, primitive->numIndices, primitive->indexType, &desc);
    return mesh;
}

void DeleteMesh(Mesh mesh)
{
    glDeleteVertexArrays(1, &mesh.vertexLayoutHandle);
    glDeleteBuffers(1, &mesh.vertexHandle);
    glDeleteBuffers(1, &mesh.indexHandle);
}

/*//////////////////////////////////////////////////////////////////////////*/
/*                                 Shader                                   */
/*//////////////////////////////////////////////////////////////////////////*/

static unsigned int currentShader = 0;

unsigned int GetUniformLocation(Shader shader, const char* name)
{
    return glGetUniformLocation(shader.handle, name);
}

void SetShaderValue(int   value, unsigned int location) { glUniform1i(location, value);}
void SetShaderValue(float value, unsigned int location) { glUniform1f(location, value);}

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
            AX_ERROR("Shader set value Graphic type invalid. type: %i", type);
            break;
    }
}

void SetMaterial(AMaterial* material)
{
    unsigned int onlyColorLoc = glGetUniformLocation(currentShader, "uOnlyColor");
    unsigned int colorLoc = glGetUniformLocation(currentShader, "uColor");
    
    int onlyColor = (int)(material->baseColorTexture.index == -1);
    SetShaderValue(onlyColor, onlyColorLoc);
    
    float color[4];
    UnpackColorRGBf(material->diffuseColor, color);
    SetShaderValue(color, colorLoc, GraphicType_Vector4f);
}

void CheckShaderError(uint shader)
{
    GLint isCompiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &isCompiled);
    if (isCompiled == GL_FALSE)
    {
        GLint maxLength = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);
        char infoLog[1024]{};
        glGetShaderInfoLog(shader, maxLength, &maxLength, infoLog);
        AX_ERROR("shader compile error: %s", infoLog);
        glDeleteShader(shader);
        DestroyRenderer();
    }
}

Shader LoadShader(const char* vertexSource, const char* fragmentSource)
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
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success); ASSERT(success);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    glUseProgram(shaderProgram);
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
    return LoadShader(vertexShaderSource, fragmentSource);
}

Shader ImportShader(const char* vertexPath, const char* fragmentPath)
{
    char* vertexText   = ReadAllFile(vertexPath, nullptr, nullptr, AX_SHADER_VERSION_PRECISION());
    char* fragmentText = ReadAllFile(fragmentPath, nullptr, nullptr, AX_SHADER_VERSION_PRECISION());
    
    Shader shader = LoadShader(vertexText, fragmentText);
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

void CreateDefaultTexture()
{
    unsigned char img[32*32*3]{};
    
    for (int i = 0; i < (32 * 32 * 3); i++)
    {
        bool columnOdd = (i & 15) < 8;
        img[i] = 200 * columnOdd;
    }
    defaultTexture = CreateTexture(32, 32, img, false, TextureType_RGB8).handle;
}

void InitRenderer()
{
    glEnable(GL_CULL_FACE);
    glFrontFace(GL_CCW);
    
#if defined(_DEBUG) || defined(DEBUG)
    // enable debug message
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageControl(GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_ERROR, GL_DEBUG_SEVERITY_LOW, 0, NULL, GL_TRUE);
    glDebugMessageCallback(GLDebugMessageCallback, nullptr);
#endif
    // create empty vao unfortunately this step is necessary for ogl 3.2
    glGenVertexArrays(1, &emptyVao);

    glClearColor(0.3f, 0.3f, 0.3f, 1.0f); 
    CreateDefaultTexture();
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

void RenderFullScreen(Shader fullScreenShader, unsigned int texture)
{
    glUseProgram(fullScreenShader.handle);
    glBindVertexArray(emptyVao);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glUniform1i(0, 0);// glUniform1i(glGetUniformLocation(fullScreenShader.handle, "tex"), 0);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    CHECK_GL_ERROR();
}

void BindShader(Shader shader)
{
    glUseProgram(shader.handle);
    currentShader = shader.handle;
}

void SetTexture(Texture texture, int index)
{
    glActiveTexture(GL_TEXTURE0 + index);
    glBindTexture(GL_TEXTURE_2D, texture.handle);
}

static Matrix4 modelViewProjection;
static Matrix4 modelMatrix;

void SetModelViewProjection(float* mvp) 
{
    SmallMemCpy(&modelViewProjection.m[0][0], mvp, 16 * sizeof(float)); 
    GLint mvpLoc   = glGetUniformLocation(currentShader, "mvp");
    glUniformMatrix4fv(mvpLoc  , 1, false, &modelViewProjection.m[0][0]);
}

void SetModelMatrix(float* model)       
{
    // Todo fix this, don't use uniform location like this
    SmallMemCpy(&modelMatrix.m[0][0], model, 16 * sizeof(float)); 
    GLint modelLoc = glGetUniformLocation(currentShader, "model");
    glUniformMatrix4fv(modelLoc, 1, false, &modelMatrix.m[0][0]);
}

void RenderMesh(Mesh mesh)
{
    glBindVertexArray(mesh.vertexLayoutHandle);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.indexHandle);
    glDrawElements(GL_TRIANGLES, mesh.numIndex, mesh.indexType, nullptr);
    CHECK_GL_ERROR();
}

void DestroyRenderer()
{

}
