#pragma once

#include "SceneObject.h"

namespace rt {

class RAYLIB_API BoxSceneObject : public ISceneObject
{
public:
    BoxSceneObject(const math::Vector4& size, const Material* material);

private:
    virtual math::Box GetBoundingBox() const override;

    virtual void Traverse_Single(const SingleTraversalContext& context, const Uint32 objectID) const override;
    virtual void Traverse_Simd8(const SimdTraversalContext& context, const Uint32 objectID) const override;
    virtual void Traverse_Packet(const PacketTraversalContext& context, const Uint32 objectID) const override;

    virtual void EvaluateShadingData_Single(const math::Matrix& worldToLocal, const HitPoint& intersechitPointtionData, ShadingData& outShadingData) const override;

    const Material* mMaterial;

    // half size
    math::Vector4 mSize;
};

} // namespace rt