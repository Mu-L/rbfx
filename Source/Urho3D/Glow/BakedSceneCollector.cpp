//
// Copyright (c) 2008-2019 the Urho3D project.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

/// \file

#include "../Glow/BakedSceneCollector.h"

#include "../Glow/RaytracerScene.h"
#include "../Graphics/Light.h"
#include "../Graphics/LightProbeGroup.h"
#include "../Graphics/Octree.h"
#include "../Graphics/StaticModel.h"
#include "../Graphics/Terrain.h"
#include "../Graphics/Zone.h"
#include "../IO/Log.h"
#include "../Scene/Scene.h"

#include <EASTL/algorithm.h>

namespace Urho3D
{

namespace
{

BoundingBox CalculateLightmappedSceneBoundingBox(const ea::vector<Node*>& nodes)
{
    ea::vector<StaticModel*> staticModels;
    ea::vector<Terrain*> terrains;
    ea::vector<LightProbeGroup*> lightProbeGroups;

    BoundingBox boundingBox;
    for (Node* node : nodes)
    {
        node->GetComponents(staticModels);
        node->GetComponents(terrains);
        node->GetComponents(lightProbeGroups);

        for (StaticModel* staticModel : staticModels)
        {
            if (staticModel->IsEnabledEffective() && staticModel->GetBakeLightmap())
                boundingBox.Merge(staticModel->GetWorldBoundingBox());
        }

#if 0
        for (Terrain* terrain : terrains)
        {
            const IntVector2 numPatches = terrain->GetNumPatches();
            for (unsigned i = 0; i < numPatches.x_ * numPatches.y_; ++i)
            {
                if (TerrainPatch* terrainPatch = terrain->GetPatch(i))
                    boundingBox.Merge(terrainPatch->GetWorldBoundingBox());
            }
        }
#endif

        for (LightProbeGroup* lightProbeGroup : lightProbeGroups)
        {
            if (lightProbeGroup->IsEnabledEffective())
                boundingBox.Merge(lightProbeGroup->GetWorldBoundingBox());
        }
    }

    // Pad bounding box
    const Vector3 size = boundingBox.Size();
    if (size.x_ < M_EPSILON)
        boundingBox.max_.x_ += M_LARGE_EPSILON;
    if (size.y_ < M_EPSILON)
        boundingBox.max_.y_ += M_LARGE_EPSILON;
    if (size.z_ < M_EPSILON)
        boundingBox.max_.z_ += M_LARGE_EPSILON;

    return boundingBox;
}

}

BakedSceneCollector::~BakedSceneCollector() = default;

void DefaultBakedSceneCollector::LockScene(Scene* scene, const Vector3& chunkSize)
{
    scene_ = scene;
    chunkSize_ = chunkSize;
    octree_ = scene_->GetComponent<Octree>();

    // Estimate dimensions
    const ea::vector<Node*> children = scene_->GetChildren(true);
    boundingBox_ = CalculateLightmappedSceneBoundingBox(children);
    chunkGridDimension_ = VectorMax(IntVector3::ONE, VectorRoundToInt(boundingBox_.Size() / chunkSize_));
    const IntVector3 maxChunk = chunkGridDimension_ - IntVector3::ONE;

    // Collect light probe groups
    scene_->GetComponents(lightProbeGroups_, true);

    // Collect nodes
    ea::vector<StaticModel*> staticModels;
    ea::vector<Terrain*> terrains;
    ea::vector<LightProbeGroup*> lightProbeGroups;

    for (Node* node : children)
    {
        const Vector3 position = node->GetWorldPosition();
        const Vector3 index = (position - boundingBox_.min_) / boundingBox_.Size() * Vector3(chunkGridDimension_);
        const IntVector3 chunk = VectorMin(VectorMax(IntVector3::ZERO, VectorFloorToInt(index)), maxChunk);
        ChunkData& chunkData = chunks_[chunk];

        node->GetComponents(staticModels);
        node->GetComponents(terrains);
        node->GetComponents(lightProbeGroups);

        for (StaticModel* staticModel : staticModels)
        {
            if (staticModel->IsEnabledEffective() && staticModel->GetBakeLightmap())
            {
                chunkData.staticModels_.push_back(staticModel);
                chunkData.boundingBox_.Merge(staticModel->GetWorldBoundingBox());
            }
        }

        for (LightProbeGroup* lightProbeGroup : lightProbeGroups)
        {
            if (lightProbeGroup->IsEnabledEffective())
            {
                chunkData.lightProbeGroups_.push_back(lightProbeGroup);
                chunkData.boundingBox_.Merge(lightProbeGroup->GetWorldBoundingBox());
            }
        }
    }
}

ea::vector<IntVector3> DefaultBakedSceneCollector::GetChunks()
{
    return chunks_.keys();
}

ea::vector<StaticModel*> DefaultBakedSceneCollector::GetUniqueStaticModels(const IntVector3& chunkIndex)
{
    auto iter = chunks_.find(chunkIndex);
    if (iter != chunks_.end())
        return iter->second.staticModels_;
    return {};
}

void DefaultBakedSceneCollector::CommitStaticModels(const IntVector3& /*chunkIndex*/)
{
}

ea::vector<LightProbeGroup*> DefaultBakedSceneCollector::GetUniqueLightProbeGroups(const IntVector3& chunkIndex)
{
    auto iter = chunks_.find(chunkIndex);
    if (iter != chunks_.end())
        return iter->second.lightProbeGroups_;
    return {};
}

void DefaultBakedSceneCollector::CommitLightProbeGroups(const IntVector3& /*chunkIndex*/)
{
}

BoundingBox DefaultBakedSceneCollector::GetChunkBoundingBox(const IntVector3& chunkIndex)
{
    auto iter = chunks_.find(chunkIndex);
    if (iter != chunks_.end())
        return iter->second.boundingBox_;
    return {};
}

ea::vector<Light*> DefaultBakedSceneCollector::GetLightsInBoundingBox(
    const IntVector3& /*chunkIndex*/, const BoundingBox& boundingBox)
{
    // Query drawables
    ea::vector<Drawable*> drawables;
    BoxOctreeQuery query(drawables, boundingBox, DRAWABLE_LIGHT);
    octree_->GetDrawables(query);

    // Collect lights
    ea::vector<Light*> lights;
    for (Drawable* drawable : drawables)
    {
        if (auto light = dynamic_cast<Light*>(drawable))
        {
            if (light->GetLightMode() != LM_REALTIME)
                lights.push_back(light);
        }
    }
    return lights;
}

ea::vector<StaticModel*> DefaultBakedSceneCollector::GetStaticModelsInBoundingBox(
    const IntVector3& /*chunkIndex*/, const BoundingBox& boundingBox)
{
    // Query drawables
    ea::vector<Drawable*> drawables;
    BoxOctreeQuery query(drawables, boundingBox, DRAWABLE_GEOMETRY);
    octree_->GetDrawables(query);

    // Collect static models
    ea::vector<StaticModel*> staticModels;
    for (Drawable* drawable : drawables)
    {
        if (auto staticModel = dynamic_cast<StaticModel*>(drawable))
        {
            if (staticModel->GetBakeLightmap())
                staticModels.push_back(staticModel);
        }
    }
    return staticModels;
}

ea::vector<LightProbeGroup*> DefaultBakedSceneCollector::GetLightProbeGroupsInBoundingBox(
    const IntVector3& /*chunkIndex*/, const BoundingBox& boundingBox)
{
    auto isAppropriate = [&](const LightProbeGroup* group)
    {
        return group->IsEnabledEffective() && group->GetWorldBoundingBox().IsInside(boundingBox) != OUTSIDE;
    };
    ea::vector<LightProbeGroup*> groups;
    ea::copy_if(lightProbeGroups_.begin(), lightProbeGroups_.end(), ea::back_inserter(groups), isAppropriate);
    return groups;
}

ea::vector<StaticModel*> DefaultBakedSceneCollector::GetStaticModelsInFrustum(
    const IntVector3& /*chunkIndex*/, const Frustum& frustum)
{
    // Query drawables
    ea::vector<Drawable*> drawables;
    FrustumOctreeQuery query(drawables, frustum, DRAWABLE_GEOMETRY);
    octree_->GetDrawables(query);

    // Collect static models
    ea::vector<StaticModel*> staticModels;
    for (Drawable* drawable : drawables)
    {
        if (auto staticModel = dynamic_cast<StaticModel*>(drawable))
        {
            if (staticModel->GetBakeLightmap())
                staticModels.push_back(staticModel);
        }
    }
    return staticModels;
}

void DefaultBakedSceneCollector::UnlockScene()
{
    scene_ = nullptr;
    chunkSize_ = Vector3::ZERO;
    boundingBox_ = BoundingBox{};
    chunkGridDimension_ = IntVector3::ZERO;
    octree_ = nullptr;
    chunks_.clear();
}

}