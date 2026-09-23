/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2004-2017 by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
|                                                                                                                        |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation        |
| the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and       |
| to permit persons to whom the Software is furnished to do so, subject to the following conditions:                     |
|                                                                                                                        |
| The above copyright notice and this permission notice shall be included in all copies or substantial portions of       |
| the Software.                                                                                                          |
|                                                                                                                        |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE   |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR  |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR       |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|                                                                                                                        |
|-----------------------------------------------------------------------------------------------------------------------*/

#ifndef IMAGE_MESH_H
#define IMAGE_MESH_H

#include <core_mbm/core-exports.h>
#include <cstdint>

namespace mbm
{
    class MESH_MBM_DEBUG;

    enum class IMAGE_MESH_SHAPE { RECTANGLE, ELLIPSE, POLYGON };
    struct IMAGE_MESH_POINT { float x = 0, y = 0; };

    enum class IMAGE_MESH_SIDE { EDGE, COLOR, REPEAT, BAND };
    enum class IMAGE_MESH_BRUSH { RAISE, LOWER, FLATTEN, SMOOTH };
    struct IMAGE_MESH_DAB
    {
        float x = 0.5f, y = 0.5f, radius = 0.1f, strength = 0.1f, height = 0.5f;
        IMAGE_MESH_BRUSH mode = IMAGE_MESH_BRUSH::RAISE;
    };

    // Value-only request/result types. Image and mesh buffers remain implementation-owned.
    struct IMAGE_MESH_HOLE
    {
        const IMAGE_MESH_POINT *points = nullptr;
        uint32_t count = 0;
    };

    enum class IMAGE_MESH_HEIGHT_CHANNEL { LUMINANCE, RED, GREEN, BLUE, ALPHA };
    enum class IMAGE_MESH_HEIGHT_SOURCE { IMAGE, MANUAL, MIXED, CURVED };
    struct IMAGE_MESH_HEIGHT_AREA
    {
        const IMAGE_MESH_POINT *points = nullptr;
        uint32_t count = 0;
        float height = 0.75f, transition = 0.02f;
        bool enabled = true;
        bool line = false; // Open centerline with round caps/joins, at least two points.
        float lineWidth = 0.05f; // Full width relative to the shorter crop side.
    };

    enum class IMAGE_MESH_CURVED_PROFILE { LINEAR, SMOOTH, BEZIER };

    // Borrowed value-only hierarchy. Parent 0 is the module contour; other parents
    // refer to an earlier node (1-based). A point/segment is terminal.
    struct IMAGE_MESH_CURVED_NODE
    {
        const IMAGE_MESH_POINT *points = nullptr;
        uint32_t count = 0, parent = 0;
        float thickness = 8.0f;
        IMAGE_MESH_CURVED_PROFILE profile = IMAGE_MESH_CURVED_PROFILE::LINEAR;
        // Internal Bezier controls, evenly spaced in X; each Y is in [0,1].
        uint32_t bezierPoints = 2; // 2, 3 or 4 internal controls; endpoints stay fixed.
        float bezier1 = 0.0f, bezier2 = 1.0f, bezier3 = 1.0f, bezier4 = 1.0f;
        bool inherited = false; // Local region; otherwise the owner's sole target.
    };

    struct IMAGE_MESH_OPTIONS
    {
        uint32_t x = 0, y = 0, cropWidth = 0, cropHeight = 0;
        IMAGE_MESH_SHAPE shape = IMAGE_MESH_SHAPE::RECTANGLE;
        uint32_t ellipseSegments = 48;
        // Borrowed normalized crop coordinates, only read during this call (3..128 points).
        const IMAGE_MESH_POINT *contour = nullptr;
        uint32_t contourCount = 0;
        // Borrowed simple disjoint contours, strictly inside the outer shape. Max 16 x 128 points.
        const IMAGE_MESH_HOLE *holes = nullptr;
        uint32_t holeCount = 0;
        uint32_t columns = 32, rows = 32;
        uint32_t maxVertices = 65535, maxTriangles = 131070;
        float width = 100.0f, height = 100.0f, depth = 20.0f, relief = 8.0f;
        float borderWidth = 0.1f;
        bool invert = false;
        bool lockBorder = true;
        // Copy the final front relief outward on the back. UV mirroring is independent.
        bool backRelief = false, backMirror = false;
        bool backOpen = false, backRemap = false, backSolid = false, backExternal = false;
        const char *backTexture = nullptr; // borrowed, read only during generation
        uint32_t backColor = 0x808080;
        // Independent same-image UV rectangle; zero sizes use the front crop dimensions.
        uint32_t backX = 0, backY = 0, backCropWidth = 0, backCropHeight = 0;
        IMAGE_MESH_SIDE sideMode = IMAGE_MESH_SIDE::EDGE;
        bool sideBandPerpendicular = false; // Translate UVs perpendicular to each edge (BAND only).
        bool sideBandInvert = false; // Swap inner/outer UV endpoints across side depth (BAND only).
        float sideInset = 1.0f, sideRepeatU = 1.0f, sideRepeatV = 1.0f;
        uint32_t sideColor = 0x808080; // opaque RGB
        const char *sideTexture = nullptr; // borrowed, read only during generation
        bool followImage = false, twoLevels = false;
        float grooveThreshold = 0.5f, grooveTransition = 0.1f, heightTolerance = 0.03f;
        uint32_t smoothPasses = 0;
        // Borrowed ordered dabs (max 4096). XY normalized to crop; radius relative to its shorter side.
        const IMAGE_MESH_DAB *heightEdits = nullptr;
        uint32_t heightEditCount = 0;
        IMAGE_MESH_HEIGHT_SOURCE heightSource = IMAGE_MESH_HEIGHT_SOURCE::IMAGE;
        float baseHeight = 0.5f;
        // CURVED: total thickness, movable normalized center, radius in mesh units.
        float curvedX = 0.5f, curvedY = 0.5f, curvedRadius = 0.0f;
        float curvedEdge = 1.0f, curvedTarget = 8.0f;
        bool curvedSymmetric = true;
        bool curvedHierarchy = false; // false retains the original radial point/circle path
        const IMAGE_MESH_CURVED_NODE *curvedNodes = nullptr;
        uint32_t curvedNodeCount = 0; // at most 32 nodes, 128 points each, depth <= 8
        IMAGE_MESH_HEIGHT_CHANNEL heightChannel = IMAGE_MESH_HEIGHT_CHANNEL::LUMINANCE;
        // Borrowed optional height-only image. Empty uses the source texture.
        const char *heightImage = nullptr;
        // false: map the whole source image; true: fit the whole map to this crop.
        bool heightImageToRegion = false;
        // Normalize/clamp black..white, then pow(value, heightCurve), before inversion.
        float heightBlack = 0.0f, heightWhite = 1.0f, heightCurve = 1.0f;
        // Ordered borrowed normalized contours, max 32 x 128. Transition is
        // relative to the shorter crop side, fades inward from each area edge.
        const IMAGE_MESH_HEIGHT_AREA *heightAreas = nullptr;
        uint32_t heightAreaCount = 0;
        // Optional CPU progress/cancellation hook, called on the generating thread.
        // Return false to cancel. Context must outlive the call; do not call Lua/GPU here.
        bool (*progress)(void *context, const char *stage, float fraction) = nullptr;
        void *progressContext = nullptr;
    };

    struct IMAGE_MESH_REPORT
    {
        uint32_t vertices = 0, triangles = 0;
        float minHeight = 0.0f, maxHeight = 0.0f;
    };

    // CPU-only extrusion of a rectangle, ellipse or simple polygon. Destination must have no frames.
    // Crop uses zero-based top-left pixels; zero crop dimensions mean remaining image.
    // CURVED uses total endpoint thickness and ignores legacy height/back controls.
    // Front faces -Z; relief extends outward. Back is optional, at +depth/2 plus copied relief when enabled. Texture references
    // the resolved source image (not copied). On failure discard the destination.
    API_IMPL bool generateImageMesh(const char *imagePath, const IMAGE_MESH_OPTIONS &options,
                                    MESH_MBM_DEBUG &destination, IMAGE_MESH_REPORT &report,
                                    char *errorOut, int errorOutLen);
    // CPU contour query. Capacity >=128 (>=256 for perpendicular endpoint pairs).
    // Crop dimensions are required. Perpendicular output contains two endpoints per edge.
    // Maximum inset is returned even when the requested inset is out of range.
    API_IMPL bool getImageMeshSideContour(const IMAGE_MESH_OPTIONS &options, IMAGE_MESH_POINT *points,
                                          uint32_t &count, float &maximumInset,
                                          char *errorOut, int errorOutLen);
    // Writes a cropped RGBA PNG: processed height or blue groove overlay; no mesh/GPU allocation.
    // Export the UV bounding rectangle with replicated padding; transform = scaleU,scaleV,offsetU,offsetV.
    API_IMPL bool exportImageMeshTexture(const char *source, const char *output, const float *bounds,
                                         uint32_t padding, float *transform, char *errorOut, int errorOutLen);
    API_IMPL bool generateImageMeshMap(const char *imagePath, const IMAGE_MESH_OPTIONS &options,
                                       const char *outputPath, bool overlay,
                                       char *errorOut, int errorOutLen);
}

#endif
