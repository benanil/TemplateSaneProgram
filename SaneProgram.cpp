
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

ParsedScene scene;
Shader     shader;
Mesh*      meshes{};
Texture*   textures{};
Texture    forestTexture;
Camera     camera{};

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
static Shader fullScreenShader{0};


void AXInit()
{
    SetWindowName("Duck Window");
    // SetWindowSize(windowStartSize.x, windowStartSize.y);
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
    ParseGLTF("Meshes/Duck/Duck.gltf", &scene);
    
    if (scene.error != AError_NONE)
    {
        AX_ERROR("%s", ParsedSceneGetError(scene.error));
        return 0;
    }
    
    forestTexture    = LoadTexture("Textures/forest.jpg", false);
    fullScreenShader = CreateFullScreenShader(fragmentShaderSource);
    shader           = ImportShader("Shaders/3DFirstVert.glsl", "Shaders/3DFirstFrag.glsl");
    
    int numMeshes = 0;
    for (int i = 0; i < scene.numMeshes; i++)
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
    for (int i = 0; i < scene.numImages; i++)
        textures[i] = LoadTexture(scene.images[i].path, true);

    Vector2i windowStartSize;
    GetMonitorSize(&windowStartSize.x, &windowStartSize.y);

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

    camera.Update();
    Matrix4 model = Matrix4::Identity() * Matrix4::CreateScale(Vector3f::One() * 0.1f) * Matrix4::FromPosition(Vector3f::Zero());//Matrix4::PositionRotationScale(Vector3f::Zero(), Quaternion::Identity(), Vector3f::One() * 0.1f);
    Matrix4 mvp = model * camera.view * camera.projection;

    BindShader(shader);
    SetModelViewProjection(mvp.GetPtr());
    SetModelMatrix(model.GetPtr());

    SetTexture(textures[0], 0);
    RenderMesh(meshes[0]);
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

// camera.Update();
// for (int i = 0; i < scene.numNodes; i++) 
// {
//     ANode node = scene.nodes[i];
//     // if node is not mesh skip
//     if (node.type != 0) continue;
// 
//     Matrix4 model = Matrix4::PositionRotationScale(node.translation, node.rotation, node.scale);
//     Matrix4 mvp = model * camera.view * camera.projection;
//     
//     SetModelViewProjection(mvp.GetPtr());
//     SetModelMatrix(model.GetPtr());
// 
//     AMesh mesh = scene.meshes[node.index];
//     for (int j = 0; j < mesh.numPrimitives; ++j)
//     {
//         AMaterial material = scene.materials[mesh.primitives[j].material];
//         SetTexture(textures[material.textures[0].index], 0);
//         RenderMesh(meshes[node.index]);
//     }
// }