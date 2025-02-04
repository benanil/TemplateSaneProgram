
#include "include/CharacterController.hpp"
#include "include/Platform.hpp"
#include "include/SceneRenderer.hpp"
#include "include/Camera.hpp"
#include "include/UI.hpp"
#include <stdio.h>

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
    mTouchStart = Vec2(0.0f, 0.0f);
    // we don't need to set zero the poses
    constexpr size_t poseSize = sizeof(AnimationController::mAnimPoseA) * 4 
                              + sizeof(AnimationController::mBoneMatrices) + sizeof(AnimationController::mOutMatrices);
    MemsetZero(&mAnimController, sizeof(AnimationController) - poseSize);
    CreateAnimationController(_character, &mAnimController, true, 58);
    mRandomState = Random::Seed32();
    mState = eCharacterControllerState_Idle;
    mControlling = true;
    
    mRotation = QIdentity();
    mPosition.x = -39.0f;
    mPosition.y =  0.65f; // make foots touch the ground
    mPosition.z = -16.0f;
    mMovementSpeed  = 2.7f;
    mIdleLimit      = 8.0f;
    mIdleTime       = 0.0f;
    mLastInputAngle = 0.0f;
    mSwordSlashDelay = -1.0f;

    mRootNodeIdx = Prefab::FindAnimRootNodeIndex(_character);
    mPosPtr = _character->nodes[mRootNodeIdx].translation;
    mRotPtr = _character->nodes[mRootNodeIdx].rotation;
    
    mStartPos = Vec3(mPosPtr);
    mStartRotation = VecLoad(mRotPtr);
    mStartPos.y = 0.65f;

    mKickHuhSound    = LoadSound("Assets/Audio/KickHuh.mp3");
    mSwordSlashSound = LoadSound("Assets/Audio/SwordSlash.wav");

    int a_idle           = FindAnimIndex(_character, "Idle");
    int a_walk           = FindAnimIndex(_character, "Walk");
    int a_jog_forward    = FindAnimIndex(_character, "Run");
    int a_jog_backward   = a_jog_forward; // FindAnimIndex(_character, "Jog_Backward");
    // int a_diagonal_left  = FindAnimIndex(_character, "Jog_Forward_Left");
    // int a_diagonal_right = FindAnimIndex(_character, "Jog_Forward_Right");
    int a_strafe_left    = FindAnimIndex(_character, "StrafeLeft");
    int a_strafe_right   = FindAnimIndex(_character, "StrafeRight");

    mAtackIndex  = FindAnimIndex(_character, "Slash2");
    mComboIndex  = FindAnimIndex(_character, "SwordCombo");
    mIdle2Index  = FindAnimIndex(_character, "Idle2");
    mJumpIndex   = FindAnimIndex(_character, "Jump");
    mImpactIndex = FindAnimIndex(_character, "Impact");
    mKickIndex   = FindAnimIndex(_character, "Kick");

    mTurnLeft90Index  = FindAnimIndex(_character, "TurnLeft90");
    mTurnRight90Index = FindAnimIndex(_character, "TurnRight90");
    mTurn180Index     = FindAnimIndex(_character, "Turn180");

    // idle, left&right strafe
    mAnimController.SetAnim(aLeft  , 0, a_strafe_left);
    mAnimController.SetAnim(aMiddle, 0, a_idle);
    mAnimController.SetAnim(aRight , 0, a_strafe_right);
    // walk, jog, run
    mAnimController.SetAnim(aMiddle, 1, a_walk);
    mAnimController.SetAnim(aMiddle, 2, a_jog_forward);

    // set second row to idle 
    mAnimController.SetAnim(aLeft  , 1, a_strafe_left);//a_diagonal_left);
    mAnimController.SetAnim(aRight , 1, a_strafe_right);//a_diagonal_right);
                                    
    // set first row to idle        
    mAnimController.SetAnim(aLeft  , 0, a_jog_forward);//a_diagonal_left);
    mAnimController.SetAnim(aRight , 0, a_jog_forward);//a_diagonal_right);
 
    // copy first row to inverse first row
    SmallMemCpy(mAnimController.mLocomotionIndicesInv[0],
                mAnimController.mLocomotionIndices[0], sizeof(int) * 3);

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
        if (mAnimController.TriggerAnim(mAtackIndex, 0.0f, 0.0f, eAnimTriggerOpt_Standing))
            mSwordSlashDelay = 0.0f; // set zero to start delay
    }

    if (GetKeyPressed(Key_SPACE)) {
        if (mAnimController.TriggerAnim(mJumpIndex, 0.0f, 0.0f, 0))
            SoundPlay(mKickHuhSound);
    }

    if (GetKeyPressed('F')) {
        if (mAnimController.TriggerAnim(mKickIndex, 0.0f, 0.0f, 0)) {
            mCurrentMovement = Vector2f::Zero();
            SoundPlay(mKickHuhSound);
        }
    }

    if (GetKeyPressed('C')) {
        if (mAnimController.TriggerAnim(mImpactIndex, 0.5f, 0.35f, 0)) {
            mCurrentMovement = Vector2f::Zero();
            SoundPlay(mKickHuhSound);
        }
    }

    if (GetKeyPressed('X')) {
        if (mAnimController.TriggerAnim(mComboIndex, 0.5f, 4.0f, eAnimTriggerOpt_ReverseOut))
            mCurrentMovement = Vector2f::Zero();
    }
#else
    Vector2f pos = { 1731.0f, 840.0f };
    const float buttonSize = 40.0f;
    const float buttonPadding = 120.0f;
    constexpr uint effect = uFadeBit | uEmptyInsideBit | uFadeInvertBit | uIntenseFadeBit;
    uCircle(pos, buttonSize, ~0, effect);
    if (uClickCheckCircle(pos, buttonSize))
    {
        if (mAnimController.TriggerAnim(mAtackIndex, 0.0f, 0.0f, eAnimTriggerOpt_Standing))
            SoundPlay(mSwordSlashSound);
    }

    pos.x -= buttonPadding;
    uCircle(pos, buttonSize, ~0, effect);
    if (uClickCheckCircle(pos, buttonSize))
    {
        if (mAnimController.TriggerAnim(mKickIndex, 0.0f, 0.0f, 0))
            SoundPlay(mKickHuhSound);
    }

    pos.x += buttonPadding;
    pos.y -= buttonPadding;
    uCircle(pos, buttonSize, ~0, effect);
    if (uClickCheckCircle(pos, buttonSize))
    {
        if (mAnimController.TriggerAnim(mImpactIndex, 0.5f, 0.35f, 0))
            SoundPlay(mKickHuhSound);
    }
#endif
}

void CharacterController::TurningState()
{
    mAnimController.EvaluateLocomotion(0.0f, 0.0f, 1.0f);
    
    if (!mAnimController.IsTrigerred())
    {
        // turning animation ended
        CameraBase* camera = SceneRenderer::GetCamera();
        // angle we have turned + camera look angle
        float x = camera->yaw * -TwoPI;
        mRotation = QFromYAngle(x - mTurnRotation + PI);

        mCurrentMovement.y = Sin(mTurnRotation + HalfPI);
        mCurrentMovement.x = Sin(mTurnRotation);
        mState = eCharacterControllerState_Movement;
    }
}

Vector2f CharacterController::GetTargetMovement()
{
    Vector2f targetMovement = {0.0f, 0.0f};
    #ifndef __ANDROID__ /* NOT android */
    if (GetKeyDown('W')) targetMovement.y = -1.0f; 
    if (GetKeyDown('S')) targetMovement.y = +1.0f; 
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
    
    targetMovement = Min(targetMovement, Vector2f( 1.0f,  1.0f));
    targetMovement = Max(targetMovement, Vector2f(-1.0f, -1.0f));
    targetMovement.x = -targetMovement.x;
    #endif
    return targetMovement;
}

extern float GetTerrainHeight(Vector3f position);

void CharacterController::MovementState(bool isSponza)
{
    const float animSpeed = 1.0f;
    float deltaTime = (float)GetDeltaTime();
    Vector2f targetMovement = GetTargetMovement();
    float animY = mCurrentMovement.y;
    float animX = mCurrentMovement.x;
    if (Abs(animX) > Abs(animY)) animY = animX;

    bool isRunning = GetKeyDown(Key_SHIFT);
    mAnimController.EvaluateLocomotion(animX, animY, isRunning ? animSpeed * 1.5f : animSpeed);
    targetMovement *= float(isRunning) + 1.0f;
    
    const float smoothTime = 0.25f;
    const float acceleration = 3.0f;
    mCurrentMovement.y = SmoothDamp(mCurrentMovement.y, targetMovement.y, mSpeedSmoothVelocity, smoothTime, 9999.0f, deltaTime); // Lerp(mCurrentMovement.y, targetMovement.y, acceleration * deltaTime);
    mCurrentMovement.x = Lerp(mCurrentMovement.x, targetMovement.x, 4.0f * deltaTime);

    CameraBase* camera = SceneRenderer::GetCamera();
    // angle of user input, (keyboard or joystick)
    float x = camera->yaw * -TwoPI;
    float inputAngle = ATan2(targetMovement.y, Clamp(targetMovement.x, -1.0f, 1.0f));
    float movementValue = targetMovement.LengthSquared();
    if (movementValue < 0.001f) inputAngle = 0.0f;
    x += -inputAngle;

    // handle character rotation
    if (movementValue > 0.1f)
    {
        const float rotationSpeed = 12.0f;
        mLastInputAngle = inputAngle;
        mOldInputAngle = x;
        mRotation = QSlerp(mRotation, QFromYAngle(x + PI), deltaTime * rotationSpeed);
    }

    if (mAnimController.IsTrigerred() && mAnimController.mTriggerredAnim == mKickIndex)
        mCurrentMovement = { 0.0f, 0.0f  };

    // handle character position
    Vector3f forward = { Sin(x), 0.0f, Cos(x) };
    Vector3f progress = forward * mMovementSpeed * deltaTime * (float(isRunning) * 4.0f + 1.0f);
    
    float movementAmount = mCurrentMovement.LengthSafe();
    progress *= Clamp(-movementAmount, -1.0f, 1.0f);

    Vector3f oldPos = mPosition;
    mPosition += progress * 1.5f;
    
    if (isSponza) ColissionDetection(oldPos);
    
    mPosition.y = 0.0f; // GetTerrainHeight(mPosition);
    // animatedPos.y = 8.15f; // if you want to walk on top floor
    camera->targetPos = mPosition;
    mNonStopDuration += float(Abs(movementAmount) < 0.002f) * deltaTime;
    mNonStopDuration *= Abs(movementAmount) < 0.002f; 
    if (mNonStopDuration > 0.15f) {
        mNonStopDuration = 0.0f;
        mState = eCharacterControllerState_Idle;
    }
}

void CharacterController::IdleState()
{
    Vector2f targetMovement = GetTargetMovement();
    float deltaTime = (float)GetDeltaTime();
    float movementAmount = targetMovement.LengthSquared();

    mNonStopDuration += float(movementAmount > 0.01f) * deltaTime;
    mIdleTime += !mAnimController.IsTrigerred() * deltaTime;
    mNonStopDuration = movementAmount > 0.1f; // reset duration if player stopped
    mCurrentMovement.y = MAX(mCurrentMovement.y - (deltaTime * 0.5f), 0.0f);
    mAnimController.EvaluateLocomotion(0.0f, mCurrentMovement.y, 1.0f);
    // naughty dog was also waiting quarter of second before running turning animation
    // https://www.gdcvault.com/play/1012300/Animation-and-Player-Control-in

    if (mNonStopDuration > 0.25f)
    {
        // no longer stopping
        mNonStopDuration = 0.0f;
        mTurnRotation = 0.0f;
        bool hasVerticalMovement   = Abs(targetMovement.y) > 0.1f;
        bool hasHorizontalMovement = Abs(targetMovement.x) > 0.1f;
        bool horizontalFull        = Abs(targetMovement.x) > 0.9f;
        bool goingBack = targetMovement.y < -0.9f;
        
        if (horizontalFull && !hasVerticalMovement)
        {
            int anim = targetMovement.x > 0.0f ? mTurnRight90Index : mTurnLeft90Index;
            mTurnRotation = HalfPI * Sign(targetMovement.x);
            mAnimController.TriggerAnim(anim, 0.0f, 0.0f, 0);
        }
        
        if (!hasHorizontalMovement && goingBack)
        {
            mTurnRotation = PI;
            mAnimController.TriggerAnim(mTurn180Index, 0.0f, 0.0f, 0);
        }
        
        if (mTurnRotation != 0.0f) {
            mState = eCharacterControllerState_Turning;
        }
        else 
        {
            mState = eCharacterControllerState_Movement;
        }
    }

    if (mIdleTime >= mIdleLimit)
    {
        mAnimController.TriggerAnim(mIdle2Index, 0.25f, 0.25f, eAnimTriggerOpt_Standing);
        mIdleTime = 0.0f;
        mIdleLimit = 6.0f + (Random::NextFloat01(Random::PCG2Next(mRandomState)) * 15.0f);
    }
}

// Rotates the character's head and body to the looking direction
void CharacterController::HandleNeckAndSpineRotation(float deltaTime)
{
    if (!mAnimController.IsTrigerred()) 
    {
        CameraBase* camera = SceneRenderer::GetCamera();
        
        float neckYTarget = -mLastInputAngle + camera->yaw * -TwoPI;
        const float yMinAngle = -PI / 3.0f, yMaxAngle = PI / 3.0f;
        // take differance of character forward direction and camera direction
        neckYTarget = Clamp(neckYTarget - mOldInputAngle, yMinAngle, yMaxAngle);
        
        const float rotateSpeed = 2.0f;
        mAnimController.mNeckYAngle  = Lerp(mAnimController.mNeckYAngle, neckYTarget, deltaTime * rotateSpeed);
        mAnimController.mSpineYAngle = Lerp(mAnimController.mSpineYAngle, neckYTarget * 0.5f, deltaTime * rotateSpeed);
        
        const float neckMaxXAngle = 1.25f, spineMaxAngle = 1.85f;
        mAnimController.mNeckXAngle  = Lerp(mAnimController.mNeckXAngle, camera->pitch * neckMaxXAngle, deltaTime * rotateSpeed);
        mAnimController.mSpineXAngle = Lerp(mAnimController.mSpineXAngle, camera->pitch * spineMaxAngle, deltaTime * rotateSpeed);
    }
    else 
    {
        // don't want to rotate the neck while animation playing,
        // triggerred animations look bad when we rotate neck but rotating spine is fine
        mAnimController.mNeckYAngle  *= Abs(mAnimController.mNeckYAngle) > 0.08f;
        mAnimController.mNeckXAngle  *= Abs(mAnimController.mNeckXAngle) > 0.08f;
        mAnimController.mNeckYAngle  = Lerp(mAnimController.mNeckYAngle , 0.0f, deltaTime * 4.0f);
        mAnimController.mNeckXAngle  = Lerp(mAnimController.mNeckXAngle , 0.0f, deltaTime * 4.0f);
    }
}

void CharacterController::Update(float deltaTime, bool isSponza)
{
    if (GetKeyPressed('L'))
    {
        mControlling = !mControlling;
    }

    if (!mControlling) return;

    SmallMemCpy(mPosPtr, &mStartPos.x, sizeof(Vector3f));
    VecStore(mRotPtr, mStartRotation);

    HandleNeckAndSpineRotation(deltaTime);

    if (mSwordSlashDelay >= 0.0f && (mSwordSlashDelay += deltaTime) > 0.45f)
        SoundPlay(mSwordSlashSound), mSwordSlashDelay = -1.0f;

    switch (mState)
    {
        case eCharacterControllerState_Idle:     IdleState(); break;
        case eCharacterControllerState_Movement: MovementState(isSponza); break;
        case eCharacterControllerState_Turning:  TurningState(); break;
    };

    RespondInput();

    VecStore(mRotPtr, mRotation);
    // set animated pos for the renderer
    SmallMemCpy(mPosPtr, &mPosition.x, sizeof(Vector3f));
    mCharacter->UpdateGlobalNodeTransforms(mRootNodeIdx, Matrix4::Identity());

    SceneRenderer::SetCharacterPos(mPosition.x, mPosition.y, mPosition.z);

    // char test[512] = {};
    // sprintf_s(test, 512, "x: %f, y: %f, z: %f", mPosition.x, mPosition.y, mPosition.z);
    // uText(test, Vector2f(500.0f));
}

void CharacterController::Destroy()
{
    ClearAnimationController(&mAnimController);
    SoundDestroy(mKickHuhSound);
    SoundDestroy(mSwordSlashSound);
}
