
#include "Animation.hpp"
#include "Scene.hpp"
#include "SceneRenderer.hpp"

struct Matrix3x4f16
{
    half x[4];
    half y[4];
    half z[4];
};

static Shader m_AnimCompute;

void StartAnimationSystem()
{
    m_AnimCompute = rImportComputeShader("Shaders/AnimCompute.glsl");
}

static void RecurseNodeMatrices(ANode* node, ANode* nodes, Matrix4 parentMatrix, Matrix4* matrices)
{
    for (int c = 0; c < node->numChildren; c++)
    {
        ASSERT(c < 128);
        int childIndex = node->children[c];
        ANode* children = &nodes[childIndex];
        Matrix4 childTranslation = Matrix4::PositionRotationScale(children->translation, children->rotation, children->scale) * parentMatrix;
        matrices[childIndex] = childTranslation;

        RecurseNodeMatrices(children, nodes, childTranslation, matrices);
    }
}

void CreateAnimationController(Prefab* prefab, AnimationController* result)
{
    const int MaxJoints = SceneRenderer::MaxNumJoints;
    ASkin& skin = prefab->skins[0];
    ASSERT(skin.numJoints < MaxJoints);

    Matrix4 nodeMatrices[MaxJoints];
    result->animTextures  = new AnimTexture[prefab->numAnimations];
    result->numAnimations = prefab->numAnimations;

    int skeletonNode = 0;
    if (prefab->numScenes > 0) {
        AScene defaultScene = prefab->scenes[prefab->defaultSceneIndex];
        skeletonNode = defaultScene.nodes[0];
    }

    for (int a = 0; a < prefab->numAnimations; a++)
    {
        AAnimation& animation = prefab->animations[a];
        int frameCount = 30 * MAX(int(animation.duration), 1);
        result->animTextures[a].numFrames = frameCount;

        int numElements = frameCount * skin.numJoints;

        Matrix3x4f16* matrices = new Matrix3x4f16[numElements];

        // todo: multi thread this
        for (int frame = 0; frame < frameCount; frame++)
        {
            float realTime = (float(frame) / float(frameCount)) * animation.duration;
    
            for (int c = 0; c < animation.numChannels; c++)
            {
                AAnimChannel& channel = animation.channels[c];
                ANode* targetNode = prefab->GetNodePtr(channel.targetNode);
                AAnimSampler& sampler = animation.samplers[channel.sampler];
            
                // morph targets are not supported
                if (channel.targetPath == AAnimTargetPath_Weight)
                    continue;
            
                int beginIdx = 0;
                while (realTime > sampler.input[beginIdx + 1])
                    beginIdx++;

                vec_t begin = ((vec_t*)sampler.output)[beginIdx]; 
                vec_t end   = ((vec_t*)sampler.output)[beginIdx + 1];
            
                float t = (realTime - sampler.input[beginIdx]) / MAX(sampler.input[beginIdx + 1] - sampler.input[beginIdx], 0.0001f);

                switch (channel.targetPath)
                {
                    case AAnimTargetPath_Scale:
                        Vec3Store(targetNode->scale, VecLerp(begin, end, t));
                        break;
                    case AAnimTargetPath_Translation:
                        Vec3Store(targetNode->translation, VecLerp(begin, end, t));
                        break;
                    case AAnimTargetPath_Rotation:
                        Quaternion rot = QSlerp(begin, end, t);
                        rot = QNorm(rot);
                        VecStore(targetNode->rotation, rot);
                        break;
                };
            }
            int rootNodeIdx = skin.skeleton != -1 ? skin.skeleton : skeletonNode; ASSERT(rootNodeIdx < MaxJoints);
            ANode* rootNode = prefab->GetNodePtr(rootNodeIdx);
            Matrix4 rootMatrix = Matrix4::PositionRotationScale(rootNode->translation, rootNode->rotation, rootNode->scale);
            nodeMatrices[rootNodeIdx] = rootMatrix;
            RecurseNodeMatrices(rootNode, prefab->nodes, rootMatrix, nodeMatrices);
         
            Matrix4* invMatrices = (Matrix4*)skin.inverseBindMatrices;
            int index = frame * skin.numJoints;
         
            for (int i = 0; i < skin.numJoints; i++)
            {
                Matrix4 mat = invMatrices[i] * nodeMatrices[skin.joints[i]];
                mat = Matrix4::Transpose(mat);
                ConvertFloatToHalf(matrices[index + i].x, (float*)&mat.r[0], 4);
                ConvertFloatToHalf(matrices[index + i].y, (float*)&mat.r[1], 4);
                ConvertFloatToHalf(matrices[index + i].z, (float*)&mat.r[2], 4);
            }
        }    

        // todo: add this to queue, seperate threads that pushes to opengl and creates data
        result->animTextures[a].matrixTex = rCreateTexture(skin.numJoints*3, frameCount, matrices, TextureType_RGBA16F, TexFlags_RawData);
        delete[] matrices;
    }
    
    int squareSize = (int)Sqrt((float)prefab->bigMesh.numVertex);
    ASSERT(squareSize < 600); // if asserted just use greater number that is multiple of 3
    // this will be texture that holds transformation data, 3x4 rgba16f, after that we will transpose our matrix
    // max vertex count of mesh that will use this animation is= (600 * 600) / 3;  120000 vert
    result->jointComputeOutTex = rCreateTexture(600, squareSize, nullptr, TextureType_RGBA16F, TexFlags_RawData);
}

void EvaluateAnimOfPrefab(Prefab* prefab, int animIndex, double animTime, AnimationController* animSystem)
{
    const bool hasAnimation = prefab->numSkins > 0 && animSystem != nullptr;
    
    if (!hasAnimation)
        return;

    rBindShader(m_AnimCompute);
    AnimTexture animTexture = animSystem->animTextures[animIndex];
    ASkin&      skin = prefab->skins[0];
    AAnimation& animation = prefab->animations[animIndex];
    
    int numFrames = animTexture.numFrames;
    animTime = (animTime * numFrames) / (double)animation.duration;
    
    float time = (float)FMod(animTime, (double)(numFrames));
    float frame = Floor(time);
    float animBlend = (time - frame);
    frame = MAX(0.0f, frame - 1.0f);
    
    ASSERT(frame < (float)numFrames);
    ASSERT(animBlend <= 1.0f && animBlend >= 0.0f);
    
    Vector2f firstAnimParam  = MakeVec2(frame, animBlend);

    rSetShaderValue(&firstAnimParam, 0, GraphicType_Vector2f);

    rBindTextureToCompute(animTexture.matrixTex, 0, TextureAccess_ReadOnly);
    rBindTextureToCompute(animSystem->jointComputeOutTex, 1, TextureAccess_WriteOnly);
    rComputeShaderBindBuffer(0, prefab->bigMesh.vertexHandle);
    
    rDispatchComputeShader(m_AnimCompute, prefab->bigMesh.numVertex / 32 + 1, 1, 1);
}

void ClearAnimationController(AnimationController* animSystem)
{
    for (int i = 0; i < animSystem->numAnimations; i++)
    {
        rDeleteTexture(animSystem->animTextures[i].matrixTex);
    }
    
    delete[] animSystem->animTextures;
}

void DestroyAnimationSystem()
{
    rDeleteShader(m_AnimCompute);
}

