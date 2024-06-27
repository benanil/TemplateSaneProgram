
#pragma once

#include "Renderer.hpp"
#include "../../ASTL/Math/Matrix.hpp"

struct Prefab;

struct Pose
{
    vec_t translation;
    vec_t rotation;
    vec_t scale;
};

enum eAnimLocation_
{
    aLeft, aMiddle, aRight
};

enum eAnimControllerState_
{
    AnimState_Update         = 1,
    AnimState_TriggerIn      = 2,
    AnimState_TriggerOut     = 4,
    AnimState_TriggerPlaying = 8,
    AnimState_None           = 16,// < doesn't do any calculations
    AnimState_TriggerMask = AnimState_TriggerIn | AnimState_TriggerOut | AnimState_TriggerPlaying
};
typedef int eAnimLocation;
typedef int eAnimState;

constexpr int MaxBonePoses = 128; // make 192 or 256 if we use more joints

struct AnimationController
{
    Texture mMatrixTex;
    int mRootNodeIndex;
    Prefab* mPrefab;
    int mNumNodes;
    eAnimState mState;

    Vector2f mAnimTime;

    int mTriggerredAnim;
    float mTrigerredNorm;
    float mTransitionTime; // trigger time
    float mCurTransitionTime; // trigger time
    int mLastAnim;
    
    // animation indexes to blend coordinates
    // Given xy blend coordinates, we will blend animations.
    // in typical animation system, the diagram should be like the diagran below.
    //  #  #  #  <- DiagonalRun , ForwardRun , DiagonalRun
    //  #  #  #  <- DiagonalJog , ForwardJog , DiagonalJog
    //  #  #  #  <- DiagonalWalk, ForwardWalk, DiagonalWalk
    //  #  #  #  <- StrafeLeft  , Idle       , StrafeRight 
    int mLocomotionIndices   [4][3];
    int mLocomotionIndicesInv[3][3];

    // two posses for blending
    Pose mAnimPoseA[MaxBonePoses]; // < the result bone array that we send to GPU
    Pose mAnimPoseB[MaxBonePoses]; // < used for blending between animations

    void SetAnim(int x, int y, int index)
    {
        if (y >= 0) mLocomotionIndices[y][x] = index;
        else        mLocomotionIndicesInv[Abs(y)][x] = index;
    }

    int GetAnim(int x, int y)
    {
        if (y >= 0) return mLocomotionIndices[y][x];
        else        return mLocomotionIndicesInv[Abs(y)-1][x];
    }
    
    bool IsTrigerred()
    {
        return (mState & AnimState_TriggerMask) > 0;
    }

    // x, y has to be between -1.0 and 1.0 (normalized)
    // xspeed and yspeed is between 0 and infinity speed of animation
    // normTime should be between 0 and 1
    // runs the walking running etc animations from given inputs
    void EvaluateLocomotion(float x,
                            float y,
                            float animSpeed);

    // play the given animation, norm is the animation progress between 0.0 and 1.0
    void PlayAnim(int index, float norm);

    void TriggerAnim(int index, float triggerTime);

    // upload to gpu. internal usage only for now
    void UploadAnimationPose(Pose* nodeMatrices);
    
    void MixedAnim(Prefab* prefab, int IdxAnimA, int IdxAnimB, float normA, float normB, float blend);
};

// no constructors and deconstructors hehe
void CreateAnimationController(Prefab* prefab, AnimationController* animController);

void ClearAnimationController(AnimationController* animController);

int FindRootNodeIndex(Prefab* prefab);

// does nothing for now
[[maybe_unused]] void StartAnimationSystem();

[[maybe_unused]] void DestroyAnimationSystem();
