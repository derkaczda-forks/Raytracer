#pragma once

#include "RayPacket.h"
#include "../Math/VectorInt8.h"

#include <float.h>

constexpr Uint32 RT_INVALID_OBJECT = UINT32_MAX;
constexpr Uint32 RT_LIGHT_OBJECT = 0xFFFFFFFE;

namespace rt {

// Ray-scene intersection data (non-SIMD)
struct HitPoint
{
    union
    {
        struct
        {
            Uint32 objectId;
            Uint32 subObjectId;
        };

        Uint64 combinedObjectId;
    };

    float distance;
    float u;
    float v;

    RT_FORCE_INLINE HitPoint()
        : objectId(RT_INVALID_OBJECT)
        , distance(FLT_MAX)
    {}

    RT_FORCE_INLINE void Set(float newDistance, Uint32 newObjectId, Uint32 newSubObjectId)
    {
        distance = newDistance;
        objectId = newObjectId;
        subObjectId = newSubObjectId;
    }

    // optimization: perform single 64-bit write instead of two 32-bit writes
    RT_FORCE_INLINE void Set(float newDistance, Uint32 newObjectId)
    {
        distance = newDistance;
        combinedObjectId = newObjectId;
    }
};

// Ray-scene intersection data (SIMD-8)
struct RT_ALIGN(32) HitPoint_Simd8
{
    math::Vector8 distance;
    math::Vector8 u;
    math::Vector8 v;
    math::VectorInt8 objectId;
    math::VectorInt8 subObjectId;

    RT_FORCE_INLINE HitPoint_Simd8()
        : distance(math::VECTOR8_MAX)
        , objectId(RT_INVALID_OBJECT)
    {}

    // extract single hit point
    RT_FORCE_INLINE HitPoint Get(Uint32 i) const
    {
        RT_ASSERT(i < 8);

        HitPoint result;
        result.distance = distance[i];
        result.u = u[i];
        result.v = v[i];
        result.objectId = objectId[i];
        result.subObjectId = subObjectId[i];
        return result;
    }
};

} // namespace rt
