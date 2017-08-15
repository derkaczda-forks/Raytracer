#pragma once

#include "RayLib.h"

namespace rt {

class Material;

enum class VertexDataFormat : Uint8
{
    None,
    Float,
    HalfFloat,
    Int32,
    Int16,
    Int8,
};

/**
 * Structure describing a vertex buffer.
 */
struct VertexBufferDesc
{
    Uint32 numVertices;
    Uint32 numTriangles;
    Uint32 numMaterials;

    const void* vertexIndexBuffer;
    const void* positions;
    const void* normals;
    const void* tangents;
    const void* texCoords;
    const void* materialIndexBuffer;
    const Material** materials;

    VertexDataFormat vertexIndexFormat;
    VertexDataFormat positionsFormat;
    VertexDataFormat normalsFormat;
    VertexDataFormat tangentsFormat;
    VertexDataFormat texCoordsFormat;
    VertexDataFormat materialIndexFormat;

    VertexBufferDesc::VertexBufferDesc()
        : numVertices(0)
        , numTriangles(0)
        , numMaterials(0)
        , vertexIndexBuffer(nullptr)
        , positions(nullptr)
        , normals(nullptr)
        , tangents(nullptr)
        , texCoords(nullptr)
        , materials(nullptr)
        , materialIndexBuffer(nullptr)
        , vertexIndexFormat(VertexDataFormat::None)
        , positionsFormat(VertexDataFormat::None)
        , normalsFormat(VertexDataFormat::None)
        , tangentsFormat(VertexDataFormat::None)
        , texCoordsFormat(VertexDataFormat::None)
        , materialIndexFormat(VertexDataFormat::None)
    { }
};

} // namespace rt
