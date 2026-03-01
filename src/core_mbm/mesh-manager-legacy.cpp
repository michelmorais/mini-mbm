#include <mesh-manager.h>
#include "mesh-manager-legacy-internal.h"

namespace mbm
{
    namespace legacy_mesh_internal
    {
        thread_local bool g_skipLegacyDispatchMesh = false;
        thread_local bool g_skipLegacyDispatchMeshDebug = false;
    }

    bool MESH_MBM_DEBUG::loadDebugLegacyCompat(const char *fileNamePath)
    {
#if defined(MBM_ENABLE_MESH_LEGACY_V7)
        legacy_mesh_internal::g_skipLegacyDispatchMeshDebug = true;
        const bool ret = this->loadDebug(fileNamePath);
        legacy_mesh_internal::g_skipLegacyDispatchMeshDebug = false;
        return ret;
#else
        (void)fileNamePath;
        return false;
#endif
    }

    bool MESH_MBM::loadLegacyCompat(const char *fileNamePath)
    {
#if defined(MBM_ENABLE_MESH_LEGACY_V7)
        legacy_mesh_internal::g_skipLegacyDispatchMesh = true;
        const bool ret = this->load(fileNamePath);
        legacy_mesh_internal::g_skipLegacyDispatchMesh = false;
        return ret;
#else
        (void)fileNamePath;
        return false;
#endif
    }
}
