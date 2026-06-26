#ifndef MESH_DEPRECATED_H
#define MESH_DEPRECATED_H

namespace mesh_deprecated
{
    // Converts a legacy v8-v10 mesh file into a v11 file by parsing the old bytes into a
    // MESH_MBM_DEBUG via its public mutation API, then calling MESH_MBM_DEBUG::saveV11. Offline-only
    // migration tool (docs/mesh-v11-plan.md Scope Decision 1) - never linked into the game runtime.
    //
    // Scoped to what saveV11 can persist: 3D/SPRITE/USER/TEXTURE/SHAPE meshes, v8-v10 only,
    // path-referenced subset textures only. Animations (frame ranges, and a shader-effect FX block
    // referencing a built-in/custom shader by name plus its variables) are imported. Cleanly rejects
    // (with a specific message) pre-v8 files, FONT/PARTICLE/TILE_MAP types, embedded/solid-color
    // textures, and a shader step's secondary texture stage (no real file has ever been observed
    // using it).
    bool convertLegacyMeshToV11(const char *legacyPath, const char *v11OutPath, char *errorOut, int lenErrorOut);
}

#endif
