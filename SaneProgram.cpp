
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

ParsedGLTF gltf;
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
    gltf = ParseGLTF("Meshes/GroveStreet.gltf");
    ASSERT(gltf.error == GLTFError_NONE);
    if (gltf.error != GLTFError_NONE) return 0;
    
    InitRenderer();
    forestTexture    = LoadTexture("Textures/forest.jpg", false);
    fullScreenShader = CreateFullScreenShader(fragmentShaderSource);
    shader           = ImportShader("Shaders/3DFirstVert.glsl", "Shaders/3DFirstFrag.glsl");
    
    int numMeshes = 0;
    for (int i = 0, n = 0; i < gltf.numMeshes; i++)
    {
        numMeshes += gltf.meshes[i].numPrimitives;
    }
    meshes = new Mesh[numMeshes]{};

    for (int i = 0, n = 0; i < gltf.numMeshes; i++)
    {
        for (int j = 0; j < gltf.meshes[i].numPrimitives; ++j, ++n)
        {
            meshes[n] = CreateMeshFromGLTF(&gltf.meshes[i].primitives[j]);
        }
    }

    textures = new Texture[gltf.numImages]{};
    for (int i = 0, n = 0; i < gltf.numImages; i++)
        textures[i] = LoadTexture(gltf.images[i].path, true);
    
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
    for (int i = 0; i < gltf.numNodes; i++) 
    {
        GLTFNode node = gltf.nodes[i];
        // if node is not mesh skip
        if (node.type != 0) continue;
    
        Matrix4 model = Matrix4::PositionRotationScale(node.translation, node.rotation, node.scale);
        Matrix4 mvp = model * camera.view * camera.projection;
        
        SetModelViewProjection(mvp.GetPtr());
        SetModelMatrix(model.GetPtr());
    
        GLTFMesh mesh = gltf.meshes[node.index];
        for (int j = 0; j < mesh.numPrimitives; ++j)
        {
            GLTFMaterial material = gltf.materials[mesh.primitives[j].material];
            SetTexture(textures[material.textures[0].index], 0);
            RenderMesh(meshes[node.index]);
        }
    }
}

void AXExit()
{
    DeleteShader(shader);
    for (int i = 0; i < gltf.numMeshes; i++) DeleteMesh(meshes[i]);
    for (int i = 0; i < gltf.numImages; i++) DeleteTexture(textures[i]);
    delete[] meshes;
    delete[] textures;
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