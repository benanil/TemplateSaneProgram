
/******************************************************************************************
*  Purpose:                                                                               *
*    Play Blend and Mix Animations Trigger and Transition Between animations              *
*    Can Play Seperate animations on Upper Body and Lower Body: Sword Slash while Walking *
*    Can Rotate The Head and Spine of humanoid character independently,                   *
*    Allows to look around and turn the body                                              *
*  Good To Know:                                                                          *
*    Stores bone matrices into Matrix3x4Half format and sends to GPU within Textures      *
*    Scale interpolation is disabled for now                                              *
*  Author:                                                                                *
*    Anilcan Gulkaya 2024 anilcangulkaya7@gmail.com github @benanil                       *
*******************************************************************************************/

#include "include/Animation.hpp"
#include "include/Scene.hpp"
#include "include/SceneRenderer.hpp"
#include "include/Platform.hpp"


void StartAnimationSystem()
{ }

static void SetBoneNode(Prefab* prefab, ANode*& node, int& index, const char* name)
{
    index = Prefab::FindNodeFromName(prefab, name);
    node = &prefab->nodes[index];
}

void CreateAnimationController(Prefab* prefab, AnimationController* result, bool humanoid, int lowerBodyStart)
{
    ASkin* skin = &prefab->skins[0];
    if (skin == nullptr) {
        AX_WARN("skin is null %s", prefab->path); return;
    }
    if (skin->numJoints > MaxBonePoses) {
        AX_WARN("number of joints is greater than max capacity %s", prefab->path); 
        return; 
    }
    result->mMatrixTex = rCreateTexture(skin->numJoints*3, 1, nullptr, TextureType_RGBA16F, TexFlags_RawData);
    result->mRootNodeIndex = Prefab::FindAnimRootNodeIndex(prefab);
    result->mPrefab = prefab;
    result->mState = AnimState_Update;
    result->mNumNodes = prefab->numNodes;
    result->mTrigerredNorm = 0.0f;
    result->lowerBodyIdxStart = lowerBodyStart;

    ASSERT(result->mRootNodeIndex < MaxBonePoses);
    ASSERT(prefab->GetNodePtr(result->mRootNodeIndex)->numChildren > 0); // root node has to have children nodes
    
    if (!humanoid)
        return;
    
    SetBoneNode(prefab, result->mSpineNode, result->mSpineNodeIdx, "mixamorig:Spine");
    SetBoneNode(prefab, result->mNeckNode , result->mNeckNodeIdx , "mixamorig:Neck");
}

static inline void RotateNode(ANode* node, float xAngle, float yAngle)
{
    Quaternion q = QMul(QMul(QFromXAngle(xAngle), QFromYAngle(yAngle)), VecLoad(node->rotation));
    VecStore(node->rotation, q);
}

__forceinline Matrix4 GetNodeMatrix(ANode* node)
{
    return Matrix4::PositionRotationScale(node->translation, node->rotation, node->scale);
}

void AnimationController::RecurseBoneMatrices(ANode* node, Matrix4 parentMatrix)
{
    for (int c = 0; c < node->numChildren; c++)
    {
        int childIndex = node->children[c];
        ANode* children = &mPrefab->nodes[childIndex];

        if (children == mSpineNode && Abs(mSpineYAngle) + Abs(mSpineXAngle) > Epsilon) { RotateNode(children, mSpineXAngle, mSpineYAngle); }
        if (children == mNeckNode && Abs(mNeckYAngle) + Abs(mSpineXAngle) > Epsilon) { RotateNode(children, mNeckXAngle, mNeckYAngle); }

        mBoneMatrices[childIndex] = GetNodeMatrix(children) * parentMatrix;

        RecurseBoneMatrices(children, mBoneMatrices[childIndex]);
    }
}

static void MergeAnims(Pose* pose0, Pose* pose1, float animBlend, int numNodes)
{
    for (int i = 0; i < numNodes; i++)
    {
        pose0[i].rotation    = QNLerp(pose0[i].rotation, pose1[i].rotation, animBlend); // slerp+norm?
        pose0[i].translation = VecLerp(pose0[i].translation, pose1[i].translation, animBlend);
        // pose0[i].scale    = VecLerp(pose0[i].scale, pose1[i].scale, animBlend);
    }
}

static void InitNodes(ANode* nodes, Pose* pose, int begin, int numNodes)
{
    numNodes += begin;
    for (int i = begin; i < numNodes; i++)
    {
        VecStore(nodes[i].translation, pose[i].translation);
        VecStore(nodes[i].rotation, pose[i].rotation);
        // VecStore(nodes[i].scale, pose[i].scale);
    }
}

static void InitPose(Pose* pose, ANode* nodes, int numNodes)
{
    for (int i = 0; i < numNodes; i++)
    {
        pose[i].translation = VecLoad(nodes[i].translation);
        pose[i].rotation    = VecLoad(nodes[i].rotation);
        // pose[i].scale    = VecLoad(nodes[i].scale);
    }
}

void AnimationController::SampleAnimationPose(Pose* pose, int animIdx, float normTime)
{
    AAnimation* animation = &mPrefab->animations[animIdx];
    bool reverse = normTime < 0.0f;
    normTime = Abs(normTime);
    if (reverse) normTime = MAX(1.0f - normTime, 0.0f);

    InitPose(pose, mPrefab->nodes, mPrefab->numNodes);
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
        int beginIdx = 0, endIdx;
        while (realTime >= sampler.input[beginIdx + 1])
            beginIdx++;
        
        beginIdx = MIN(beginIdx, sampler.count - 1);
        endIdx   = beginIdx + 1;

        if (reverse) Swap(beginIdx, endIdx);

        vec_t begin = ((vec_t*)sampler.output)[beginIdx];
        vec_t end   = ((vec_t*)sampler.output)[endIdx];
    
        float beginTime = MAX(0.0001f, realTime - sampler.input[beginIdx]);
        float endTime   = MAX(0.0001f, sampler.input[endIdx] - sampler.input[beginIdx]);
        
        if (reverse) Swap(beginTime, endTime);
        
        float t = Clamp01(beginTime / endTime);

        switch (channel.targetPath)
        {
            case AAnimTargetPath_Translation:
                pose[targetNode].translation = VecLerp(begin, end, t);
                break;
            case AAnimTargetPath_Rotation:
                Quaternion rot = QSlerp(begin, end, t);
                pose[targetNode].rotation = QNorm(rot);// QNormEst
                break;
        //  case AAnimTargetPath_Scale:
        //      pose[targetNode].scale = VecLerp(begin, end, t);
        //      break;
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

void AnimationController::UploadPose(Pose* pose)
{
    InitNodes(mPrefab->nodes, pose, 0, mPrefab->numNodes);

    ANode* rootNode = mPrefab->GetNodePtr(mRootNodeIndex);
    mBoneMatrices[mRootNodeIndex] = GetNodeMatrix(rootNode);

    RecurseBoneMatrices(rootNode, mBoneMatrices[mRootNodeIndex]);
    UploadBoneMatrices();
}

// when we want to play different animations with lower body and upper body
void AnimationController::UploadPoseUpperLower(Pose* lowerPose, Pose* uperPose)
{
    // apply posess to lower body and upper body seperately, so both of it has diferrent animations
    InitNodes(mPrefab->nodes, lowerPose, lowerBodyIdxStart, mPrefab->numNodes - lowerBodyIdxStart);
    InitNodes(mPrefab->nodes, uperPose, 0, lowerBodyIdxStart);

    ANode* rootNode = mPrefab->GetNodePtr(mRootNodeIndex);
    Matrix4 rootMatrix = GetNodeMatrix(rootNode);
    mBoneMatrices[mRootNodeIndex] = rootMatrix;

    RecurseBoneMatrices(rootNode, rootMatrix);
    UploadBoneMatrices();
}

void AnimationController::PlayAnim(int index, float norm)
{
    SampleAnimationPose(mAnimPoseA, index, norm);
    UploadPose(mAnimPoseA);
}

void AnimationController::TriggerAnim(int index, float transitionInTime, float transitionOutTime, eAnimTriggerOpt triggerOpt)
{
    if (IsTrigerred()) return; // already trigerred

    mTriggerredAnim = index;
    mTriggerOpt = triggerOpt;
    mTransitionTime = transitionInTime;
    mCurTransitionTime = transitionInTime;
    mTransitionOutTime = transitionOutTime;
    if (transitionInTime < 0.02f) { // no transition requested
        mState = AnimState_TriggerPlaying;
        return;
    }

    mState = AnimState_TriggerIn;
    SmallMemCpy(mAnimPoseC, mAnimPoseA, sizeof(mAnimPoseA));
    if (EnumHasBit(triggerOpt, eAnimTriggerOpt_ReverseOut))
        mAnimTime.y = 0.0f;
}

bool AnimationController::TriggerTransition(float deltaTime, int targetAnim)
{
    float newNorm   = Clamp01((mTransitionTime - mCurTransitionTime) / mTransitionTime);
    float animDelta = Clamp01(deltaTime * (1.0f / MAX(1.0f - newNorm, Epsilon)));
    SampleAnimationPose(mAnimPoseD, targetAnim, mAnimTime.y);
    MergeAnims(mAnimPoseC, mAnimPoseD, animDelta, mNumNodes);
    mCurTransitionTime -= deltaTime;
    return mCurTransitionTime <= 0.0f;
}

// x, y has to be between -1.0 and 1.0
void AnimationController::EvaluateLocomotion(float x, float y, float animSpeed)
{
    const float deltaTime = (float)GetDeltaTime();
    bool wasTriggerState = IsTrigerred();

    if (mState == AnimState_TriggerIn)
    {
        if (TriggerTransition(deltaTime, mTriggerredAnim)) 
            mState = AnimState_TriggerPlaying;
    }
    else if (mState == AnimState_TriggerOut)
    {
        if (!EnumHasBit(mTriggerOpt, eAnimTriggerOpt_ReverseOut)) 
        {
            if (TriggerTransition(deltaTime, mLastAnim)) 
                mState = AnimState_Update;
        }
        else 
        {
            SampleAnimationPose(mAnimPoseC, mTriggerredAnim, -mTrigerredNorm);
            float animStep = 1.0f / mPrefab->animations[mTriggerredAnim].duration;
            mTrigerredNorm = Clamp01(mTrigerredNorm + (animSpeed * animStep * deltaTime));
            if (mTrigerredNorm >= 1.0f)
                mState = AnimState_Update;
        }
    }
    else if (mState == AnimState_TriggerPlaying)
    {
        SampleAnimationPose(mAnimPoseC, mTriggerredAnim, mTrigerredNorm);

        float animStep = 1.0f / mPrefab->animations[mTriggerredAnim].duration;
        mTrigerredNorm = Clamp01(mTrigerredNorm + (animSpeed * animStep * deltaTime));

        if (mTrigerredNorm >= 1.0f)
        {
            mTrigerredNorm = 0.0f; // trigger stage complated
            mTransitionTime = mTransitionOutTime;
            mCurTransitionTime = mTransitionOutTime;
            mState = mTransitionTime < 0.02f ? AnimState_Update : AnimState_TriggerOut;
        }
    }

    int yIndex = GetAnim(aMiddle, 0);
    // if trigerred animation is not standing, we don't have to sample walking or running animations
    if (!wasTriggerState || (wasTriggerState && EnumHasBit(mTriggerOpt, eAnimTriggerOpt_Standing) && Abs(y) > 0.001f))
    {
        // play and blend walking and running anims
        y = Abs(y); 
        int yi = int(y);
        // sample y anim
        ASSERTR(yi <= 3, return); // must be between 1 and 4
        yIndex = GetAnim(aMiddle, yi);

        SampleAnimationPose(mAnimPoseA, yIndex, mAnimTime.y);
        float yBlend = Fract(y);

        bool shouldAnimBlendY = yi != 3 && yBlend > 0.00002f;
        if (shouldAnimBlendY)
        {
            yIndex = GetAnim(aMiddle, yi + 1);
            SampleAnimationPose(mAnimPoseB, yIndex, mAnimTime.y);
            MergeAnims(mAnimPoseA, mAnimPoseB, EaseOut(yBlend), mNumNodes);
        }

        // if anim is two seconds animStep is 0.5 because we are using normalized value
        float yAnimStep = 1.0f / mPrefab->animations[yIndex].duration;
        mAnimTime.y += animSpeed * yAnimStep * deltaTime;
        mAnimTime.y  = Fract(mAnimTime.y);
    }
    mLastAnim = yIndex;

    if (!wasTriggerState) {
        UploadPose(mAnimPoseA);
    }
    else {
        if (EnumHasBit(mTriggerOpt, eAnimTriggerOpt_Standing) && y > 0.001f)
            UploadPoseUpperLower(mAnimPoseA, mAnimPoseC);
        else
            UploadPose(mAnimPoseC);
    }
}

void ClearAnimationController(AnimationController* animSystem)
{
    rDeleteTexture(animSystem->mMatrixTex);
}

void DestroyAnimationSystem()
{ }

