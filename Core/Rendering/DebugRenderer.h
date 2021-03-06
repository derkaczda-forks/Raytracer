#pragma once

#include "Renderer.h"

namespace rt {

enum class DebugRenderingMode : Uint8
{
    CameraLight = 0,            // simple shading based on normal vector orientation

    // geometry
    TriangleID,                 // draw every triangle with random color
    Depth,                      // visualize depth
    Position,                   // visualize world-space position
    Normals,                    // visualize normal vectors (in world space)
    Tangents,
    Bitangents,
    TexCoords,   

    // material
    BaseColor,                  // visualize base color of the first intersection
    Emission,                   // visualize emission color
    Roughness,                  // visualize "rougness" parameter
    Metalness,                  // visualize "metalness" parameter

#ifdef RT_ENABLE_INTERSECTION_COUNTERS
    // stats
    RayBoxIntersection,         // visualize number of performed ray-box intersections
    RayBoxIntersectionPassed,   // visualize number of passed ray-box intersections
    RayTriIntersection,         // visualize number of performed ray-triangle intersections
    RayTriIntersectionPassed,   // visualize number of passed ray-triangle intersections
#endif // RT_ENABLE_INTERSECTION_COUNTERS
};


// Debug renderer for visualizing normals, tangents, base color, etc.
class DebugRenderer : public IRenderer
{
public:
    DebugRenderer(const Scene& scene);

    virtual const char* GetName() const;
    virtual const RayColor RenderPixel(const math::Ray& ray, const RenderParam& param, RenderingContext& ctx) const override;
    virtual void Raytrace_Packet(RayPacket& packet, const Camera& camera, Film& film, RenderingContext& context) const override;

    DebugRenderingMode mRenderingMode;
};

} // namespace rt
