
// #if defined(__ANDROID__) 
//     #include "PlatformAndroid.cpp"
// #else
//     #include "PlatformAndroid.cpp"
// #endif


#include <stdio.h>
#include <math.h> // sin
#include "ASTL/Math/Matrix.hpp"
#include "ASTL/Additional/GLTFParser.hpp"

#include "Renderer.hpp"
#include "Camera.hpp"
#include "Platform.hpp"
#include "AssetManager.hpp"
#include "ASTL/String.hpp"
#include "ASTL/Array.hpp"
#include "ASTL/IO.hpp"

struct Scene
{
    ParsedGLTF data;
    Mesh* meshes;
    Texture* textures;
};

Scene GLTFScene{};
Scene OBJScene{};
Scene FBXScene{};

Shader shader;
       
Texture skyTexture;
Camera camera{};

unsigned int colorUniform;

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

void LoadSceneMeshesAndTexturesToGPU(Scene* scene)
{
    ASSERT(scene != nullptr);
    ParsedGLTF& data = scene->data;

    int numMeshes = 0;
    for (int i = 0; i < data.numMeshes; i++)
        numMeshes += data.meshes[i].numPrimitives;

    scene->meshes = numMeshes ? new Mesh[numMeshes]{} : nullptr;

    for (int i = 0; i < numMeshes; i++)
    {
        scene->meshes[i] = CreateMeshFromPrimitive(&data.meshes[i].primitives[0]);
    }

    scene->textures = data.numImages ? new Texture[data.numImages]{} : nullptr;
    for (int i = 0; i < data.numImages; i++)
    {
        if (data.images[i].path)
            scene->textures[i] = LoadTexture(data.images[i].path, true);
    }
}

int ImportScene(Scene* scene, const char* path)
{
    int length = StringLength(path);
    if (FileHasExtension(path, length, "fbx"))  return LoadFBX(path, &scene->data);
    if (FileHasExtension(path, length, "abm"))  return LoadGLTFBinary(path, &scene->data);
    if (FileHasExtension(path, length, "gltf")) return ParseGLTF(path, &scene->data);
    return 0;
}

void DestroyScene(Scene* scene)
{
    ParsedGLTF& data = scene->data;

    for (int i = 0; i < data.numMeshes; i++) DeleteMesh(scene->meshes[i]);
    for (int i = 0; i < data.numTextures; i++) DeleteTexture(scene->textures[i]);

    delete[] scene->meshes;
    delete[] scene->textures;

    FreeParsedGLTF(&scene->data);
}

void AXInit()
{
    SetWindowName("Duck Window");
    SetWindowSize(1920, 1080);

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
    
    if (!ImportScene(&GLTFScene, "Meshes/GroveStreet/GroveStreet.abm"))
    {
        AX_ERROR("gltf binary mesh load failed");
        return 0;
    }
    
    if (!ImportScene(&FBXScene, "Meshes/wooden_windmill.fbx"))
    {
        AX_ERROR("fbx mesh load failed");
        return 0;
    }
    
    LoadSceneMeshesAndTexturesToGPU(&GLTFScene);
    LoadSceneMeshesAndTexturesToGPU(&FBXScene);

    skyTexture       = LoadTexture("Textures/orange-top-gradient-background.jpg", false);
    fullScreenShader = CreateFullScreenShader(fragmentShaderSource);
    shader           = ImportShader("Shaders/3DFirstVert.glsl", "Shaders/3DFirstFrag.glsl");
    colorUniform     = GetUniformLocation(shader, "uColor");

    Vector2i windowStartSize;
    GetMonitorSize(&windowStartSize.x, &windowStartSize.y);

    camera.Init(windowStartSize);
    return 1;
}

void RenderScene(Scene* scene)
{
    ParsedGLTF& data = scene->data;
    for (int i = 0; i < data.numNodes; i++)
    {
        ANode node = data.nodes[i];
        // if node is not mesh skip (camera)
        if (node.type != 0 || node.index == -1) continue;

        Matrix4 model = Matrix4::PositionRotationScale(node.translation, node.rotation, node.scale);
        Matrix4 mvp = model * camera.view * camera.projection;

        SetModelViewProjection(mvp.GetPtr());
        SetModelMatrix(model.GetPtr());

        AMesh mesh = data.meshes[node.index];
        
        for (int j = 0; j < mesh.numPrimitives; ++j)
        {
            if (scene->meshes[node.index].numIndex == 0)
                continue;

            AMaterial material = data.materials[mesh.primitives[j].material];
            SetMaterial(&material);
            
            if (material.baseColorTexture.index != -1) 
                SetTexture(scene->textures[material.baseColorTexture.index], 0);
    
            RenderMesh(scene->meshes[node.index]);
        }
    }
}

// do rendering and main loop here
void AXLoop()
{
    SetDepthTest(false);
    // works like a skybox
    RenderFullScreen(fullScreenShader, skyTexture.handle);
    SetDepthTest(true);

    camera.Update();
    BindShader(shader);
    
    RenderScene(&GLTFScene);

    RenderScene(&FBXScene);

    // todo material and light system
}

void AXExit()
{
    DeleteShader(shader);

    DestroyScene(&FBXScene);
    DestroyScene(&GLTFScene);

    DeleteShader(fullScreenShader);
    
    DestroyRenderer();
}
