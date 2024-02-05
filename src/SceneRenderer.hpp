
struct Scene;
struct SubScene;

namespace SceneRenderer
{
    void Init();

    void BeginRendering();

    void RenderSubScene(Scene* scene, SubSceneID subsceneId);
    
    void PostProcessPass();

    void EndRendering();

    void Destroy();
}