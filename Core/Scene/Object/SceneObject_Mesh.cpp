#include "PCH.h"
#include "SceneObject_Mesh.h"
#include "Mesh/Mesh.h"
#include "Traversal/Traversal_Single.h"
#include "Traversal/Traversal_Packet.h"

namespace rt {

using namespace math;

MeshSceneObject::MeshSceneObject(const MeshPtr mesh)
    : mMesh(mesh)
{ }

Box MeshSceneObject::GetBoundingBox() const
{
    const Box localBox = mMesh->GetBoundingBox();

    // TODO just transformed box may be bigger that bounding box of rotated triangles
    const Box box0 = ComputeTransform(0.0f).TransformBox(localBox);
    const Box box1 = ComputeTransform(1.0f).TransformBox(localBox);

    return Box(box0, box1);
}

void MeshSceneObject::Traverse_Single(const SingleTraversalContext& context, const Uint32 objectID) const
{
    GenericTraverse_Single<Mesh>(context, objectID, mMesh.get());
}

bool MeshSceneObject::Traverse_Shadow_Single(const SingleTraversalContext& context) const
{
    return GenericTraverse_Shadow_Single<Mesh>(context, mMesh.get());
}

void MeshSceneObject::Traverse_Packet(const PacketTraversalContext& context, const Uint32 objectID, const Uint32 numActiveGroups) const
{
    GenericTraverse_Packet<Mesh, 1>(context, objectID, mMesh.get(), numActiveGroups);
}

void MeshSceneObject::EvaluateShadingData_Single(const HitPoint& hitPoint, ShadingData& outShadingData) const
{
    mMesh->EvaluateShadingData_Single(hitPoint, outShadingData, GetDefaultMaterial());
}


} // namespace rt
