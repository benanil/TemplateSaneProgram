
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
    return 0;
}

static Vector2i monitorSize;

void CharacterController::Start(Prefab* _character)
{
    wGetMonitorSize(&monitorSize.x, &monitorSize.y);

    mCharacter  = _character;
    mTouchStart = MakeVec2(0.0f, 0.0f);
    MemsetZero(&mAnimController, sizeof(AnimationController));
    CreateAnimationController(_character, &mAnimController);
    
    mMovementSpeed = 2.7f;
    mAnimSpeed = 1.0f;
    {
        ASkin& skin = _character->skins[0];
    
        int skeletonNode = _character->GetRootNodeIdx();
        mRootNodeIdx = skin.skeleton != -1 ? skin.skeleton : skeletonNode; 

        float* posPtr = _character->nodes[mRootNodeIdx].translation;
        float* rotPtr = _character->nodes[mRootNodeIdx].rotation;
    
        mStartPos = MakeVec3(posPtr);
        mStartRotation = VecLoad(rotPtr);
    }
    
    int a_idle           = FindAnimIndex(_character, "idle_short");
    int a_walk           = FindAnimIndex(_character, "walk");
    int a_jog_forward    = FindAnimIndex(_character, "jog_forward");
    int a_jog_backward   = FindAnimIndex(_character, "jog_backward");
    int a_run            = FindAnimIndex(_character, "run_fast");
    int a_diagonal_left  = FindAnimIndex(_character, "jog_left");
    int a_diagonal_right = FindAnimIndex(_character, "jog_right");
    int a_strafe_left    = FindAnimIndex(_character, "strafe_left");
    int a_strafe_right   = FindAnimIndex(_character, "strafe_right");

    // idle, left&right strafe
    mAnimController.SetAnim(a_left  , 0, a_strafe_left);
    mAnimController.SetAnim(a_middle, 0, a_idle);
    mAnimController.SetAnim(a_right , 0, a_strafe_right);
    // walk, jog, run
    mAnimController.SetAnim(a_middle, 1, a_walk);
    mAnimController.SetAnim(a_middle, 2, a_jog_forward);

    // set last row to all run anim
    mAnimController.SetAnim(a_left  , 3, a_run);
    mAnimController.SetAnim(a_middle, 3, a_run);
    mAnimController.SetAnim(a_right , 3, a_run);
    // set second row to idle 
    mAnimController.SetAnim(a_left      , 1, a_diagonal_left);
    mAnimController.SetAnim(a_right     , 1, a_diagonal_right);
    
    // set first row to idle 
    mAnimController.SetAnim(a_left      , 0, a_diagonal_left);
    mAnimController.SetAnim(a_right     , 0, a_diagonal_right);
 
    // copy first row to inverse first row
    SmallMemCpy(mAnimController.locomotionIndicesInv[0],
                mAnimController.locomotionIndices[0], sizeof(int) * 5);

    mAnimController.SetAnim(a_left  , -1, a_diagonal_left);
    mAnimController.SetAnim(a_middle, -1, a_jog_backward);
    mAnimController.SetAnim(a_right , -1, a_diagonal_right);
}

void CharacterController::Update(float deltaTime)
{
    Vector2f targetMovement = {0.0f, 0.0f};
    bool isRunning = false;
#ifndef __ANDROID__ /* NOT android */
    if (GetKeyDown('W')) targetMovement.y = +1.0f; 
    if (GetKeyDown('S')) targetMovement.y = -1.0f; 
    if (GetKeyDown('A')) targetMovement.x = -1.0f; 
    if (GetKeyDown('D')) targetMovement.x = +1.0f; 
#else
    // Joystick
    //------------------------------------------------------------------------
    wGetMonitorSize(&monitorSize.x, &monitorSize.y);
    int numTouch = NumTouchPressing();
    Touch touch0 = GetTouch(0);
    Touch touch1 = GetTouch(1);

    Vector2f cursorPos = mTouchStart;
    bool interacted = false;
    // left side is for movement, right side is for rotating camera
    // if num touch is 1 and its right side of the screen
    if (numTouch == 1 && touch0.positionX < (monitorSize.x / 2.0f))
    {
        interacted = true;
    }
    else if (numTouch > 1)
    {
        if (touch1.positionX < touch0.positionX) // select smallest one
            Swap(touch0, touch1);
        interacted = true;
    }
    cursorPos = { touch0.positionX, touch0.positionY };

    bool pressing = numTouch > 0 && interacted;
    if (!wasPressing && pressing) {
        mTouchStart = cursorPos;
    }

    wasPressing = pressing;
    if (pressing)
        // use rsqrt and mul instead of sqrt and div, to avoid devide zero by zero
        targetMovement = Vector2f::NormalizeEst(mTouchStart - cursorPos);  
    
    targetMovement = Min(targetMovement, MakeVec2( 1.0f,  1.0f));
    targetMovement = Max(targetMovement, MakeVec2(-1.0f, -1.0f));
    targetMovement.x = -targetMovement.x;
    //------------------------------------------------------------------------
#endif
    mAnimTime.y += deltaTime * (mAnimSpeed * 0.87f);
    mAnimTime.y = Fract(mAnimTime.y);
    
    mAnimTime.x += deltaTime * (mAnimSpeed * 0.87f);
    mAnimTime.x = Fract(mAnimTime.x);

    float* posPtr = mCharacter->nodes[mRootNodeIdx].translation;
    float* rotPtr = mCharacter->nodes[mRootNodeIdx].rotation;
    
    SmallMemCpy(posPtr, &mStartPos.x, sizeof(Vector3f));
    VecStore(rotPtr, mStartRotation);

    EvaluateAnimOfPrefab(mCharacter,
                        &mAnimController,
                         mCurrentMovement.x,
                         mCurrentMovement.y,
                         mAnimTime.x,
                         mAnimTime.y);

    isRunning  |= GetKeyDown(Key_SHIFT);
    bool isSprinting = GetKeyDown(Key_CONTROL);
    targetMovement.y += (float)isRunning + (float)isSprinting;

    float animAddition = (float)isRunning * 0.45f + (float)isSprinting * 0.2f;
    mAnimSpeed = Lerp(mAnimSpeed, 0.9f + animAddition, 0.1f);

    const float smoothTime = 0.25f;

    mCurrentMovement.y = SmoothDamp(mCurrentMovement.y, targetMovement.y, mSpeedSmoothVelocity, smoothTime, 9999.0f, deltaTime); // Lerp(mCurrentMovement.y, targetMovement.y, acceleration * deltaTime); //
    mCurrentMovement.x = Lerp(mCurrentMovement.x, targetMovement.x, 4.0f * deltaTime);

    Camera* camera = SceneRenderer::GetCamera();
    // angle of user input, (keyboard or joystick)
    float x = camera->angle.x * -TwoPI;
    float inputAngle = atan2f(targetMovement.x, MIN(targetMovement.y, 1.0f));
    x += -inputAngle / 2.0f;

    // handle camera rotation
    if (mCurrentMovement.y > 0.1f)
    {
        const float rotationSpeed = 12.0f;
        Quaternion targetRotation = QFromAxisAngle(MakeVec3(0.0f, 1.0f, 0.0f), x + PI);
        mRotation = QSlerp(mRotation, targetRotation, deltaTime * rotationSpeed);
    }
    VecStore(rotPtr, mRotation);

    const float sprintMultiplier = 2.0f;
    targetMovement.y += (float)isSprinting * sprintMultiplier;
    x += -inputAngle / 2.0f;
    // handle character position
    {   
        Vector3f forward = MakeVec3(sinf(x), 0.0f, cosf(x));
        
        Vector3f progress = forward * -mCurrentMovement.Length() * mMovementSpeed * deltaTime;
        
        mPosition += progress * mAnimSpeed * 1.5f;
        // animatedPos.y = 8.15f;
        camera->targetPos = mPosition;
        // set animated pos for the renderer
        SmallMemCpy(posPtr, &mPosition.x, sizeof(Vector3f));
    }
    SceneRenderer::SetCharacterPos(mPosition.x, mPosition.y, mPosition.z);
}

void CharacterController::Destroy()
{
    ClearAnimationController(&mAnimController);
}
