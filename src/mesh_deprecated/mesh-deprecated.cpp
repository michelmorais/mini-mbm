#include "mesh-deprecated.h"

#include <cstdio>
#include <cstring>

namespace mesh_deprecated
{
    bool convertLegacyMeshToV11(const char *legacyPath, const char *v11OutPath, char *errorOut, int lenErrorOut)
    {
        (void)legacyPath;
        (void)v11OutPath;
        const char *message = "convertLegacyMeshToV11 is not implemented yet (milestone 5, Phase B2)";
        if (errorOut && lenErrorOut > 0)
            snprintf(errorOut, static_cast<size_t>(lenErrorOut), "%s", message);
        return false;
    }
}
