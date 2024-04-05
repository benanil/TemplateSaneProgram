
#include "CharacterController.hpp"
#include "Platform.hpp"
#include "SceneRenderer.hpp"
#include "Camera.hpp"

#include "../ASTL/String.hpp" // StringEqual

static int FindAnimIndex(Prefab* prefab, const char* name)
{
    int len = StringLength(name);
    for (int i = 0; i < prefab->numAnimations; i++)
    {
        if (StringEqual(prefab->animations[i].name, name, len))
            return i;
    }
    ASSERT(0);
    return -1;
}

void CharacterController::Start(Prefab* _character)
{
    character = _character;
    MemsetZero(&animationController, sizeof(AnimationController));
    CreateAnimationController(_character, &animationController);
    
    {
        ASkin& skin = _character->skins[0];
    
        int skeletonNode = _character->GetRootNodeIdx();
        m_rootNodeIdx = skin.skeleton != -1 ? skin.skeleton : skeletonNode; 

        float* posPtr = _character->nodes[m_rootNodeIdx].translation;
        float* rotPtr = _character->nodes[m_rootNodeIdx].rotation;
    
        startPos = MakeVec3(posPtr);
        startRotation = VecLoad(rotPtr);
    }
    
    int a_idle           = FindAnimIndex(_character, "idle_short");
    int a_walk           = FindAnimIndex(_character, "walk");
    int a_jog_forward    = FindAnimIndex(_character, "jog_forward");
    int a_run            = FindAnimIndex(_character, "run_fast");
    int a_diagonal_left  = FindAnimIndex(_character, "jog_left");
    int a_diagonal_right = FindAnimIndex(_character, "jog_right");
    
    // index: 3 is middle, 0 is left most, 4 is right most
    animationController.SetAnim(a_middle, 0, a_idle);
    animationController.SetAnim(a_middle, 1, a_walk);
    animationController.SetAnim(a_middle, 2, a_jog_forward);
    animationController.SetAnim(a_middle, 3, a_run);
    
    // set second row to idle 
    animationController.SetAnim(a_left_most , 1, a_diagonal_left);
    animationController.SetAnim(a_left      , 1, a_diagonal_left);
    animationController.SetAnim(a_right_most, 1, a_diagonal_right);
    animationController.SetAnim(a_right     , 1, a_diagonal_right);
    
    // set first row to idle 
    animationController.SetAnim(a_left_most , 0, a_diagonal_left);
    animationController.SetAnim(a_left      , 0, a_diagonal_left);
    animationController.SetAnim(a_right     , 0, a_diagonal_right);
    animationController.SetAnim(a_right_most, 0, a_diagonal_right);
}

void CharacterController::Update(float deltaTime)
{
    float movement = 0.0f;

    if (GetKeyDown('W')) movement = -1.0f; 
    if (GetKeyDown('S')) movement =  1.0f; 

    animTime += deltaTime;
    animTime = Fract(animTime);
    
    float* posPtr = character->nodes[m_rootNodeIdx].translation;
    float* rotPtr = character->nodes[m_rootNodeIdx].rotation;
    
    SmallMemCpy(posPtr, &startPos.x, sizeof(Vector3f));
    VecStore(rotPtr, startRotation);

    EvaluateAnimOfPrefab(character, &animationController, animTarget.x, -movement, animTime);

    {
        Camera* camera = SceneRenderer::GetCamera();
        float x = camera->angle.x * -TwoPI;

        Quaternion rotation = QFromAxisAngle(MakeVec3(0.0f, 1.0f, 0.0f), x + PI);
        VecStore(rotPtr, rotation);

        float progress = movement * deltaTime * 2.7f;
        x = camera->angle.x * -TwoPI;

        Vector3f dir = { sinf(x), 0.0f, cosf(x) };
        animatedPos += (dir * progress);

        camera->targetPos = animatedPos;

        SmallMemCpy(posPtr, &animatedPos.x, sizeof(Vector3f));
    }
}

void CharacterController::Destroy()
{
    ClearAnimationController(&animationController);
}
