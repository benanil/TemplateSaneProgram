
#include <math.h> // atan2f

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
        if (StringEqual(prefab->animations[i].name, name, len))
            return i;
    AX_WARN("couldn't find animation from name %s", name);
    return 0;
}

static Vector2i monitorSize;

void CharacterController::Start(Prefab* _character)
{
    wGetMonitorSize(&monitorSize.x, &monitorSize.y);

    mCharacter  = _character;
    mTouchStart = MakeVec2(0.0f, 0.0f);
    // we don't need to set zero the poses
    constexpr size_t poseSize = sizeof(AnimationController::mAnimPoseA) + sizeof(AnimationController::mAnimPoseB);
    MemsetZero(&mAnimController, sizeof(AnimationController) - poseSize);
    CreateAnimationController(_character, &mAnimController, true, 58);
    
    mRandomState = Random::Seed32();

    mPosition.y = -0.065f; // make foots touch the ground
    mMovementSpeed = 2.7f;
    mIdleLimit = 8.0f;
    mIdleTime = 0.0f;
    {
        mRootNodeIdx = Prefab::FindAnimRootNodeIndex(_character);
        float* posPtr = _character->nodes[mRootNodeIdx].translation;
        float* rotPtr = _character->nodes[mRootNodeIdx].rotation;
    
        mStartPos = MakeVec3(posPtr);
        mStartRotation = VecLoad(rotPtr);
    }
    
    int a_idle           = FindAnimIndex(_character, "Idle");
    int a_walk           = FindAnimIndex(_character, "Walk");
    int a_jog_forward    = FindAnimIndex(_character, "Run");
    int a_jog_backward   = a_jog_forward; // FindAnimIndex(_character, "Jog_Backward");
    // int a_diagonal_left  = FindAnimIndex(_character, "Jog_Forward_Left");
    // int a_diagonal_right = FindAnimIndex(_character, "Jog_Forward_Right");
    int a_strafe_left    = FindAnimIndex(_character, "StrafeLeft");
    int a_strafe_right   = a_strafe_left;

    mAtackIndex  = FindAnimIndex(_character, "Slash2");
    mIdle2Index  = FindAnimIndex(_character, "Idle2");
    mJumpIndex   = FindAnimIndex(_character, "Jump");
    mImpactIndex = FindAnimIndex(_character, "Impact");
    mKickIndex   = FindAnimIndex(_character, "Kick");

    // idle, left&right strafe
    mAnimController.SetAnim(aLeft  , 0, a_strafe_left);
    mAnimController.SetAnim(aMiddle, 0, a_idle);
    mAnimController.SetAnim(aRight , 0, a_strafe_right);
    // walk, jog, run
    mAnimController.SetAnim(aMiddle, 1, a_walk);
    mAnimController.SetAnim(aMiddle, 2, a_jog_forward);

    // set second row to idle 
    mAnimController.SetAnim(aLeft  , 1, a_jog_forward);//a_diagonal_left);
    mAnimController.SetAnim(aRight , 1, a_jog_forward);//a_diagonal_right);
                                    
    // set first row to idle        
    mAnimController.SetAnim(aLeft  , 0, a_jog_forward);//a_diagonal_left);
    mAnimController.SetAnim(aRight , 0, a_jog_forward);//a_diagonal_right);
 
    // copy first row to inverse first row
    SmallMemCpy(mAnimController.mLocomotionIndicesInv[0],
                mAnimController.mLocomotionIndices[0], sizeof(int) * 5);

    mAnimController.SetAnim(aLeft  , -1, a_jog_forward); //a_diagonal_left);
    mAnimController.SetAnim(aMiddle, -1, a_jog_backward);
    mAnimController.SetAnim(aRight , -1, a_jog_forward);//a_diagonal_right);
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

void CharacterController::RespondInput()
{
#ifndef PLATFORM_ANDROID
    if (GetMousePressed(MouseButton_Left)) {
        mAnimController.TriggerAnim(mAtackIndex, 0.25f, true);
    }

    if (GetKeyPressed(Key_SPACE)) {
        mAnimController.TriggerAnim(mJumpIndex, 0.2f, false);
    }

    if (GetKeyPressed('F')) {
        mAnimController.TriggerAnim(mKickIndex, 0.25f, false);
    }

    if (GetKeyPressed('C')) {
        mAnimController.TriggerAnim(mImpactIndex, 1.0f, false);
    }
#else
    Vector2f pos = { 1731.0f, 840.0f };
    const float buttonSize = 40.0f;
    const float buttonPadding = 120.0f;
    constexpr uint effect = uFadeBit | uEmptyInsideBit | uFadeInvertBit | uIntenseFadeBit;
    uCircle(pos, buttonSize, ~0, effect);
    if (uClickCheckCircle(pos, buttonSize))
    {
        mAnimController.TriggerAnim(mAtackIndex, 0.25f, true);
    }

    pos.x -= buttonPadding;
    uCircle(pos, buttonSize, ~0, effect);
    if (uClickCheckCircle(pos, buttonSize))
    {
        mAnimController.TriggerAnim(mKickIndex, 0.2f, false);
    }

    pos.x += buttonPadding;
    pos.y -= buttonPadding;
    uCircle(pos, buttonSize, ~0, effect);
    if (uClickCheckCircle(pos, buttonSize))
    {
        mAnimController.TriggerAnim(mImpactIndex, 0.25f, false);
    }
#endif
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

    float* posPtr = mCharacter->nodes[mRootNodeIdx].translation;
    float* rotPtr = mCharacter->nodes[mRootNodeIdx].rotation;
    
    SmallMemCpy(posPtr, &mStartPos.x, sizeof(Vector3f));
    VecStore(rotPtr, mStartRotation);

    const float animSpeed = 1.0f;
    mAnimController.EvaluateLocomotion(mCurrentMovement.x,
                                       mCurrentMovement.y,
                                       animSpeed);

    isRunning |= GetKeyDown(Key_SHIFT);
    targetMovement.y += (float)isRunning;
    
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
        mRotation = QMul(QFromXAngle(mCurrentMovement.y * 0.005f), mRotation);
    }

    // mRotation = QMul(QFromZAngle(mCurrentMovement.x * 0.2f), mRotation);

    x += -inputAngle / 2.0f;
    // handle character position
    {   
        Vector3f forward = MakeVec3(Sin(x), 0.0f, Cos(x));
        Vector3f progress = forward * mMovementSpeed * deltaTime;
        progress *= Clamp(-mCurrentMovement.LengthSafe(), -1.0f, targetMovement.y);
        Vector3f oldPos = mPosition;
        float runAddition = 1.0f + (float(isRunning) * 0.55f);
        mPosition += progress * runAddition * 1.5f;
        
        if (isSponza)
            ColissionDetection(oldPos);
        
        // animatedPos.y = 8.15f; // if you want to walk on top floor
        camera->targetPos = mPosition;
    }

    RespondInput();

    if (!mAnimController.IsTrigerred() && Abs(mCurrentMovement.x) + Abs(mCurrentMovement.y) < 0.05f)
    {
        mIdleTime += deltaTime;
    }
    else {
        mIdleTime = 0.0f;
    }

    if (mAnimController.IsTrigerred() &&
       (mAnimController.mTriggerredAnim == mImpactIndex || 
        mAnimController.mTriggerredAnim == mKickIndex))
    {
        mCurrentMovement = Vector2f::Zero();
    }

    if (mIdleTime >= mIdleLimit)
    {
        mAnimController.TriggerAnim(mIdle2Index, 0.25f, true);
        mIdleTime = 0.0f;
        mIdleLimit = 6.0f + (Random::NextFloat01(Random::PCG2Next(mRandomState)) * 15.0f);
    }

    VecStore(rotPtr, mRotation);
    // set animated pos for the renderer
    SmallMemCpy(posPtr, &mPosition.x, sizeof(Vector3f));
    SceneRenderer::SetCharacterPos(mPosition.x, mPosition.y, mPosition.z);
}

void CharacterController::Destroy()
{
    ClearAnimationController(&mAnimController);
}
