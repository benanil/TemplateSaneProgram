
#include "include/Animation.hpp"
#include "include/Scene.hpp"
#include "include/SceneRenderer.hpp"
#include "include/Platform.hpp"
#include "../ASTL/String.hpp" // StrCmp16


void StartAnimationSystem()
{ }

int FindRootNodeIndex(Prefab* prefab)
{
    if (prefab->skins == nullptr)
        return 0;

    ASkin skin = prefab->skins[0];
    if (skin.skeleton != -1) 
        return skin.skeleton;
    
    // search for Armature name, and also record the node that has most children
    int armatureIdx = -1;
    int maxChilds   = 0;
    int maxChildIdx = 0;
    // maybe recurse to find max children
    for (int i = 0; i < prefab->numNodes; i++)
    {
        if (StrCMP16(prefab->nodes[i].name, "Armature")) {
            armatureIdx = i;
            break;
        }
    
        int numChildren = prefab->nodes[i].numChildren;
        if (numChildren > maxChilds) {
            maxChilds = numChildren;
            maxChildIdx = i;
        }
    }
    
    int skeletonNode = armatureIdx != -1 ? armatureIdx : maxChildIdx;
    return skeletonNode;
}

static int FindNodeIndex(Prefab* prefab, const char* name)
{
    int len = StringLength(name);
    for (int i = 0; i < prefab->numNodes; i++)
    {
        if (StringEqual(prefab->nodes[i].name, name, len))
            return i;
    }
    ASSERT(0);
    return 0;
}

static void SetBoneNode(Prefab* prefab, ANode*& node, int& index, const char* name)
{
    index = FindNodeIndex(prefab, name);
    node = &prefab->nodes[index];
}

static inline void RotateNode(ANode* node, Vector3f axis, float angle)
{
    Quaternion q = QMul(QFromAxisAngle(axis, angle), VecLoad(node->rotation));
    VecStore(node->rotation, q);
}

__forceinline Matrix4 GetNodeMatrix(ANode* node)
{
    return Matrix4::PositionRotationScale(node->translation, node->rotation, node->scale);
}

void CreateAnimationController(Prefab* prefab, AnimationController* result, bool humanoid)
{
    ASkin* skin = &prefab->skins[0];
    ASSERTR(skin != nullptr, AX_LOG("skin is null"); return);
    ASSERTR(skin->numJoints < MaxBonePoses, AX_LOG("number of joints is greater than max capacity"); return);

    result->mMatrixTex = rCreateTexture(skin->numJoints*3, 1, nullptr, TextureType_RGBA16F, TexFlags_RawData);
    result->mRootNodeIndex = FindRootNodeIndex(prefab);
    result->mPrefab = prefab;
    result->mState = AnimState_Update;
    result->mNumNodes = prefab->numNodes;
    result->mTrigerredNorm = 0.0f;
    result->lowerBodyIdxStart = 60;

    ASSERT(result->mRootNodeIndex < MaxBonePoses);
    ASSERT(prefab->GetNodePtr(result->mRootNodeIndex)->numChildren > 0); // root node has to have children nodes
    
    if (!humanoid)
        return;
    
    SetBoneNode(prefab, result->mSpineNode, result->mSpineNodeIdx, "mixamorig:Spine");
    SetBoneNode(prefab, result->mNeckNode , result->mNeckNodeIdx , "mixamorig:Neck");
    
    result->mSpineAxis = Vector3f::Up();
    result->mNeckAxis = Vector3f::Up();
}

void AnimationController::RecurseNodeMatrices(ANode* node, Matrix4 parentMatrix)
{
    for (int c = 0; c < node->numChildren; c++)
    {
        int childIndex = node->children[c];
        ANode* children = &mPrefab->nodes[childIndex];

        if (children == mSpineNode && Abs(mSpineAngle) > Epsilon) { RotateNode(children, mSpineAxis, mSpineAngle); }
        if (children == mNeckNode  && Abs(mNeckAngle)  > Epsilon) { RotateNode(children, mNeckAxis, mNeckAngle); }

        mBoneMatrices[childIndex] = GetNodeMatrix(children) * parentMatrix;

        RecurseNodeMatrices(children, mBoneMatrices[childIndex]);
    }
}

static void MergeAnims(Pose* pose0, Pose* pose1, float animBlend, int numNodes)
{
    for (int i = 0; i < numNodes; i++)
    {
        pose0[i].translation = VecLerp(pose0[i].translation, pose1[i].translation, animBlend);
        pose0[i].scale       = VecLerp(pose0[i].scale, pose1[i].scale, animBlend);
        
        vec_t q = QSlerp(pose0[i].rotation, pose1[i].rotation, animBlend); // QNLerp
        pose0[i].rotation = QNormEst(q);
    }
}

static void InitNodes(ANode* nodes, Pose* pose, int begin, int numNodes)
{
    numNodes += begin;
    for (int i = begin; i < numNodes; i++)
    {
        VecStore(nodes[i].translation, pose[i].translation);
        VecStore(nodes[i].rotation, pose[i].rotation);
        VecStore(nodes[i].scale, pose[i].scale);
    }
}

static void InitPose(Pose* pose, ANode* nodes, int numNodes)
{
    for (int i = 0; i < numNodes; i++)
    {
        pose[i].translation = VecLoad(nodes[i].translation);
        pose[i].rotation    = VecLoad(nodes[i].rotation);
        pose[i].scale       = VecLoad(nodes[i].scale);
    }
}

static void SampleAnimationPose(Pose* pose, AAnimation* animation, float normTime, ANode* nodes, int numNodes)
{
    InitPose(pose, nodes, numNodes);
    float realTime = normTime * animation->duration;
    
    for (int c = 0; c < animation->numChannels; c++)
    {
        AAnimChannel& channel = animation->channels[c];
        int targetNode = channel.targetNode; // prefab->GetNodePtr();
        AAnimSampler& sampler = animation->samplers[channel.sampler];
    
        // morph targets are not supported
        if (channel.targetPath == AAnimTargetPath_Weight)
            continue;
    
        // maybe binary search
        int beginIdx = 0;
        while (realTime >= sampler.input[beginIdx + 1])
            beginIdx++;
        
        beginIdx = MIN(beginIdx, sampler.count - 1);
        vec_t begin = ((vec_t*)sampler.output)[beginIdx];
        vec_t end   = ((vec_t*)sampler.output)[beginIdx + 1];
    
        float t = MAX(0.0f, realTime - sampler.input[beginIdx]) / MAX(sampler.input[beginIdx + 1] - sampler.input[beginIdx], 0.0001f);
        t = Clamp01(t);

        switch (channel.targetPath)
        {
            case AAnimTargetPath_Scale:
                pose[targetNode].scale = VecLerp(begin, end, t);
                break;
            case AAnimTargetPath_Translation:
                pose[targetNode].translation = VecLerp(begin, end, t);
                break;
            case AAnimTargetPath_Rotation:
                Quaternion rot = QSlerp(begin, end, t);
                pose[targetNode].rotation = QNormEst(rot);
                break;
        };
    }
}

// send matrices to GPU
void AnimationController::UploadBoneMatrices()
{
    ASkin& skin = mPrefab->skins[0];
    Matrix4* invMatrices = (Matrix4*)skin.inverseBindMatrices;

    for (int i = 0; i < skin.numJoints; i++)
    {
        Matrix4 mat = invMatrices[i] * mBoneMatrices[skin.joints[i]];
        mat = Matrix4::Transpose(mat);
        ConvertFloatToHalf4(mOutMatrices[i].x, (float*)&mat.r[0]);
        ConvertFloatToHalf4(mOutMatrices[i].y, (float*)&mat.r[1]);
        ConvertFloatToHalf4(mOutMatrices[i].z, (float*)&mat.r[2]);
    }
    
    // upload anim matrix texture to the GPU
    rUpdateTexture(mMatrixTex, mOutMatrices);
}

void AnimationController::UploadAnimationPose(Pose* pose)
{
    InitNodes(mPrefab->nodes, pose, 0, mPrefab->numNodes);

    ANode* rootNode = mPrefab->GetNodePtr(mRootNodeIndex);
    Matrix4 rootMatrix = GetNodeMatrix(rootNode);
    mBoneMatrices[mRootNodeIndex] = rootMatrix;

    RecurseNodeMatrices(rootNode, rootMatrix);
    UploadBoneMatrices();
}

// when we want to play different animations with lower body and upper body
void AnimationController::UploadPoseUpperLower(Pose* lowerPose, Pose* uperPose)
{
    InitNodes(mPrefab->nodes, lowerPose, lowerBodyIdxStart, mPrefab->numNodes - lowerBodyIdxStart);
    InitNodes(mPrefab->nodes, uperPose, 0, lowerBodyIdxStart);

    ANode* rootNode = mPrefab->GetNodePtr(mRootNodeIndex);
    Matrix4 rootMatrix = GetNodeMatrix(rootNode);
    mBoneMatrices[mRootNodeIndex] = rootMatrix;

    RecurseNodeMatrices(rootNode, rootMatrix);
    UploadBoneMatrices();
}

void AnimationController::PlayAnim(int index, float norm)
{
    AAnimation* animation0 = &mPrefab->animations[index];
    SampleAnimationPose(mAnimPoseA, animation0, norm, mPrefab->nodes, mPrefab->numNodes);
    UploadAnimationPose(mAnimPoseA);
}

void AnimationController::TriggerAnim(int index, float triggerTime, bool standing)
{
    if (IsTrigerred()) return; // already trigerred

    mTriggerredAnim = index;
    mTrigerredStanding = standing;
    if (triggerTime < 0.02f) { // no transition requested
        mState = AnimState_TriggerPlaying;
        return;
    }
    mTransitionTime = triggerTime;
    mCurTransitionTime = triggerTime;
    mState = AnimState_TriggerIn;
    
    AAnimation* animEnd = &mPrefab->animations[index];
    SmallMemCpy(mAnimPoseC, mAnimPoseA, sizeof(mAnimPoseA));
    SampleAnimationPose(mAnimPoseD, animEnd, 0.0f, mPrefab->nodes, mNumNodes);
}

// x, y has to be between -1.0 and 1.0
void AnimationController::EvaluateLocomotion(float x, float y, float animSpeed)
{
    const float deltaTime = (float)GetDeltaTime();

    if (mState == AnimState_TriggerIn)
    {
        float newNorm = Clamp01((mTransitionTime - mCurTransitionTime) / mTransitionTime);
        newNorm *= 1.0f - (newNorm * newNorm * newNorm);
        float animDelta = deltaTime * (1.0f / MAX(1.0f-newNorm, 0.001f));
        animDelta = Clamp01(animDelta);
        MergeAnims(mAnimPoseC, mAnimPoseD, newNorm, mNumNodes);
        mCurTransitionTime -= deltaTime * 1.1f;

        if (mCurTransitionTime <= 0.0f) {
            mState = AnimState_TriggerPlaying;
        }
    }
    else if (mState == AnimState_TriggerOut)
    {
        // merge last animation with trigerred animation's beginning
        float newNorm = Clamp01((mTransitionTime - mCurTransitionTime) / mTransitionTime);
        newNorm *= 1.0f - (newNorm * newNorm * newNorm);
        float animDelta = deltaTime * (1.0f / MAX(1.0f-newNorm, 0.001f));
        animDelta = Clamp01(animDelta);
        MergeAnims(mAnimPoseC, mAnimPoseD, animDelta, mNumNodes);
        mCurTransitionTime -= deltaTime * 1.1f;

        if (mCurTransitionTime <= 0.0f)
        {
            mState = AnimState_Update;
        }
    }
    else if (mState == AnimState_TriggerPlaying)
    {
        float animStep = 1.0f / mPrefab->animations[mTriggerredAnim].duration;
        mTrigerredNorm += animSpeed * animStep * deltaTime;
        mTrigerredNorm = Clamp01(mTrigerredNorm);

        AAnimation* anim = &mPrefab->animations[mTriggerredAnim];
        SampleAnimationPose(mAnimPoseC, anim, mTrigerredNorm, mPrefab->nodes, mPrefab->numNodes);

        if (mTrigerredNorm >= 1.0f)
        {
            mTrigerredNorm = 0.0f; // trigger stage complated
            AAnimation* animEnd = &mPrefab->animations[mLastAnim];
            SampleAnimationPose(mAnimPoseD, animEnd, mAnimTime.y, mPrefab->nodes, mNumNodes);
            mState = AnimState_TriggerOut;
            mCurTransitionTime = mTransitionTime;
        }
    }
    
    // if trigerred animation is not standing, we don't have to sample walking or running animations
    if (IsTrigerred() && !mTrigerredStanding)
        goto walk_run_end;

    // play and blend walking and running anims
    y = Abs(y);
    float yBlend = Fract(y);
    
    int yi = int(y);
    // sample y anim
    ASSERTR(yi <= 3, return); // must be between 1 and 4
    int sign = Sign(yi);
    int yIndex = GetAnim(aMiddle, yi);
    AAnimation* animStart = &mPrefab->animations[yIndex];
    
    SampleAnimationPose(mAnimPoseA, animStart, mAnimTime.y, mPrefab->nodes, mNumNodes);
    
    bool shouldAnimBlendY = yi != 3 && yBlend > 0.00002f;
    if (shouldAnimBlendY)
    {
        yIndex = GetAnim(aMiddle, yi + sign);
        AAnimation* animEnd = &mPrefab->animations[yIndex];
        SampleAnimationPose(mAnimPoseB, animEnd, mAnimTime.y, mPrefab->nodes, mNumNodes);
        MergeAnims(mAnimPoseA, mAnimPoseB, EaseOut(yBlend), mNumNodes);
    }
    
    // if anim is two seconds animStep is 0.5 because we are using normalized value
    float yAnimStep = 1.0f / mPrefab->animations[yIndex].duration;
    mAnimTime.y += animSpeed * yAnimStep * deltaTime;
    mAnimTime.y  = Fract(mAnimTime.y);
    
    mLastAnim = yIndex;

    if (!IsTrigerred()) {
        UploadAnimationPose(mAnimPoseA);
    }
    else 
    {
        walk_run_end:{}
        if (mTrigerredStanding)
        {
            UploadPoseUpperLower(mAnimPoseA, mAnimPoseC);
        }
        else
        {
            UploadAnimationPose(mAnimPoseC);
        }
    }
}

void ClearAnimationController(AnimationController* animSystem)
{
    rDeleteTexture(animSystem->mMatrixTex);
}

void DestroyAnimationSystem()
{ }

