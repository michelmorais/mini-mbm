#ifndef MESH_DEPRECATED_H
#define MESH_DEPRECATED_H

namespace mesh_deprecated
{
    // Converts a legacy v1-v10 mesh file into a v11 file by parsing the old bytes into a
    // MESH_MBM_DEBUG via its public mutation API, then calling MESH_MBM_DEBUG::saveV11. Offline-only
    // migration tool (docs/mesh-v11-plan.md Scope Decision 1) - never linked into the game runtime.
    //
    // Not implemented yet (milestone 5, Phase B2) - currently always fails with a clear message.
    bool convertLegacyMeshToV11(const char *legacyPath, const char *v11OutPath, char *errorOut, int lenErrorOut);
}

#endif
