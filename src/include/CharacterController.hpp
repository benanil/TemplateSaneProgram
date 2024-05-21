
#pragma once

#include "Animation.hpp"
#include "Scene.hpp"

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

//------------------------------------------------------------------------
    void Start(Prefab* _character);

    void Update(float deltaTime, bool isSponza);

    void ColissionDetection(Vector3f oldPos);
    
    void Destroy();
};