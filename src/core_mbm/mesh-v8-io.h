#ifndef MESH_V8_IO_H
#define MESH_V8_IO_H

#include <cstdio>
#include <header-mesh.h>
#include <shapes.h>

// v11's SECTION_DETAIL_PHYSICS keeps the v8 on-disk field layout verbatim (docs/mesh-v11-plan.md
// File Format Design Principles), so saveV11/loadV11 reuse these primitives directly. Every other
// v8-v10 primitive (headers, frames, subsets, animation/shader, material-texture-slot,
// font/particle/tile/btile) moved to src/mesh_deprecated/mesh-v8-io-legacy.h/.cpp in milestone 5.
// readHeaderV8/readHeaderImgV8 (+ write counterparts) moved there too in milestone 5.5 - uber-image.cpp's
// UBER_IMG (an unrelated compressed-image-blob format, not a mesh format) no longer reuses them, it
// has its own self-contained header now (docs/mesh-v11-plan.md Scope Decision 5).
namespace util
{
    bool readDetailMeshV8(FILE *fp, util::DETAIL_MESH &out);
    bool writeDetailMeshV8(FILE *fp, const util::DETAIL_MESH &in);

    bool readVec2V8(FILE *fp, mbm::VEC2 &out);
    bool writeVec2V8(FILE *fp, const mbm::VEC2 &in);

    bool readVec3V8(FILE *fp, mbm::VEC3 &out);
    bool writeVec3V8(FILE *fp, const mbm::VEC3 &in);

    bool readVec3ArrayV8(FILE *fp, mbm::VEC3 *out, uint32_t count);
    bool writeVec3ArrayV8(FILE *fp, const mbm::VEC3 *in, uint32_t count);

    bool readCubeV8(FILE *fp, mbm::CUBE &out);
    bool writeCubeV8(FILE *fp, const mbm::CUBE &in);

    bool readSphereV8(FILE *fp, mbm::SPHERE &out);
    bool writeSphereV8(FILE *fp, const mbm::SPHERE &in);

    bool readCubeComplexV8(FILE *fp, mbm::CUBE_COMPLEX &out);
    bool writeCubeComplexV8(FILE *fp, const mbm::CUBE_COMPLEX &in);

    bool readTriangleV8(FILE *fp, mbm::TRIANGLE &out);
    bool writeTriangleV8(FILE *fp, const mbm::TRIANGLE &in);
    bool readTriangleLegacyNoPosV8(FILE *fp, mbm::TRIANGLE &out);
}

#endif
