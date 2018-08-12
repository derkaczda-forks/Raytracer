#include "PCH.h"
#include "Scene.h"
#include "Rendering/Context.h"
#include "Rendering/ShadingData.h"
#include "Mesh/Mesh.h"
#include "Material/Material.h"
#include "Math/Transcendental.h"
#include "Utils/Bitmap.h"
#include "BVH/BVHBuilder.h"

#include "Traversal/TraversalContext.h"
#include "Traversal/Traversal_Single.h"
#include "Traversal/Traversal_Simd.h"
#include "Traversal/Traversal_Packet.h"

#include "Color/Color.h"


namespace rt {

using namespace math;

Scene::Scene()
{
}

void Scene::SetEnvironment(const SceneEnvironment& env)
{
    mEnvironment = env;
}

void Scene::AddObject(SceneObjectPtr object)
{
    mObjects.push_back(std::move(object));
}

bool Scene::BuildBVH()
{
    std::vector<Box, AlignmentAllocator<Box>> boxes;

    for (const auto& obj : mObjects)
    {
        boxes.push_back(obj->GetBoundingBox());
    }

    BVHBuilder::BuildingParams params;
    params.maxLeafNodeSize = 2;

    BVHBuilder::Indices newOrder;
    BVHBuilder bvhBuilder(mBVH);
    if (!bvhBuilder.Build(boxes.data(), (Uint32)mObjects.size(), params, newOrder))
    {
        return false;
    }

    std::vector<SceneObjectPtr> newObjectsArray;
    newObjectsArray.reserve(mObjects.size());
    for (size_t i = 0; i < mObjects.size(); ++i)
    {
        Uint32 sourceIndex = newOrder[i];
        newObjectsArray.push_back(std::move(mObjects[sourceIndex]));
    }

    mObjects = std::move(newObjectsArray);

    return true;
}

// TODO this has nothing to do with a scene
Uint32 Hash(const Uint32 value)
{
    // FNV-1a
    Uint32 hash = 2166136261u;

    hash ^= value & 0xFF;
    hash *= 16777619u;

    hash ^= (value >> 8) & 0xFF;
    hash *= 16777619u;

    hash ^= (value >> 16) & 0xFF;
    hash *= 16777619u;

    hash ^= (value >> 24) & 0xFF;
    hash *= 16777619u;

    return hash;
}

// TODO this has nothing to do with a scene
RayColor Scene::HandleSpecialRenderingMode(RenderingContext& context, const HitPoint& hitPoint, const ShadingData& shadingData)
{
    switch (context.params->renderingMode)
    {
        case RenderingMode::Depth:
        {
            const float logDepth = std::max<float>(0.0f, (log2f(hitPoint.distance) + 5.0f) / 10.0f);
            return Vector4(logDepth);
        }
#ifdef RT_ENABLE_INTERSECTION_COUNTERS
        case RenderingMode::RayBoxIntersection:
        {
            const float num = static_cast<float>(context.localCounters.numRayBoxTests);
            return Vector4(num * 0.01f, num * 0.004f, num * 0.001f, 0.0f);
        }
        case RenderingMode::RayBoxIntersectionPassed:
        {
            const float num = static_cast<float>(context.localCounters.numPassedRayBoxTests);
            return Vector4(num * 0.01f, num * 0.005f, num * 0.001f, 0.0f);
        }
        case RenderingMode::RayTriIntersection:
        {
            const float num = static_cast<float>(context.localCounters.numRayTriangleTests);
            return Vector4(num * 0.01f, num * 0.004f, num * 0.001f, 0.0f);
        }
        case RenderingMode::RayTriIntersectionPassed:
        {
            const float num = static_cast<float>(context.localCounters.numPassedRayTriangleTests);
            return Vector4(num * 0.01f, num * 0.004f, num * 0.001f, 0.0f);
        }
#endif // RT_ENABLE_INTERSECTION_COUNTERS
        case RenderingMode::Normals:
        {
            return Vector4::MulAndAdd(shadingData.normal, VECTOR_HALVES, VECTOR_HALVES);
        }
        case RenderingMode::Tangents:
        {
            return Vector4::MulAndAdd(shadingData.tangent, VECTOR_HALVES, VECTOR_HALVES);
        }
        case RenderingMode::Bitangents:
        {
            return Vector4::MulAndAdd(shadingData.bitangent, VECTOR_HALVES, VECTOR_HALVES);
        }
        case RenderingMode::Position:
        {
            return Vector4::MulAndAdd(shadingData.position, VECTOR_HALVES, VECTOR_HALVES);
        }
        case RenderingMode::TexCoords:
        {
            return Vector4(fmodf(shadingData.texCoord[0], 1.0f), fmodf(shadingData.texCoord[1], 1.0f), 0.0f, 0.0f);
        }
        case RenderingMode::BaseColor:
        {
            return shadingData.material->GetBaseColor(shadingData.texCoord);
        }
        case RenderingMode::TriangleID:
        {
            const Uint32 hash = Hash(hitPoint.objectId + hitPoint.triangleId);
            const float hue = (float)hash / (float)UINT32_MAX;
            return HSVtoRGB(hue, 0.95f, 1.0f);
        }
    }

    return RayColor();
}

void Scene::Traverse_Leaf_Single(const SingleTraversalContext& context, const Uint32 objectID, const BVH::Node& node) const
{
    RT_UNUSED(objectID);

    for (Uint32 i = 0; i < node.numLeaves; ++i)
    {
        const Uint32 objectIndex = node.childIndex + i;
        const ISceneObject* object = mObjects[objectIndex].get();

        const float time = 0.0f; // TODO
        const auto invTransform = object->GetInverseTransform(time);

        // transform ray to local-space
        math::Ray transformedRay;
        transformedRay.origin = invTransform.TransformPoint(context.ray.origin);
        transformedRay.dir = invTransform.TransformVector(context.ray.dir);
        transformedRay.invDir = Vector4::FastReciprocal(transformedRay.dir);

        SingleTraversalContext objectContext =
        {
            transformedRay,
            context.hitPoint,
            context.context
        };

        object->Traverse_Single(objectContext, objectIndex);
    }
}

void Scene::Traverse_Leaf_Simd8(const SimdTraversalContext& context, const Uint32 objectID, const BVH::Node& node) const
{
    RT_UNUSED(objectID);

    for (Uint32 i = 0; i < node.numLeaves; ++i)
    {
        const Uint32 objectIndex = node.childIndex + i;
        const ISceneObject* object = mObjects[objectIndex].get();

        const float time = 0.0f; // TODO
        const auto invTransform = object->GetInverseTransform(time);

        // transform ray to local-space
        math::Ray_Simd8 transformedRay = context.ray;
        transformedRay.origin = invTransform.TransformPoint(context.ray.origin);
        transformedRay.dir = invTransform.TransformVector(context.ray.dir);
        transformedRay.invDir = Vector3x8::FastReciprocal(transformedRay.dir);

        // TODO remove
        //const Vector8 previousDistance = outHitPoint.distance;


        SimdTraversalContext objectContext =
        {
            transformedRay,
            context.hitPoint,
            context.context
        };

        object->Traverse_Simd8(context, objectIndex);

        // TODO remove
        //const __m256 compareMask = _mm256_cmp_ps(outHitPoint.distance, previousDistance, _CMP_NEQ_OQ);
        //outHitPoint.objectId = _mm256_blendv_ps(outHitPoint.objectId, Vector8(objectIndex), compareMask);
    }
}

void Scene::Traverse_Leaf_Packet(const PacketTraversalContext& context, const Uint32 objectID, const BVH::Node& node, Uint32 numActiveGroups) const
{
    (void)numActiveGroups;
    (void)node;
    (void)context;
    (void)objectID;
    // TODO
}

void Scene::Traverse_Single(const SingleTraversalContext& context) const
{
    size_t numObjects = mObjects.size();

    if (numObjects == 0) // scene is empty
    {
        return;
    }
    else if (numObjects == 1) // bypass BVH
    {
        // TODO transform ray

        mObjects.front()->Traverse_Single(context, 0);
    }
    else // full BVH traversal
    {
        GenericTraverse_Single(context, 0, this);
    }
}

void Scene::Traverse_Packet(const PacketTraversalContext& context) const
{
    size_t numObjects = mObjects.size();

    // clear hit-points
    // TODO temporary - distances should be written to RayGroups
    const Uint32 numRayGroups = context.ray.GetNumGroups();
    for (Uint32 i = 0; i < numRayGroups; ++i)
    {
        context.hitPoint[i].distance = VECTOR8_MAX;
        context.hitPoint[i].objectId = VectorInt8(UINT32_MAX);
    }

    if (numObjects == 0) // scene is empty
    {
        return;
    }
    else if (numObjects == 1) // bypass BVH
    {
        // TODO transform ray

        mObjects.front()->Traverse_Packet(context, 0);
    }
    else // full BVH traversal
    {
        GenericTraverse_Packet(context, 0, this);
    }
}

void Scene::ExtractShadingData(const math::Vector4& rayOrigin, const math::Vector4& rayDir, const HitPoint& hitPoint, ShadingData& outShadingData) const
{
    const float time = 0.0f;

    outShadingData.position = Vector4::MulAndAdd(rayDir, Vector4(hitPoint.distance), rayOrigin);

    // calculate normal, tangent, tex coord, etc. from intersection data
    const ISceneObject* object = mObjects[hitPoint.objectId].get();
    const auto invTransform = object->GetInverseTransform(time); // HACK
    object->EvaluateShadingData_Single(invTransform, hitPoint, outShadingData);

    // transform shading data from local space to world space
    const auto transform = object->GetTransform(time);
    outShadingData.tangent = transform.TransformVector(outShadingData.tangent);
    outShadingData.bitangent = transform.TransformVector(outShadingData.bitangent);
    outShadingData.normal = transform.TransformVector(outShadingData.normal);
}

RayColor Scene::TraceRay_Single(const Ray& ray, RenderingContext& context) const
{
    const bool regularRenderingMode = context.params->renderingMode == RenderingMode::Regular;

    HitPoint hitPoint;
    Ray currentRay = ray;

    RayColor resultColor;
    RayColor throughput = RayColor::One();

    for (Uint32 depth = 0; depth < context.params->maxRayDepth; ++depth)
    {
        hitPoint.distance = FLT_MAX;
        context.localCounters.Reset();
        Traverse_Single({ ray, hitPoint, context });
        context.counters.Append(context.localCounters);

        // ray missed - return background color
        if (hitPoint.distance == FLT_MAX)
        {
            if (regularRenderingMode)
            {
                resultColor += throughput * GetBackgroundColor(currentRay);
            }
            else
            {
                resultColor = Vector4(0.75f, 0.75f, 0.75f, 0.0f);
            }
            break;
        }

        ShadingData shadingData;
        ExtractShadingData(currentRay.origin, currentRay.dir, hitPoint, shadingData);

        if (!regularRenderingMode)
        {
            resultColor = HandleSpecialRenderingMode(context, hitPoint, shadingData);
            break;
        }

        // accumulate emission color
        resultColor += throughput * shadingData.material->emissionColor;

        // Russian roulette algorithm
        if (depth >= context.params->minRussianRouletteDepth)
        {
            const float threshold = throughput.values.HorizontalMax()[0];
            if (context.randomGenerator.GetFloat() > threshold)
                break;

            throughput *= 1.0f / Max(FLT_EPSILON, threshold);
        }

        const Vector4 outgoingDirWorldSpace = -currentRay.dir;
        Vector4 incomingDirWorldSpace;
        throughput *= shadingData.material->Shade(outgoingDirWorldSpace, incomingDirWorldSpace, shadingData, context.randomGenerator);

        // ray is not visible anymore
        if (throughput.values.IsZero())
        {
            break;
        }

        // generate secondary ray
        currentRay = math::Ray(shadingData.position, incomingDirWorldSpace);
    }

    return resultColor;
}

void Scene::TraceRay_Simd8(const math::Ray_Simd8& simdRay, RenderingContext& context, RayColor* outColors) const
{
    HitPoint_Simd8 hitPoints;

    context.localCounters.Reset();

    const SimdTraversalContext traversalContext =
    {
        simdRay,
        hitPoints,
        context
    };

    const size_t numObjects = mObjects.size();
    if (numObjects == 1) // bypass BVH
    {
        // TODO transform ray
        mObjects.front()->Traverse_Simd8(traversalContext, 0);
    }
    else if (numObjects > 1) // full BVH traversal
    {
        GenericTraverse_Simd8(traversalContext, 0, this);
    }

    context.counters.Append(context.localCounters);

    Shade_Simd8(simdRay, hitPoints, context, outColors);
}

void Scene::Shade_Simd8(const math::Ray_Simd8& ray, const HitPoint_Simd8& hitPoints, RenderingContext& context, RayColor* outColors) const
{
    // TODO if all rays hit the same triangle, use the SIMD version

    const bool regularRenderingMode = context.params->renderingMode == RenderingMode::Regular;

    Vector4 rayOrigins[8];
    Vector4 rayDirs[8];

    ray.origin.Unpack(rayOrigins);
    ray.dir.Unpack(rayDirs);

    ShadingData shadingData;

    for (Uint32 i = 0; i < 8; ++i)
    {
        RayColor resultColor;

        // ray missed - return background color
        if (hitPoints.objectId.u[i] == UINT32_MAX)
        {
            const Vector4 backgroundColor = regularRenderingMode ? mEnvironment.backgroundColor : Vector4(0.75f, 0.75f, 0.75f, 0.0f);
            //resultColor += throughput * mEnvironment.backgroundColor;
            resultColor = backgroundColor;
        }
        else
        {
            const HitPoint hitPoint = hitPoints.Get(i);
            ExtractShadingData(rayOrigins[i], rayDirs[i], hitPoint, shadingData);

            if (!regularRenderingMode)
            {
                resultColor = HandleSpecialRenderingMode(context, hitPoint, shadingData);
            }
        }

        // TODO push secondary rays to output stream

        outColors[i] = resultColor;
    }
}

void Scene::Shade_Packet(const RayPacket& packet, const HitPoint_Packet& hitPoints, RenderingContext& context, Bitmap& renderTarget) const
{
    const bool regularRenderingMode = context.params->renderingMode == RenderingMode::Regular;

    ShadingData shadingData;

    const Uint32 numGroups = packet.GetNumGroups();
    for (Uint32 i = 0; i < numGroups; ++i)
    {
        const HitPoint_Simd8& hitPoint = hitPoints[i];
        const Ray_Simd8& ray = packet.groups[i].rays;

        RayColor colors[8];
        Shade_Simd8(ray, hitPoint, context, colors);

        // TODO push secondary rays to output stream

        Vector4 weights[8];
        packet.weights[i].Unpack(weights);

        for (Uint32 j = 0; j < RayPacket::RaysPerGroup; ++j)
        {
            const ImageLocationInfo& imageLocation = packet.imageLocations[RayPacket::RaysPerGroup * i + j];
            const Vector4 color = weights[j] * colors[j].values;

            // TODO this should be performed using dedicated Bitmap method in one batch
            renderTarget.AccumulateFloat_Unsafe(imageLocation.x, imageLocation.y, color);
        }
    }
}


RayColor Scene::GetBackgroundColor(const math::Ray& ray) const
{
    RayColor color = mEnvironment.backgroundColor;

    // sample environment map
    if (mEnvironment.texture)
    {
        const Float theta = ACos(ray.dir[1]);
        const Float phi = FastATan2(ray.dir[2], ray.dir[0]);
        const Vector4 coords(phi / (2.0f * RT_PI) + 0.5f, theta / RT_PI, 0.0f, 0.0f);

        SamplerDesc sampler;
        color *= mEnvironment.texture->Sample(coords, sampler);
    }

    return color;
}

} // namespace rt