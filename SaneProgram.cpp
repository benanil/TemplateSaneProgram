
// #if defined(__ANDROID__) 
//     #include "PlatformAndroid.cpp"
// 
// #else
//     #include "PlatformAndroid.cpp"
// #endif

#include <stdio.h>
#include "ASTL/Math/Matrix.hpp"

#include "Renderer.hpp"
#include "Camera.hpp"
#include "Platform.hpp"

const Vector2i windowStartSize{1920, 1080};

ParsedGLTF gltf;
Shader     shader;
Mesh       mesh;
Texture    texture;
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
}

void WindowResizeCallback(int width, int height)
{
    camera.RecalculateProjection(width, height);
}

// return 1 if success
int AXStart()
{
    gltf = ParseGLTF("Meshes/Duck.gltf");
    ASSERT(gltf.error == GLTFError_NONE);
    if (gltf.error != GLTFError_NONE) return 0;
    
    InitRenderer();
    forestTexture    = LoadTexture("Textures/forest.jpg", false);
    fullScreenShader = CreateFullScreenShader(fragmentShaderSource);
    shader           = ImportShader("Shaders/3DFirstVert.glsl", "Shaders/3DFirstFrag.glsl");
    mesh             = CreateMeshFromGLTF(&gltf.meshes[0].primitives[0]);
    texture          = LoadTexture(gltf.images[0].path, true);
    
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
    if (GetMousePressed(MouseButton_Left))  meshPosition.x += 2.0f;
    if (GetMousePressed(MouseButton_Right)) meshPosition.x -= 2.0f;

    camera.Update();
    Matrix4 model = Matrix4::CreateScale(0.05f, 0.05f, 0.05f) * Matrix4::FromPosition(meshPosition);
    Matrix4 mvp = model * camera.view * camera.projection;

    SetModelViewProjection(mvp.GetPtr());
    SetModelMatrix(model.GetPtr());

    SetTexture(texture, 0);
    RenderMesh(mesh);
}

void AXExit()
{
    DeleteShader(shader);
    DeleteMesh(mesh);
    DeleteTexture(texture);
    FreeGLTF(gltf);
    DeleteShader(fullScreenShader);
    DestroyRenderer();
}

// in update before RenderFunction
// for (int i = 0; i < gltf.numNodes; i++) 
// {
//     GLTFNode node = gltf.nodes[i];
//     // in this scene first mesh is shitty so pass that
//     if (node.type != 0) continue;
// 
//     model = Matrix4::CreateScale(1.0f, 1.0f, 1.0f) * Matrix4::FromQuaternion(node.rotation) * Matrix4::FromPosition(node.translation);
//     mvp = model * view * projection;
//     
//     SetModelViewProjection(&mvp.m[0][0]);
//     SetModelMatrix(&model.m[0][0]);
// 
//     GLTFMesh mesh = gltf.meshes[node.index];
//     for (int j = 0; j < mesh.numPrimitives; ++j)
//     {
//         GLTFMaterial material = gltf.materials[mesh.primitives[j].material];
//         SetTexture(textures[0], 0); //SetTexture(textures[material.textures[0].index], 0);
//         RenderMesh(meshes[node.index]);
//     }
// }