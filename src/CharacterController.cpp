
#include <math.h>

#include "include/CharacterController.hpp"
#include "include/Platform.hpp"
#include "include/SceneRenderer.hpp"
#include "include/Camera.hpp"
#include "include/UI.hpp"

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
    
    mPosition.y = -0.065f; // make foots touch the ground
    mMovementSpeed = 2.7f;
    mAnimSpeed = 1.0f;
    mIsAnimTrigerred = false;
    mIdleLimit = 8.0f;
    mIdleTime = 0.0f;
    {
        mRootNodeIdx = FindRootNodeIndex(_character);
        float* posPtr = _character->nodes[mRootNodeIdx].translation;
        float* rotPtr = _character->nodes[mRootNodeIdx].rotation;
    
        mStartPos = MakeVec3(posPtr);
        mStartRotation = VecLoad(rotPtr);
    }
    
    int a_idle           = FindAnimIndex(_character, "Idle");
    int a_walk           = FindAnimIndex(_character, "Walk");
    int a_jog_forward    = FindAnimIndex(_character, "Jog_Forward");
    int a_jog_backward   = a_jog_forward; // FindAnimIndex(_character, "Jog_Backward");
    int a_diagonal_left  = FindAnimIndex(_character, "Jog_Forward_Left");
    int a_diagonal_right = FindAnimIndex(_character, "Jog_Forward_Right");
    int a_strafe_left    = FindAnimIndex(_character, "Strafe_Left");
    int a_strafe_right   = FindAnimIndex(_character, "Strafe_Right");

    mAtackIndex = FindAnimIndex(_character, "Sword_Attack");
    mIdle2Index = FindAnimIndex(_character, "Idle_Look_Around");
    mJumpIndex  = FindAnimIndex(_character, "Jump");
    mJumpWalkingIndex  = FindAnimIndex(_character, "Jump_Walking");

    // idle, left&right strafe
    mAnimController.SetAnim(a_left  , 0, a_strafe_left);
    mAnimController.SetAnim(a_middle, 0, a_idle);
    mAnimController.SetAnim(a_right , 0, a_strafe_right);
    // walk, jog, run
    mAnimController.SetAnim(a_middle, 1, a_walk);
    mAnimController.SetAnim(a_middle, 2, a_jog_forward);

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

void CharacterController::ColissionDetection(Vector3f oldPos)
{
    const Vector2f boundingMin = { -27.8f, -11.5f };
    const Vector2f boundingMax = {  25.5f,  10.5f };
    
    mPosition.x = Clamp(mPosition.x, boundingMin.x, boundingMax.x);
    mPosition.z = Clamp(mPosition.z, boundingMin.y, boundingMax.y);

    // column is banner parts of sponza ||
    // Right Columns: 18.0, 0.0, -5.0
    //               -20.0, 0.0, -5.0
    // Left Columns:  18.0, 0.0, 3.8
    //               -20.0, 0.0, 3.8
    bool xBetweenColumns = mPosition.x < 19.0f && mPosition.x > -21.0;
    
    bool zIsLeft  = Abs(mPosition.z - -5.0f) < 1.0f;
    bool zIsRight = Abs(mPosition.z -  3.8f) < 1.0f;
    
    bool characterInBanners = xBetweenColumns & (zIsLeft || zIsRight);
    if (characterInBanners)
    {
        float xDiff = (oldPos.z - mPosition.z) * 0.5f;
        // set z position to old position and add opposite direction movement
        mPosition.z = oldPos.z + xDiff;
    }
}

void CharacterController::TriggerAnim(int index)
{
    if (mIsAnimTrigerred) return; // already trigerred
    mIsAnimTrigerred = true;
    mTriggerredAnim = index;
    mCurrentMovement = Vector2f::Zero();
    mAnimTime.y = 0.0f;
}

void CharacterController::Update(float deltaTime, bool isSponza)
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
    if (!mWasPressing && pressing) {
        mTouchStart = cursorPos;
    }

    mWasPressing = pressing;
    if (pressing)
        // use rsqrt and mul instead of sqrt and div, to avoid devide zero by zero
        targetMovement = Vector2f::NormalizeEst(mTouchStart - cursorPos);  
    
    targetMovement = Min(targetMovement, MakeVec2( 1.0f,  1.0f));
    targetMovement = Max(targetMovement, MakeVec2(-1.0f, -1.0f));
    targetMovement.x = -targetMovement.x;
    //------------------------------------------------------------------------
#endif
    mAnimTime.y += deltaTime * (mAnimSpeed * 0.87f);
    mAnimTime.y = !mIsAnimTrigerred ? Fract(mAnimTime.y) : mAnimTime.y;
    
    mAnimTime.x += deltaTime * (mAnimSpeed * 0.87f);
    mAnimTime.x = Fract(mAnimTime.x);

    float* posPtr = mCharacter->nodes[mRootNodeIdx].translation;
    float* rotPtr = mCharacter->nodes[mRootNodeIdx].rotation;
    
    SmallMemCpy(posPtr, &mStartPos.x, sizeof(Vector3f));
    VecStore(rotPtr, mStartRotation);

    if (!mIsAnimTrigerred) {
        mAnimController.EvaluateLocomotion(mCharacter,
                                           mCurrentMovement.x,
                                           mCurrentMovement.y,
                                           mAnimTime.x,
                                           mAnimTime.y);
    }
    else {
        if (mAnimTime.y >= 1.0f)
        {
            mAnimTime.y = 0.0f;
            mIsAnimTrigerred = false;
            mTriggerredAnim = mIdle2Index;
            mIdleTime = 0.0f;
            mPosition.y = -0.065f;
        }
        mAnimController.PlayAnim(mCharacter, mTriggerredAnim, mAnimTime.y);
    }

    isRunning |= GetKeyDown(Key_SHIFT);
    targetMovement.y += (float)isRunning;
    float animAddition = (float)isRunning * 0.55f;
    mAnimSpeed = Lerp(mAnimSpeed, 0.9f + animAddition, 0.1f);
    
    if (mIsAnimTrigerred && mTriggerredAnim == mIdle2Index)
        mAnimSpeed = 0.2f;

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

    x += -inputAngle / 2.0f;
    // handle character position
    {   
        bool isWalkJumping = mTriggerredAnim == mJumpWalkingIndex;
        if (mIsAnimTrigerred == false || isWalkJumping)
        {
            Vector3f forward = MakeVec3(Sin(x), 0.0f, Cos(x));
            Vector3f progress = forward * -mCurrentMovement.Length() * mMovementSpeed * deltaTime;
            Vector3f oldPos = mPosition;
            mPosition += progress * mAnimSpeed * 1.5f;
            
            if (isSponza)
                ColissionDetection(oldPos);
        }
        
        // animatedPos.y = 8.15f; // if you want to walk on top floor
        camera->targetPos = mPosition;
        // set animated pos for the renderer
        SmallMemCpy(posPtr, &mPosition.x, sizeof(Vector3f));
    }
    SceneRenderer::SetCharacterPos(mPosition.x, mPosition.y, mPosition.z);

    if (GetMousePressed(MouseButton_Left) && !mIsAnimTrigerred) {
        TriggerAnim(mAtackIndex);
    }

    if (GetKeyPressed(Key_SPACE)) {
        int anim = mCurrentMovement.y > 0.15f ? mJumpWalkingIndex : mJumpIndex;
        TriggerAnim(anim);
    }

    if (!mIsAnimTrigerred && Abs(mCurrentMovement.x) + Abs(mCurrentMovement.y) < 0.05f) 
    {
        mIdleTime += deltaTime;
    }
    else {
        mIdleTime = 0.0f;
    }

    if (mIdleTime >= mIdleLimit)
    {
        TriggerAnim(mIdle2Index);
        mIdleTime = 0.0f;
        mIdleLimit = 6.0f + (Random::NextFloat01(Random::Seed32()) * 15.0f);
    }
}

void CharacterController::Destroy()
{
    ClearAnimationController(&mAnimController);
}
