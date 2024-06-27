
#include "include/Animation.hpp"
#include "include/Scene.hpp"
#include "include/SceneRenderer.hpp"
#include "include/Platform.hpp"
#include "../ASTL/String.hpp" // StrCmp16

struct Matrix3x4f16
{
    half x[4];
    half y[4];
    half z[4];
};

const int MaxNumJoints = 256;

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

void CreateAnimationController(Prefab* prefab, AnimationController* result)
{
    ASkin* skin = &prefab->skins[0];
    ASSERTR(skin != nullptr, return);
    result->mMatrixTex = rCreateTexture(skin->numJoints*3, 1, nullptr, TextureType_RGBA16F, TexFlags_RawData);
    result->mRootNodeIndex = FindRootNodeIndex(prefab);
    result->mPrefab = prefab;
    result->mState = AnimState_Update;
    result->mNumNodes = prefab->numNodes;

    ASSERT(skin->numJoints < MaxNumJoints);
    ASSERT(result->mRootNodeIndex < MaxNumJoints);
    ASSERT(prefab->GetNodePtr(result->mRootNodeIndex)->numChildren > 0); // root node has to have children nodes
}

static void RecurseNodeMatrices(ANode* node, ANode* nodes, Matrix4 parentMatrix, Matrix4* matrices)
{
    for (int c = 0; c < node->numChildren; c++)
    {
        int childIndex = node->children[c];
        ANode* children = &nodes[childIndex];
        Matrix4 childTranslation = Matrix4::PositionRotationScale(children->translation, children->rotation, children->scale) * parentMatrix;
        matrices[childIndex] = childTranslation;

        RecurseNodeMatrices(children, nodes, childTranslation, matrices);
    }
}

static void MergeAnims(Pose* pose0, Pose* pose1, float animBlend, int numNodes)
{
    for (int i = 0; i < numNodes; i++)
    {
        pose0[i].translation = VecLerp(pose0[i].translation, pose1[i].translation, animBlend);
        pose0[i].scale       = VecLerp(pose0[i].scale, pose1[i].scale, animBlend);
        
        vec_t q = QSlerp(pose0[i].rotation, pose1[i].rotation, animBlend);
        q = QNorm(q);
        pose0[i].rotation = q;
    }
}

static void InitNodes(ANode* nodes, Pose* pose, int numNodes)
{
    for (int i = 0; i < numNodes; i++)
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
                rot = QNorm(rot);
                pose[targetNode].rotation = rot;
                break;
        };
    }
}

// send matrices to GPU
void AnimationController::UploadAnimationPose(Pose* pose)
{
    InitNodes(mPrefab->nodes, pose, mPrefab->numNodes);

    Matrix4 nodeMatrices[MaxNumJoints];
    ANode* rootNode = mPrefab->GetNodePtr(mRootNodeIndex);
    Matrix4 rootMatrix = Matrix4::PositionRotationScale(rootNode->translation, rootNode->rotation, rootNode->scale);
    nodeMatrices[mRootNodeIndex] = rootMatrix;

    RecurseNodeMatrices(rootNode, mPrefab->nodes, rootMatrix, nodeMatrices);

    ASkin& skin = mPrefab->skins[0];
    Matrix4* invMatrices = (Matrix4*)skin.inverseBindMatrices;
    Matrix3x4f16 outMatrices[MaxNumJoints];

    for (int i = 0; i < skin.numJoints; i++)
    {
        Matrix4 mat = invMatrices[i] * nodeMatrices[skin.joints[i]];
        mat = Matrix4::Transpose(mat);
        ConvertFloatToHalf4(outMatrices[i].x, (float*)&mat.r[0]);
        ConvertFloatToHalf4(outMatrices[i].y, (float*)&mat.r[1]);
        ConvertFloatToHalf4(outMatrices[i].z, (float*)&mat.r[2]);
    }
    
    // upload anim matrix texture to the GPU
    rUpdateTexture(mMatrixTex, outMatrices);
}

void AnimationController::PlayAnim(int index, float norm)
{
    Pose animPose[MaxBonePoses];
    AAnimation* animation0 = &mPrefab->animations[index];
    SampleAnimationPose(animPose, animation0, norm, mPrefab->nodes, mPrefab->numNodes);
    UploadAnimationPose(animPose);
}

void AnimationController::TriggerAnim(int index, float triggerTime)
{
    if (IsTrigerred()) return; // already trigerred

    AAnimation* animEnd = &mPrefab->animations[index];
    mTriggerredAnim = index;
    mTransitionTime = triggerTime;
    mCurTransitionTime = triggerTime;
    mState = AnimState_TriggerIn;
    
    SampleAnimationPose(mAnimPoseB, animEnd, 0.0f, mPrefab->nodes, mNumNodes);
}

// x, y has to be between -1.0 and 1.0
void AnimationController::EvaluateLocomotion(float x, float y, float animSpeed)
{
    const float deltaTime = (float)GetDeltaTime();

    if (mState == AnimState_TriggerIn)
    {
        float newNorm = (mTransitionTime - mCurTransitionTime) / mTransitionTime;
        float animDelta = deltaTime * (1.0f / MAX(1.0f-newNorm, 0.001f));
        animDelta = Clamp01(animDelta);
        MergeAnims(mAnimPoseA, mAnimPoseB, animDelta, mNumNodes);
        mCurTransitionTime -= deltaTime;

        if (mCurTransitionTime <= 0.0f) {
            mState = AnimState_TriggerPlaying;
            mTrigerredNorm = 0.0f; // trigger stage complated
        }
    }
    else if (mState == AnimState_TriggerOut)
    {
        // merge last animation with trigerred animation's beginning
        float newNorm = (mTransitionTime - mCurTransitionTime) / mTransitionTime;
        float animDelta = deltaTime * (1.0f / MAX(1.0f-newNorm, 0.001f));
        animDelta = Clamp01(animDelta);
        MergeAnims(mAnimPoseA, mAnimPoseB, animDelta, mNumNodes);
        mCurTransitionTime -= deltaTime;

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
        SampleAnimationPose(mAnimPoseA, anim, mTrigerredNorm, mPrefab->nodes, mPrefab->numNodes);

        if (mTrigerredNorm >= 1.0f)
        {
            AAnimation* animEnd = &mPrefab->animations[mLastAnim];
            SampleAnimationPose(mAnimPoseB, animEnd, 0.0f, mPrefab->nodes, mNumNodes);
            mState = AnimState_TriggerOut;
            mCurTransitionTime = mTransitionTime;
        }
    }
    else if (mState == AnimState_Update)
    {
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
    }

    // if (mState != AnimState_None)
    UploadAnimationPose(mAnimPoseA);
}

void ClearAnimationController(AnimationController* animSystem)
{
    rDeleteTexture(animSystem->mMatrixTex);
}

void DestroyAnimationSystem()
{ }

