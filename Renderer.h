#ifndef ANDROIDGLINVESTIGATIONS_RENDERER_H
#define ANDROIDGLINVESTIGATIONS_RENDERER_H

struct Shader
{
    unsigned int handle;
};

struct Texture
{
    int width, height;
    unsigned int handle;
};

enum GraphicType : int
{
    Float, Int, Uint, Byte
};

struct VertexAttribute
{
    GraphicType type;
    int numComponents; // float = 1; vec2 = 2; vec3 = 3
};

struct Mesh
{
    int numVertex, numIndex;
    unsigned int vertexHandle, indexHandle;
    unsigned int vertexLayoutHandle;
};

Mesh CreateMesh(void* vertexBuffer, void* indexBuffer, int numVertex, int numIndex, int vertexSize);

Texture CreateTexture(int width, int height, void* data);

Texture CreateTexture(const char* path);

Shader LoadShader(const char* vertexSource, const char* fragmentSource);

Shader ImportShder(const char* vertexSource, const char* fragmentSource);

void DeleteTexture(Texture texture);

void DeleteShader(Shader shader);

void DeleteMesh(Mesh mesh);

void HandleInput();

void InitRenderer();

void DestroyRenderer();

void UpdateRenderArea();

extern struct android_app* g_android_app;

#endif //ANDROIDGLINVESTIGATIONS_RENDERER_H