
#pragma once

#include "Animation.hpp"
#include "Scene.hpp"
#include "../../ASTL/Random.hpp"

enum eCharacterControllerState_
{
    eCharacterControllerState_Idle,
    eCharacterControllerState_Movement,
    eCharacterControllerState_Turning,
};

typedef int eCharacterControllerState;

struct CharacterController
{
    Quaternion mStartRotation;
    Vector3f mStartPos;
    int mRootNodeIdx;
    
    Prefab* mCharacter;
    float* mPosPtr;
    float* mRotPtr;
    AnimationController mAnimController;

    Vector2f mAnimTime;
    float    mAnimSpeed;
    
    Vector2f   mCurrentMovement;
    float      mMovementSpeed; // default -> 2.7f
    float      mCurrentSpeed;
    float      mSpeedSmoothVelocity;
    Quaternion mRotation;
    Vector3f   mPosition;

    Vector2f mTouchStart;

    // animation indices
    int mAtackIndex;
    int mComboIndex;
    int mIdle2Index;
    int mJumpIndex;
    int mKickIndex;
    int mImpactIndex;
    int mTurnLeft90Index;
    int mTurnRight90Index;
    int mTurn180Index;

    int mKickHuhSound;
    int mSwordSlashSound;
    float mSwordSlashDelay;

    float mIdleTime;
    float mIdleLimit;

    eCharacterControllerState mState;
    uint mRandomState; // < for random number generation
    float mNonStopDuration;
    float mTurnRotation; // in radians
    float mLastInputAngle;
    float mOldInputAngle;
    bool mWasPressing;
    bool mControlling;

//------------------------------------------------------------------------
    void Start(Prefab* _character);

    void Update(float deltaTime, bool isSponza);
    
    void RespondInput();

    void ColissionDetection(Vector3f oldPos);
    
    void TurningState();

    void MovementState(bool isSponza);

    void IdleState();

    Vector2f GetTargetMovement();

    void HandleNeckAndSpineRotation(float deltaTime);

    void Destroy();
};