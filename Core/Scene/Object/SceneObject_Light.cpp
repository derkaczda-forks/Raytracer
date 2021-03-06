#include "PCH.h"
#include "SceneObject_Light.h"
#include "../Light/Light.h"
#include "Traversal/TraversalContext.h"

namespace rt {

using namespace math;

LightSceneObject::LightSceneObject(const ILight& light)
    : mLight(light)
{ }

Box LightSceneObject::GetBoundingBox() const
{
    return mLight.GetBoundingBox();
}

void LightSceneObject::Traverse_Single(const SingleTraversalContext& context, const Uint32 objectID) const
{
    float lightDistance;
    if (mLight.TestRayHit(context.ray, lightDistance))
    {
        if (lightDistance < context.hitPoint.distance)
        {
            // mark as light
            context.hitPoint.Set(lightDistance, objectID, RT_LIGHT_OBJECT);
        }
    }
}

bool LightSceneObject::Traverse_Shadow_Single(const SingleTraversalContext& context) const
{
    float lightDistance;
    if (mLight.TestRayHit(context.ray, lightDistance))
    {
        if (lightDistance < context.hitPoint.distance)
        {
            context.hitPoint.distance = lightDistance;
            return true;
        }
    }

    return false;
}

void LightSceneObject::Traverse_Packet(const PacketTraversalContext& context, const Uint32 objectID, const Uint32 numActiveGroups) const
{
    RT_UNUSED(context);
    RT_UNUSED(objectID);
    RT_UNUSED(numActiveGroups);
    // TODO
}

void LightSceneObject::EvaluateShadingData_Single(const HitPoint&, ShadingData&) const
{
    RT_FATAL("Light surface cannot be shaded");
}


} // namespace rt
