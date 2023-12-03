
// #if defined(__ANDROID__) 
//     #include "PlatformAndroid.cpp"
// #else
//     #include "PlatformAndroid.cpp"
// #endif

#include <stdio.h>
#include "ASTL/Math/Matrix.hpp"

#include "Renderer.hpp"
#include "Camera.hpp"
#include "Platform.hpp"

const Vector2i windowStartSize{1920, 1080};

ParsedScene scene;
Shader     shader;
Mesh*      meshes{};
Texture*   textures{};
Texture    forestTexture;
Camera     camera;
Vector3f   meshPosition{};

const char* fragmentShaderSource = R"(
    #version 150 core
    out vec4 color;
    in vec2 texCoord;
    uniform sampler2D tex;
    void main() {
        color = texture(tex, texCoord);
    }
)";
static Shader fullScreenShader{0};

void AXInit()
{
    SetWindowName("Duck Window");
    SetWindowSize(windowStartSize.x, windowStartSize.y);
    SetWindowPosition(0, 0);
    SetVSync(true);
}

void WindowResizeCallback(int width, int height)
{
    camera.RecalculateProjection(width, height);
}

// return 1 if success
int AXStart()
{
    ParseGLTF("Meshes/GroveStreet/GroveStreet.gltf", &scene);
    
    if (scene.error != AError_NONE)
    {
        AX_ERROR("%s", ParsedSceneGetError(scene.error));
        return 0;
    }
    
    InitRenderer();
    forestTexture    = LoadTexture("Textures/forest.jpg", false);
    fullScreenShader = CreateFullScreenShader(fragmentShaderSource);
    shader           = ImportShader("Shaders/3DFirstVert.glsl", "Shaders/3DFirstFrag.glsl");
    
    int numMeshes = 0;
    for (int i = 0, n = 0; i < scene.numMeshes; i++)
    {
        numMeshes += scene.meshes[i].numPrimitives;
    }
    meshes = new Mesh[numMeshes]{};

    for (int i = 0, n = 0; i < scene.numMeshes; i++)
    {
        for (int j = 0; j < scene.meshes[i].numPrimitives; ++j, ++n)
        {
            meshes[n] = CreateMeshFromPrimitive(&scene.meshes[i].primitives[j]);
        }
    }

    textures = new Texture[scene.numImages]{};
    for (int i = 0, n = 0; i < scene.numImages; i++)
        textures[i] = LoadTexture(scene.images[i].path, true);
    
    camera.Init(windowStartSize);
    return 1;
}

// do rendering and main loop here
void AXLoop()
{
    SetDepthTest(false);
    // works like a skybox
    RenderFullScreen(fullScreenShader, forestTexture.handle);
    SetDepthTest(true);

    BindShader(shader);

    camera.Update();
    for (int i = 0; i < scene.numNodes; i++) 
    {
        ANode node = scene.nodes[i];
        // if node is not mesh skip
        if (node.type != 0) continue;
    
        Matrix4 model = Matrix4::PositionRotationScale(node.translation, node.rotation, node.scale);
        Matrix4 mvp = model * camera.view * camera.projection;
        
        SetModelViewProjection(mvp.GetPtr());
        SetModelMatrix(model.GetPtr());
    
        AMesh mesh = scene.meshes[node.index];
        for (int j = 0; j < mesh.numPrimitives; ++j)
        {
            AMaterial material = scene.materials[mesh.primitives[j].material];
            SetTexture(textures[material.textures[0].index], 0);
            RenderMesh(meshes[node.index]);
        }
    }
}

void AXExit()
{
    DeleteShader(shader);
    for (int i = 0; i < scene.numMeshes; i++) DeleteMesh(meshes[i]);
    for (int i = 0; i < scene.numImages; i++) DeleteTexture(textures[i]);
    delete[] meshes;
    delete[] textures;
    FreeParsedScene(&scene);
    DeleteShader(fullScreenShader);
    DestroyRenderer();
}