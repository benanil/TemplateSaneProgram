
#pragma once

#include "Renderer.hpp"
#include "../../ASTL/Math/Matrix.hpp"

struct Prefab;

struct Pose
{
    vec_t translation;
    vec_t rotation;
    // vec_t scale;
};

struct Matrix3x4f16
{
    half x[4];
    half y[4];
    half z[4];
};

enum eAnimLocation_
{
    aLeft, aMiddle, aRight
};

enum eAnimTriggerOpt_
{
    eAnimTriggerOpt_None = 0,
    // allows sword slash while walking,
    // Enable if you want to play different animations on lower and upper body
    eAnimTriggerOpt_Standing   = 1,
    // reverse the animation when transitionning out instead of lerping to previous animation
    eAnimTriggerOpt_ReverseOut = 2
};

enum eAnimControllerState_
{
    AnimState_None           = 0,
    AnimState_Update         = 1,
    AnimState_TriggerIn      = 2,
    AnimState_TriggerOut     = 4,
    AnimState_TriggerPlaying = 8,
    AnimState_TriggerMask = AnimState_TriggerIn | AnimState_TriggerOut | AnimState_TriggerPlaying
};
typedef int eAnimTriggerOpt;
typedef int eAnimLocation;
typedef int eAnimState;
typedef int eAnimControllerState;

constexpr int MaxBonePoses = 128; // make 192 or 256 if we use more joints

struct AnimationController
{
    Texture mMatrixTex;
    Prefab* mPrefab;
    eAnimState mState;

    int mRootNodeIndex;
    int mNumNodes;

    Vector2f mAnimTime;

    int mTriggerredAnim;
    float mTrigerredNorm;
    float mTransitionTime; // trigger transition time
    float mTransitionOutTime;
    float mCurTransitionTime;
    int mLastAnim;
    eAnimTriggerOpt mTriggerOpt;

    ANode* mSpineNode; // < upper body root bone 
    ANode* mNeckNode;

    int mSpineNodeIdx;
    int mNeckNodeIdx;

    // lower body bones are starting from 60th with Brute character and 58 with mixamo Paladin Character
    // used for animating diferrent animations for legs and uper body
    // this value can change from character to character. 
    // you can detect using 3DVert and GBuffer shader just uncomment lines with vBoneIdx, and it will visualize lower body as white
    // Maybe Add: automatic detect.
    int lowerBodyIdxStart; 

    // angle's recomended values are between (-PI/3, PI/3)
    // calculate the angle between target and player, then clamp the value between the limits
    // to enable spine or neck additive rotation you just have to set angle's any value which is not zero
    float mSpineYAngle;
    float mNeckYAngle;
    float mSpineXAngle; // < will rotate around this axis (normalized) default vec3::up
    float mNeckXAngle;  // < will rotate around this axis (normalized) default vec3::up

    // two posses for blending
    Pose mAnimPoseA[MaxBonePoses]; // < the result bone array that we send to GPU
    Pose mAnimPoseB[MaxBonePoses]; // < blend target
    
    Pose mAnimPoseC[MaxBonePoses]; // < Trigerred animations result
    Pose mAnimPoseD[MaxBonePoses]; // < Trigerred Animations blend target

    Matrix4 mBoneMatrices[MaxBonePoses];
    Matrix3x4f16 mOutMatrices[MaxBonePoses];

    // animation indexes to blend coordinates
    // Given xy blend coordinates, we will blend animations.
    // in typical animation system, the diagram should be like the diagran below.
    //  #  #  #  <- DiagonalRun , ForwardRun , DiagonalRun
    //  #  #  #  <- DiagonalJog , ForwardJog , DiagonalJog
    //  #  #  #  <- DiagonalWalk, ForwardWalk, DiagonalWalk
    //  #  #  #  <- StrafeLeft  , Idle       , StrafeRight 
    int mLocomotionIndices   [4][3];
    int mLocomotionIndicesInv[3][3];

    void SetAnim(int x, int y, int index)
    {
        if (y >= 0) mLocomotionIndices[y][x] = index;
        else        mLocomotionIndicesInv[Abs(y-1)][x] = index;
    }

    int GetAnim(int x, int y)
    {
        if (y >= 0) return mLocomotionIndices[y][x];
        else        return mLocomotionIndicesInv[Abs(y)-1][x];
    }
    
    bool IsTrigerred()
    {
        return (mState & AnimState_TriggerMask) != 0;
    }

    // x, y has to be between -1.0 and 1.0 (normalized)
    // xspeed and yspeed is between 0 and infinity speed of animation
    // normTime should be between 0 and 1
    // runs the walking running etc animations from given inputs
    void EvaluateLocomotion(float x, float y, float animSpeed);

    bool TriggerTransition(float dt, int targetAnim);

    // play the given animation, norm is the animation progress between 0.0 and 1.0
    void PlayAnim(int index, float norm);

    // trigger time is the animation transition time
    // standing anims are animations that we can play when walking or running
    void TriggerAnim(int animIndex, float triggerInTime, float triggerOutTime, eAnimTriggerOpt triggerOpt);

    // after this line all of the functions are private but feel free to use
    // upload to gpu. internal usage only for now
    void UploadPose(Pose* nodeMatrices);
    
    void RecurseBoneMatrices(ANode* node, Matrix4 parentMatrix);

    void UploadBoneMatrices();
    
    // when we want to play different animations with lower body and upper body
    void UploadPoseUpperLower(Pose* lowerPose, Pose* uperPose);

    // use negative normTime to sample animation reversely
    void SampleAnimationPose(Pose* pose, int animIdx, float normTime);
};

// no constructors and deconstructors hehe
void CreateAnimationController(Prefab* prefab, AnimationController* animController, bool humanoid = true, int lowerBodyStart = 58);

void ClearAnimationController(AnimationController* animController);

// does nothing for now
[[maybe_unused]] void StartAnimationSystem();

[[maybe_unused]] void DestroyAnimationSystem();
