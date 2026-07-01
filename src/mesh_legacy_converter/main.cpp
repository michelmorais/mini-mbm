// Standalone CLI tool for mesh_deprecated::convertLegacyMeshToV11 (docs/mesh-v11-plan.md Phase B2).
// Offline-only migration tool - never linked into the game runtime.
#include "mesh-deprecated.h"
#include <cstdio>

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        fprintf(stderr, "usage: %s <legacy-mesh-in> <v11-mesh-out>\n", argc > 0 ? argv[0] : "mesh_legacy_converter");
        fprintf(stderr, "Converts a legacy v8-v10 mesh file (static, non-animated, path-referenced "
                        "textures only) into a v11 mesh file.\n");
        return 1;
    }

    const char *legacyPath = argv[1];
    const char *v11OutPath = argv[2];

    char errorOut[1024] = {0};
    if (!mesh_deprecated::convertLegacyMeshToV11(legacyPath, v11OutPath, errorOut, sizeof(errorOut) - 1))
    {
        fprintf(stderr, "failed to convert [%s] -> [%s]\n%s\n", legacyPath, v11OutPath, errorOut);
        return 1;
    }

    printf("converted [%s] -> [%s]\n", legacyPath, v11OutPath);
    return 0;
}
