#ifndef MESH_MANAGER_LEGACY_INTERNAL_H
#define MESH_MANAGER_LEGACY_INTERNAL_H

namespace mbm
{
    namespace legacy_mesh_internal
    {
        extern thread_local bool g_skipLegacyDispatchMesh;
        extern thread_local bool g_skipLegacyDispatchMeshDebug;
    }
}

#endif
