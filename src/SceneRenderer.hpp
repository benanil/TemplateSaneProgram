
struct Scene;
struct SubScene;
struct LightInstance;

namespace SceneRenderer
{
    void Init();

    void BeginRendering();

    void RenderSubScene(Scene* scene, SubSceneID subsceneId);
    
    void PostProcessPass();

    void EndRendering();

    void BeginUpdateLights();
    
    void UpdateLight(int index, LightInstance* instance);

    void EndUpdateLights();

    void Destroy();
}