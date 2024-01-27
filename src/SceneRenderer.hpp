
struct Scene;
struct SubScene;

namespace SceneRenderer
{
    void Init();

    void RenderShadows(SubScene* scene, SubSceneID subsceneId);

    void RenderSubScene(Scene* scene, SubSceneID subsceneId);

    void Destroy();
}