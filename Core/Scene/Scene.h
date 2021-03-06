#pragma once

#include "../RayLib.h"

#include "../Color/RayColor.h"
#include "../Traversal/HitPoint.h"
#include "../BVH/BVH.h"
#include "../Containers/DynArray.h"

namespace rt {

class ISceneObject;
class ILight;
class Bitmap;
class Camera;
struct RenderingContext;
struct HitPoint;
struct ShadingData;
struct SingleTraversalContext;
struct PacketTraversalContext;

using SceneObjectPtr = std::unique_ptr<ISceneObject>;
using LightPtr = std::unique_ptr<ILight>;

namespace math {
class Ray;
class Ray_Simd4;
class Ray_Simd8;
} // namespace math

/**
 * Rendering scene.
 * Allows for placing objects (meshes, lights, etc.) and raytracing them.
 */
class RT_ALIGN(16) Scene : public Aligned<16>
{
public:
    RAYLIB_API Scene();
    RAYLIB_API ~Scene();
    RAYLIB_API Scene(Scene&&);
    RAYLIB_API Scene& operator = (Scene&&);

    RAYLIB_API void AddLight(LightPtr object);
    RAYLIB_API void AddObject(SceneObjectPtr object);

    RAYLIB_API bool BuildBVH();

    RT_FORCE_INLINE const BVH& GetBVH() const { return mBVH; }
    RT_FORCE_INLINE const DynArray<SceneObjectPtr>& GetObjects() const { return mObjects; }
    RT_FORCE_INLINE const DynArray<LightPtr>& GetLights() const { return mLights; }
    RT_FORCE_INLINE const DynArray<const ILight*>& GetGlobalLights() const { return mGlobalLights; }

    // traverse the scene, returns hit points
    RAYLIB_API void Traverse_Single(const SingleTraversalContext& context) const;
    void Traverse_Packet(const PacketTraversalContext& context) const;

    // cast shadow ray
    bool Traverse_Shadow_Single(const SingleTraversalContext& context) const;

    RAYLIB_API void ExtractShadingData(const math::Ray& ray, const HitPoint& hitPoint, const float time, ShadingData& outShadingData) const;

    void TraceRay_Simd8(const math::Ray_Simd8& ray, RenderingContext& context, RayColor* outColors) const;

    void Traverse_Leaf_Single(const SingleTraversalContext& context, const Uint32 objectID, const BVH::Node& node) const;
    void Traverse_Leaf_Packet(const PacketTraversalContext& context, const Uint32 objectID, const BVH::Node& node, Uint32 numActiveGroups) const;

    bool Traverse_Leaf_Shadow_Single(const SingleTraversalContext& context, const BVH::Node& node) const;

    RAYLIB_API const ILight& GetLightByObjectId(Uint32 id) const;

private:
    Scene(const Scene&) = delete;
    Scene& operator = (const Scene&) = delete;

    RT_FORCE_NOINLINE void Traverse_Object_Single(const SingleTraversalContext& context, const Uint32 objectID) const;
    RT_FORCE_NOINLINE bool Traverse_Object_Shadow_Single(const SingleTraversalContext& context, const Uint32 objectID) const;

    DynArray<LightPtr> mLights;
    DynArray<const ILight*> mGlobalLights;

    DynArray<SceneObjectPtr> mObjects;

    // bounding volume hierarchy for scene object
    BVH mBVH;
};

} // namespace rt
