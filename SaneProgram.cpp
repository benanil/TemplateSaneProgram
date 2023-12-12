
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

ParsedObj objScene;
ParsedGLTF scene;

Mesh* objMeshes = nullptr;
Mesh* meshes = nullptr;

Shader     shader;
Texture*   textures = nullptr;
Texture    skyTexture;
Camera     camera{};

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
    ParseGLTF("Meshes/GroveStreet/GroveStreet.gltf", &scene);
    
    if (scene.error != AError_NONE)
    {
        AX_ERROR("%s", ParsedSceneGetError(scene.error));
        return 0;
    }
    // OptimizeScene(scene);

    ParseObj("Meshes/bunny.obj", &objScene);
    
    if (objScene.error != AError_NONE)
    {
        AX_ERROR("%s", ParsedSceneGetError(objScene.error));
        return 0;
    }

    skyTexture       = LoadTexture("Textures/orange-top-gradient-background.jpg", false);
    fullScreenShader = CreateFullScreenShader(fragmentShaderSource);
    shader           = ImportShader("Shaders/3DFirstVert.glsl", "Shaders/3DFirstFrag.glsl");
    colorUniform     = GetUniformLocation(shader, "uColor");

    int numMeshes = 0;
    for (int i = 0; i < scene.numMeshes; i++)
    {
        numMeshes += scene.meshes[i].numPrimitives;
    }
    
    numMeshes += objScene.numMeshes;

    meshes = new Mesh[numMeshes]{};

    int n = 0;
    for (int i = 0; i < scene.numMeshes; i++)
    {
        for (int j = 0; j < scene.meshes[i].numPrimitives; ++j, ++n)
        {
            meshes[n] = CreateMeshFromPrimitive(&scene.meshes[i].primitives[j]);
        }
    }
    
    objMeshes = new Mesh[objScene.numMeshes]{};

    for (int i = 0; i < objScene.numMeshes; i++)
    {
        objMeshes[i] = CreateMeshFromPrimitive(&objScene.meshes[i].primitives[0]);
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
    RenderFullScreen(fullScreenShader, skyTexture.handle);
    SetDepthTest(true);

    camera.Update();
    BindShader(shader);
    
    static Vector4f uColor{};
    static double _time = 0.0;

    uColor.w = sin(_time);
    _time += GetDeltaTime();

    int n = 0;
    for (int i = 0; i < scene.numNodes; i++)
    {
        ANode node = scene.nodes[i];
        // if node is not mesh skip
        if (node.type != 0) continue;

        Matrix4 model = Matrix4::PositionRotationScale(node.translation, node.rotation, node.scale);
        Matrix4 mvp = model * camera.view * camera.projection;

        SetModelViewProjection(mvp.GetPtr());
        SetModelMatrix(model.GetPtr());

        // SetShaderValue(&uColor.x, colorUniform, GraphicType_Vector4f); 

        AMesh mesh = scene.meshes[node.index];
        for (int j = 0; j < mesh.numPrimitives; ++j, n++)
        {
            AMaterial material = scene.materials[mesh.primitives[j].material];
            SetTexture(textures[material.textures[0].index], 0);
            RenderMesh(meshes[node.index]);
        }
    }

    Matrix4 model = Matrix4::PositionRotationScale(Vector3f::Zero(), Quaternion::Identity(), Vector3f::One());
    Matrix4 mvp = model * camera.view * camera.projection;

    SetModelViewProjection(mvp.GetPtr());
    SetModelMatrix(model.GetPtr());

    for (int i = 0; i < objScene.numMeshes; i++)
    {
        AMesh mesh = scene.meshes[i];
        RenderMesh(objMeshes[i]);
    }
    // todo material and light system
}

void AXExit()
{
    DeleteShader(shader);
    for (int i = 0; i < scene.numMeshes; i++) DeleteMesh(meshes[i]);
    for (int i = 0; i < scene.numImages; i++) DeleteTexture(textures[i]);
    delete[] meshes;
    delete[] textures;
    FreeParsedGLTF(&scene);
    FreeParsedObj(&objScene);
    DeleteShader(fullScreenShader);
    DestroyRenderer();
}
