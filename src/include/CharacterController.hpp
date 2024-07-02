
#pragma once

#include "Animation.hpp"
#include "Scene.hpp"
#include "../../ASTL/Random.hpp"

struct CharacterController
{
    Quaternion mStartRotation;
    Vector3f mStartPos;
    int mRootNodeIdx;
    
    Prefab* mCharacter;
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
    bool mWasPressing;

    // animation indices
    int mAtackIndex;
    int mComboIndex;
    int mIdle2Index;
    int mJumpIndex;
    int mKickIndex;
    int mImpactIndex;

    float mIdleTime;
    float mIdleLimit;

    uint mRandomState;

//------------------------------------------------------------------------
    void Start(Prefab* _character);

    void Update(float deltaTime, bool isSponza);
    
    void RespondInput();

    void ColissionDetection(Vector3f oldPos);
    
    void Destroy();
};