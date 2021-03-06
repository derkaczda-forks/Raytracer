#include "PCH.h"
#include "Scene.h"
#include "Light/BackgroundLight.h"
#include "Object/SceneObject_Light.h"
#include "Rendering/ShadingData.h"
#include "BVH/BVHBuilder.h"
#include "Material/Material.h"

#include "Traversal/Traversal_Single.h"
#include "Traversal/Traversal_Packet.h"

namespace rt {

using namespace math;

Scene::Scene() = default;

Scene::~Scene() = default;

Scene::Scene(Scene&&) = default;

Scene& Scene::operator = (Scene&&) = default;

void Scene::AddLight(LightPtr object)
{
    mLights.PushBack(std::move(object));
}

void Scene::AddObject(SceneObjectPtr object)
{
    mObjects.PushBack(std::move(object));
}

bool Scene::BuildBVH()
{
    for (const LightPtr& light : mLights)
    {
        if (!light->IsDelta() && light->IsFinite())
        {
            mObjects.EmplaceBack(std::make_unique<LightSceneObject>(*light));
        }
        else if (!light->IsFinite())
        {
            mGlobalLights.PushBack(light.get());
        }
    }

    DynArray<Box> boxes;
    for (const auto& obj : mObjects)
    {
        boxes.PushBack(obj->GetBoundingBox());
    }

    BVHBuilder::BuildingParams params;
    params.maxLeafNodeSize = 2;

    BVHBuilder::Indices newOrder;
    BVHBuilder bvhBuilder(mBVH);
    if (!bvhBuilder.Build(boxes.Data(), (Uint32)mObjects.Size(), params, newOrder))
    {
        return false;
    }

    DynArray<SceneObjectPtr> newObjectsArray;
    newObjectsArray.Reserve(mObjects.Size());
    for (Uint32 i = 0; i < mObjects.Size(); ++i)
    {
        Uint32 sourceIndex = newOrder[i];
        newObjectsArray.PushBack(std::move(mObjects[sourceIndex]));
    }

    mObjects = std::move(newObjectsArray);

    return true;
}

const ILight& Scene::GetLightByObjectId(Uint32 id) const
{
    const LightSceneObject* lightSceneObj = static_cast<const LightSceneObject*>(mObjects[id].get());
    return lightSceneObj->GetLight();
}

void Scene::Traverse_Object_Single(const SingleTraversalContext& context, const Uint32 objectID) const
{
    const ISceneObject* object = mObjects[objectID].get();

    const Matrix4 invTransform = object->ComputeInverseTransform(context.context.time);

    // transform ray to local-space
    const Ray transformedRay = invTransform.TransformRay_Unsafe(context.ray);

    const SingleTraversalContext objectContext =
    {
        transformedRay,
        context.hitPoint,
        context.context
    };

    object->Traverse_Single(objectContext, objectID);
}

bool Scene::Traverse_Object_Shadow_Single(const SingleTraversalContext& context, const Uint32 objectID) const
{
    const ISceneObject* object = mObjects[objectID].get();

    const Matrix4 invTransform = object->ComputeInverseTransform(context.context.time);

    // transform ray to local-space
    Ray transformedRay = invTransform.TransformRay_Unsafe(context.ray);
    transformedRay.originDivDir = transformedRay.origin * transformedRay.invDir;

    const SingleTraversalContext objectContext =
    {
        transformedRay,
        context.hitPoint,
        context.context
    };

    return object->Traverse_Shadow_Single(objectContext);
}

void Scene::Traverse_Leaf_Single(const SingleTraversalContext& context, const Uint32 objectID, const BVH::Node& node) const
{
    RT_UNUSED(objectID);

    for (Uint32 i = 0; i < node.numLeaves; ++i)
    {
        const Uint32 objectIndex = node.childIndex + i;
        Traverse_Object_Single(context, objectIndex);
    }
}

bool Scene::Traverse_Leaf_Shadow_Single(const SingleTraversalContext& context, const BVH::Node& node) const
{
    for (Uint32 i = 0; i < node.numLeaves; ++i)
    {
        const Uint32 objectIndex = node.childIndex + i;
        if (Traverse_Object_Shadow_Single(context, objectIndex))
        {
            return true;
        }
    }

    return false;
}

void Scene::Traverse_Leaf_Packet(const PacketTraversalContext& context, const Uint32 objectID, const BVH::Node& node, Uint32 numActiveGroups) const
{
    RT_UNUSED(objectID);

    for (Uint32 i = 0; i < node.numLeaves; ++i)
    {
        const Uint32 objectIndex = node.childIndex + i;
        const ISceneObject* object = mObjects[objectIndex].get();
        const Matrix4 invTransform = object->ComputeInverseTransform(context.context.time);

        // transform ray to local-space
        for (Uint32 j = 0; j < numActiveGroups; ++j)
        {
            RayGroup& rayGroup = context.ray.groups[context.context.activeGroupsIndices[j]];
            rayGroup.rays[1].origin = invTransform.TransformPoint(rayGroup.rays[0].origin);
            rayGroup.rays[1].dir = invTransform.TransformVector(rayGroup.rays[0].dir);
            rayGroup.rays[1].invDir = Vector3x8::FastReciprocal(rayGroup.rays[1].dir);
        }

        object->Traverse_Packet(context, objectIndex, numActiveGroups);
    }
}

void Scene::Traverse_Single(const SingleTraversalContext& context) const
{
    const Uint32 numObjects = mObjects.Size();

    if (numObjects == 0) // scene is empty
    {
        return;
    }
    else if (numObjects == 1) // bypass BVH
    {
        Traverse_Object_Single(context, 0);
    }
    else // full BVH traversal
    {
        GenericTraverse_Single(context, 0, this);
    }
}

bool Scene::Traverse_Shadow_Single(const SingleTraversalContext& context) const
{
    const Uint32 numObjects = mObjects.Size();

    if (numObjects == 0) // scene is empty
    {
        return false;
    }
    else if (numObjects == 1) // bypass BVH
    {
        return Traverse_Object_Shadow_Single(context, 0);
    }
    else // full BVH traversal
    {
        return GenericTraverse_Shadow_Single(context, this);
    }
}

void Scene::Traverse_Packet(const PacketTraversalContext& context) const
{
    const Uint32 numObjects = mObjects.Size();

    const Uint32 numRayGroups = context.ray.GetNumGroups();
    for (Uint32 i = 0; i < numRayGroups; ++i)
    {
        context.ray.groups[i].maxDistances = VECTOR8_MAX;
        context.context.activeGroupsIndices[i] = (Uint16)i;
    }

    for (Uint32 i = 0; i < context.ray.numRays; ++i)
    {
        context.context.hitPoints[i].distance = FLT_MAX;
        context.context.hitPoints[i].objectId = UINT32_MAX;
    }

    if (numObjects == 0) // scene is empty
    {
        return;
    }
    else if (numObjects == 1) // bypass BVH
    {
        const ISceneObject* object = mObjects.Front().get();
        const Matrix4 invTransform = object->ComputeInverseTransform(context.context.time);

        for (Uint32 j = 0; j < numRayGroups; ++j)
        {
            RayGroup& rayGroup = context.ray.groups[context.context.activeGroupsIndices[j]];
            rayGroup.rays[1].origin = invTransform.TransformPoint(rayGroup.rays[0].origin);
            rayGroup.rays[1].dir = invTransform.TransformVector(rayGroup.rays[0].dir);
            rayGroup.rays[1].invDir = Vector3x8::FastReciprocal(rayGroup.rays[1].dir);
        }

        mObjects.Front()->Traverse_Packet(context, 0, numRayGroups);
    }
    else // full BVH traversal
    {
        GenericTraverse_Packet<Scene, 0>(context, 0, this, numRayGroups);
    }
}

void Scene::ExtractShadingData(const math::Ray& ray, const HitPoint& hitPoint, const float time, ShadingData& outShadingData) const
{
    RT_ASSERT(hitPoint.distance < FLT_MAX);

    const ISceneObject* object = mObjects[hitPoint.objectId].get();

    const Matrix4 transform = object->ComputeTransform(time);
    const Matrix4 invTransform = transform.FastInverseNoScale();

    const Vector4 worldPosition = ray.GetAtDistance(hitPoint.distance);
    outShadingData.frame[3] = invTransform.TransformPoint(worldPosition);

    // calculate normal, tangent, tex coord, etc. from intersection data
    object->EvaluateShadingData_Single(hitPoint, outShadingData);

    Matrix4 localSpaceFrame = outShadingData.frame;

    RT_ASSERT(outShadingData.texCoord.IsValid());

    // apply normal mapping
    if (outShadingData.material->normalMap)
    {
        const Vector4 localNormal = outShadingData.material->GetNormalVector(outShadingData.texCoord);

        // transform normal vector
        {
            Vector4 newNormal = localSpaceFrame[0] * localNormal.x;
            newNormal = Vector4::MulAndAdd(localSpaceFrame[1], localNormal.y, newNormal);
            newNormal = Vector4::MulAndAdd(localSpaceFrame[2], localNormal.z, newNormal);
            localSpaceFrame[2] = newNormal.FastNormalized3();
        }
    }

    // orthogonalize tangent vector (required due to normal/tangent vectors interpolation and normal mapping)
    // TODO this can be skipped if the tangent vector is the same for every point on the triangle (flat shading)
    // and normal mapping is disabled
    localSpaceFrame[0] = Vector4::Orthogonalize(localSpaceFrame[0], localSpaceFrame[2]).Normalized3();

    // Note: no need to normalize, as normal and tangent are both normalized and orthogonal
    localSpaceFrame[1] = Vector4::Cross3(localSpaceFrame[0], localSpaceFrame[2]);

    // TODO uncomment this after ExtractShadingData() is not called for light objects
    //RT_ASSERT(Abs(1.0f - outShadingData.frame[0].SqrLength3()) < 0.001f);
    //RT_ASSERT(Abs(1.0f - outShadingData.frame[1].SqrLength3()) < 0.001f);
    //RT_ASSERT(Abs(1.0f - outShadingData.frame[2].SqrLength3()) < 0.001f);
    //RT_ASSERT(Vector4::Dot3(outShadingData.frame[1], outShadingData.frame[0]) < 0.001f);
    //RT_ASSERT(Vector4::Dot3(outShadingData.frame[1], outShadingData.frame[2]) < 0.001f);
    //RT_ASSERT(Vector4::Dot3(outShadingData.frame[2], outShadingData.frame[0]) < 0.001f);

    // transform shading data from local space to world space
    outShadingData.frame[0] = transform.TransformVector(localSpaceFrame[0]);
    outShadingData.frame[1] = transform.TransformVector(localSpaceFrame[1]);
    outShadingData.frame[2] = transform.TransformVector(localSpaceFrame[2]);
    outShadingData.frame[3] = worldPosition;

    outShadingData.outgoingDirWorldSpace = -ray.dir;
}

} // namespace rt
