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

#ifndef MESH_MANAGER_GLES_H
#define MESH_MANAGER_GLES_H

#include "core-exports.h"
#include "primitives.h"
#include "header-mesh.h"
#include "physics.h"
#include "texture-role.h"
#include "animation.h"
#include <map>
#include <memory>
#include <functional>

namespace util
{
    struct SUBSET;
}


namespace mbm
{
    class BUFFER_GL;
    class RENDERIZABLE;
    class RENDERIZABLE_TO_TARGET;
    class SHADER;
    class MESH_MBM;
    class ARTICULATED_ANIMATION_PLAYER;
    struct IMAGE_RESOURCE;
    // Defined in mesh-manager.cpp only - forward-declared here so MESH_MBM::finishLoadFromIntermediate
    // can be declared without exposing the type's layout in the public header, same PIMPL-style
    // pattern as MESH_MBM::Impl.
    struct MESH_LOAD_INTERMEDIATE_V11;

    struct BUFFER_MESH
    {
        BUFFER_GL *pBufferGL;
        util::SUBSET *  subset;
        uint32_t    totalSubset;
        constexpr BUFFER_MESH() noexcept;
        API_IMPL virtual ~BUFFER_MESH();
        API_IMPL void release();
        API_IMPL BUFFER_GL *getRenderBuffer() const noexcept;
        API_IMPL bool hasLoadedRenderBuffer() const noexcept;
        API_IMPL uint32_t getTotalSubsets() const noexcept;
        API_IMPL util::SUBSET *getSubset(const uint32_t indexSubset) const noexcept;
    };

    class ARTICULATED_ANIMATION_PLAYER
    {
        friend class MESH_MBM;
      public:
        API_IMPL ARTICULATED_ANIMATION_PLAYER();
        API_IMPL ~ARTICULATED_ANIMATION_PLAYER();
        API_IMPL void reset() noexcept;
        ARTICULATED_ANIMATION_PLAYER(const ARTICULATED_ANIMATION_PLAYER &) = delete;
        ARTICULATED_ANIMATION_PLAYER &operator=(const ARTICULATED_ANIMATION_PLAYER &) = delete;
      private:
        struct Impl;
        std::unique_ptr<Impl> impl;
    };

    class MESH_MBM_DEBUG
    {
      public:
        API_IMPL MESH_MBM_DEBUG();

        API_IMPL virtual ~MESH_MBM_DEBUG();
        API_IMPL uint32_t addBuffer(const int stride = 3);
        API_IMPL uint32_t addSubset(uint32_t indexFrame);
        API_IMPL void     removeSubset(uint32_t indexFrame, uint32_t indexSubset);
        API_IMPL uint32_t copyBufferFrom(MESH_MBM_DEBUG &src, uint32_t srcFrameIdx);
        API_IMPL uint32_t copySubsetFrom(uint32_t targetFrame, MESH_MBM_DEBUG &src, uint32_t srcFrame, uint32_t srcSubsetIdx);
        API_IMPL bool getInfo(util::HEADER_MESH &headerMeshMbmOut, util::TYPE_MESH &typeOut, INFO_BOUND_FONT **datailFontOut,
                     std::vector<util::STAGE_PARTICLE> &lsStageParticle);
        API_IMPL static bool getInfo(const char *fileNamePath, util::HEADER_MESH &headerMeshMbmOut,util::INFO_DRAW_MODE & info_mode,
                                  util::TYPE_MESH &typeOut, INFO_BOUND_FONT &datailFontOut,
                                  std::vector<util::STAGE_PARTICLE> & lsStageParticle, int *versionOut = nullptr,
                                  bool *hasSkeletonOut = nullptr, uint16_t *totalBonesOut = nullptr);
        API_IMPL static const char* getValidExtension(const char* fileName,bool &isImage,bool &isMesh,bool &isUnknown);
        API_IMPL static std::string getExtension(const char* fileName);
        API_IMPL util::TYPE_MESH getMeshType() const noexcept;
        API_IMPL void setMeshType(const util::TYPE_MESH type) noexcept;
        API_IMPL util::TYPE_MESH getType() noexcept;
        API_IMPL util::TYPE_MESH getType(const char *fileNamePath);
        API_IMPL INFO_PHYSICS &getPhysicsInfo() noexcept;
        API_IMPL const INFO_PHYSICS &getPhysicsInfo() const noexcept;
        API_IMPL int getFileVersion() const noexcept;
        API_IMPL util::MATERIAL &getMaterial() noexcept;
        API_IMPL const util::MATERIAL &getMaterial() const noexcept;
        API_IMPL int16_t getHasNormal() const noexcept;
        API_IMPL void setHasNormal(const int16_t hasNormalMode) noexcept;
        API_IMPL int16_t getHasTexture() const noexcept;
        API_IMPL void setHasTexture(const int16_t hasTextureMode) noexcept;
        API_IMPL const char *getFilenameMesh() const noexcept;
        API_IMPL unsigned int getModeDraw() const noexcept;
        API_IMPL void setModeDraw(const unsigned int modeDraw) noexcept;
        API_IMPL unsigned int getModeCullFace() const noexcept;
        API_IMPL void setModeCullFace(const unsigned int modeCullFace) noexcept;
        API_IMPL unsigned int getModeFrontFaceDirection() const noexcept;
        API_IMPL void setModeFrontFaceDirection(const unsigned int modeFrontFaceDirection) noexcept;
        API_IMPL void * getDetailInfo() const noexcept;
        API_IMPL void replaceDetailInfo(void *detailInfo) noexcept;
        API_IMPL uint32_t getTotalAnimationHeaders() const noexcept;
        API_IMPL util::INFO_ANIMATION::INFO_HEADER_ANIM *getAnimationHeader(const uint32_t index) const noexcept;
        API_IMPL void appendAnimationHeader(util::INFO_ANIMATION::INFO_HEADER_ANIM *infoHead) noexcept;
        API_IMPL void clearBlendOperations() noexcept;
        API_IMPL void resizeBlendOperations(const uint32_t totalAnimations);
        API_IMPL void setBlendOperation(const uint32_t index, const int blendOperation);
        API_IMPL uint32_t getTotalFrames() const noexcept;
        API_IMPL util::BUFFER_MESH_DEBUG *getFrameBuffer(const uint32_t indexFrame) const noexcept;
        API_IMPL uint32_t getTotalSubsets(const uint32_t indexFrame) const noexcept;
        API_IMPL util::SUBSET_DEBUG *getSubset(const uint32_t indexFrame, const uint32_t indexSubset) const noexcept;
        API_IMPL bool hasIndexBuffer(const uint32_t indexFrame) const noexcept;
        API_IMPL VEC3 *getPositionArray(const uint32_t indexFrame) const noexcept;
        API_IMPL VEC3 *getNormalArray(const uint32_t indexFrame) const noexcept;
        API_IMPL VEC2 *getUvArray(const uint32_t indexFrame) const noexcept;
        API_IMPL uint16_t *getIndexArray(const uint32_t indexFrame) const noexcept;
        API_IMPL void calculateNormals();
        API_IMPL void calculateUV();
        API_IMPL void removeNormals();
        API_IMPL void addNormals();
        API_IMPL void removeBuffer(uint32_t indexFrame);
        API_IMPL void removeAnimation(uint32_t index);
        // Writes the v11 section/TLV format (docs/mesh-v11-format.md): material+transform, frames,
        // physics bounding volumes, extra paths, animation headers, and (for FONT/PARTICLE/TILE_MAP
        // meshes) their own detail section. Returns false with a message in errorOut (not a partial
        // file) on failure.
        // `compress` (opt-in, default-off at every call site): when true, requests DEFLATE for
        // SECTION_FRAME_STATIC (the vertex/index buffer - the one section large enough for
        // compression to be worth it); every other section always stays uncompressed.
        API_IMPL bool saveV11(const char *fileOut, const bool recalculateNormal, const bool recalculateUV, const bool compress, char *errorOut,const int lenErrorOut);
        API_IMPL bool loadDebugFromMemory(const MESH_MBM* meshMemory);
        // Reads the v11 section/TLV format. This is the only mesh format core_mbm reads/writes -
        // v1-v10 support has been removed entirely.
        API_IMPL bool loadV11(const char *fileNamePath);
        API_IMPL bool check(char *error,const int lenError);
        // indexSubset selects the center anchor; the calculated translation applies to every
        // subset in each selected frame so their relative placement remains unchanged.
        API_IMPL void centralizeFrame(const int indexFrame, const int indexSubset);
        // Independently centralizes each selected subset. Unselected subsets are not moved.
        API_IMPL void centralizeFrameItself(const int indexFrame, const int indexSubset);
        API_IMPL void rotateFrame(const int indexFrame, const int indexSubset, const float angleX, const float angleY, const float angleZ);
        API_IMPL void scaleFrame(const int indexFrame, const int indexSubset, const float sx, const float sy, const float sz);
        API_IMPL void translateFrame(const int indexFrame, const int indexSubset, const float dx, const float dy, const float dz);
        API_IMPL bool addIndex(const uint32_t indexFrame, const uint32_t indexSubset,
                            const uint16_t *newIndexPart, const uint32_t sizeArrayNewIndexPart,
                            char *strErrorOut, const int strErrorOutLen);
        API_IMPL bool addVertex(const uint32_t indexFrame, const uint32_t indexSubset, const uint32_t totalVertex);
        API_IMPL int addAnimation(const char *nameAnimation, const int initialFrame, const int finalFrame,
                               const float timeBetweenFrame, const int typeAnimation, char *errorOut, const int errorOutLen);
        API_IMPL bool updateAnimation(const uint32_t index, const char *nameAnimation, const int initialFrame, const int finalFrame,
                               const float timeBetweenFrame, const int typeAnimation, char *errorOut,const int lenError);
        API_IMPL const util::INFO_ANIMATION::INFO_HEADER_ANIM *getAnim(const uint32_t index)const;
        API_IMPL const char *getAnimationEffectTexture(const uint32_t index) const noexcept;
        API_IMPL bool setAnimationEffectTexture(const uint32_t index, const char *fileName) noexcept;
        // Skeleton accessors (SECTION_FRAME_SKINNED, docs/mesh-v11-format.md Sec. 6e) - editor/
        // diagnostic round-trip only, never consulted by rendering. `parentName` must be nullptr/""
        // (root) or already-added via a prior addBone call in this instance; addBone returns 0 and
        // fills errorOut on any validation failure, else a 1-based joint index, mirroring
        // addAnimation's contract. rotX/Y/Z (Euler degrees, world/armature space, engine's own
        // X-then-Y-then-Z order) and scaleX/Y/Z (default 1,1,1) and length (default 0, meaning "no
        // orientation data") are the fields SECTION_FRAME_SKINNED's sectionVersion 2 added - see
        // SKELETON_BONE_V11's own comment in header-mesh.h.
        API_IMPL int addBone(const char *name, const char *parentName, const float x, const float y, const float z,
                              const float radius, const float rotX, const float rotY, const float rotZ,
                              const float scaleX, const float scaleY, const float scaleZ, const float length,
                              char *errorOut, const int errorOutLen);
        API_IMPL uint32_t getTotalBone() const noexcept;
        API_IMPL const util::SKELETON_BONE_V11 *getBone(const uint32_t index) const noexcept;
        // Edits an existing bone in place (name/parent/position/radius/rotation/scale/length).
        // Rejects an empty/duplicate name, an unknown parent, self-parenting, and any reparent that
        // would create a cycle (the candidate parent is a descendant of `index`). On success,
        // re-sorts the internal joint list so parent-before-child order still holds (required by
        // the on-disk format), which callers relying on stable indices across calls must account for.
        API_IMPL bool updateBone(const uint32_t index, const char *name, const char *parentName,
                                  const float x, const float y, const float z, const float radius,
                                  const float rotX, const float rotY, const float rotZ,
                                  const float scaleX, const float scaleY, const float scaleZ, const float length,
                                  char *errorOut, const int errorOutLen);
        // Removes bone `index`. If other bones reference it as their parent, the call fails (errorOut
        // explains how many) unless `cascadeChildren` is true, in which case the whole subtree rooted
        // at `index` is removed.
        API_IMPL bool removeBone(const uint32_t index, const bool cascadeChildren, char *errorOut, const int errorOutLen);
        // Vertex skin weight accessors (SECTION_VERTEX_SKIN_WEIGHTS, docs/mesh-v11-format.md Sec.
        // 6f) - editor/diagnostic + FBX re-export round-trip only, never consulted by rendering.
        // vertexIndex is 0-based, against frame 1's own vertex order (this section always describes
        // frame 1's topology, never any other frame's). Each of the 4 slots is independent: pass a
        // nullptr/empty boneNameN to leave that slot unused. Bone names are resolved against (or
        // added to) this instance's own weight palette - NOT SECTION_FRAME_SKINNED's bone list, so
        // this works even for a mesh with no SECTION_FRAME_SKINNED data at all. Growing the vertex
        // array itself only happens implicitly the first time any slot is set for a given
        // vertexIndex; setVertexWeight fails (returns false, fills errorOut) if vertexIndex is out
        // of range for frame 1's current vertex count.
        API_IMPL bool setVertexWeight(const uint32_t vertexIndex,
                                       const char *boneName0, const float weight0,
                                       const char *boneName1, const float weight1,
                                       const char *boneName2, const float weight2,
                                       const char *boneName3, const float weight3,
                                       char *errorOut, const int errorOutLen);
        // Returns false if vertexIndex is out of range or no weight data has been set for it yet.
        // On success, fills up to 4 (boneName, weight) out-pairs - boneNameN is set to nullptr (not
        // an empty string) for an unused slot, so a caller can tell "no 4th influence" apart from
        // "4th influence is an empty-named bone" (which addBone's own empty-name rejection makes
        // impossible anyway, but the distinction is kept for symmetry/clarity).
        API_IMPL bool getVertexWeight(const uint32_t vertexIndex,
                                       const char **boneName0, float *weight0,
                                       const char **boneName1, float *weight1,
                                       const char **boneName2, float *weight2,
                                       const char **boneName3, float *weight3) const noexcept;
        API_IMPL bool hasVertexWeights() const noexcept;
        API_IMPL uint32_t getTotalVertexWeightBones() const noexcept; // weight palette size (unique bones referenced)
        API_IMPL void removeVertexWeights() noexcept; // clears palette + all per-vertex weight data
        // Rigid/articulated animation authoring data. The storage remains PIMPL-owned; these
        // narrow operations are the editor-facing API and are also suitable for Lua bindings.
        API_IMPL uint32_t getTotalArticulatedParts() const noexcept;
        API_IMPL const util::ARTICULATED_PART_V11 *getArticulatedPart(const uint32_t index) const noexcept;
        API_IMPL uint32_t initializeArticulatedParts();
        // Removes all parts/pivots and tracks that reference those parts. Clips remain available.
        API_IMPL uint32_t removeArticulatedParts() noexcept;
        API_IMPL int addArticulatedPart(const uint64_t partId, const uint32_t frameIndex,
                                        const uint32_t subsetIndex, const char *name,
                                        const float pivotX, const float pivotY, const float pivotZ,
                                        const float pivotQX, const float pivotQY, const float pivotQZ, const float pivotQW,
                                        const uint64_t parentPartId, char *errorOut, const int errorOutLen);
        API_IMPL bool updateArticulatedPart(const uint32_t index, const char *name,
                                            const float pivotX, const float pivotY, const float pivotZ,
                                            const float pivotQX, const float pivotQY, const float pivotQZ, const float pivotQW,
                                            const uint64_t parentPartId, char *errorOut, const int errorOutLen);
        API_IMPL uint32_t getTotalArticulatedAnimations() const noexcept;
        API_IMPL const char *getArticulatedAnimationName(const uint32_t index) const noexcept;
        API_IMPL bool getArticulatedAnimation(const uint32_t index, const char **name, float *duration,
                                              float *speed, int *priority, bool *loop,
                                              uint8_t *blendMode) const noexcept;
        API_IMPL bool updateArticulatedAnimation(const uint32_t index, const char *name, const float duration,
                                                 const float speed, const int priority, const bool loop,
                                                 const uint8_t blendMode,
                                                 char *errorOut, const int errorOutLen);
        API_IMPL uint32_t getTotalArticulatedTracks(const uint32_t animationIndex) const noexcept;
        API_IMPL bool getArticulatedTrack(const uint32_t animationIndex, const uint32_t trackIndex,
                                          uint64_t *partId, uint8_t *channelMask, uint32_t *keyCount) const noexcept;
        API_IMPL bool getArticulatedKey(const uint32_t animationIndex, const uint32_t trackIndex,
                                        const uint32_t keyIndex, float *time,
                                        float *positionX, float *positionY, float *positionZ,
                                        float *rotationX, float *rotationY, float *rotationZ, float *rotationW,
                                        float *scaleX, float *scaleY, float *scaleZ,
                                        uint8_t *easing,
                                        float *bezierX1, float *bezierY1,
                                        float *bezierX2, float *bezierY2) const noexcept;
        API_IMPL int addArticulatedAnimation(const char *name, const float duration, const float speed,
                                             const int priority, const bool loop, const uint8_t blendMode,
                                             char *errorOut, const int errorOutLen);
        API_IMPL bool removeArticulatedAnimation(const uint32_t animationIndex, char *errorOut, const int errorOutLen);
        API_IMPL int addArticulatedTrack(const uint32_t animationIndex, const uint64_t partId,
                                         const uint8_t channelMask, char *errorOut, const int errorOutLen);
        API_IMPL bool addArticulatedKey(const uint32_t animationIndex, const uint32_t trackIndex,
                                        const float time, const float positionX, const float positionY, const float positionZ,
                                        const float rotationX, const float rotationY, const float rotationZ, const float rotationW,
                                        const float scaleX, const float scaleY, const float scaleZ,
                                        char *errorOut, const int errorOutLen);
        API_IMPL bool setArticulatedKeyEuler(const uint32_t animationIndex, const uint32_t trackIndex,
                                             const float time, const float rotationEulerX,
                                             const float rotationEulerY, const float rotationEulerZ,
                                             char *errorOut, const int errorOutLen);
        API_IMPL bool setArticulatedKeyEasing(const uint32_t animationIndex, const uint32_t trackIndex,
                                              const uint32_t keyIndex, const uint8_t easing,
                                              char *errorOut, const int errorOutLen);
        API_IMPL bool setArticulatedKeyBezier(const uint32_t animationIndex, const uint32_t trackIndex,
                                              const uint32_t keyIndex,
                                              const float x1, const float y1,
                                              const float x2, const float y2,
                                              char *errorOut, const int errorOutLen);
        API_IMPL bool updateArticulatedKey(const uint32_t animationIndex, const uint32_t trackIndex,
                                           const uint32_t keyIndex, const float time,
                                           const float positionX, const float positionY, const float positionZ,
                                           const float rotationX, const float rotationY, const float rotationZ, const float rotationW,
                                           const float scaleX, const float scaleY, const float scaleZ,
                                           char *errorOut, const int errorOutLen);
        API_IMPL bool removeArticulatedKey(const uint32_t animationIndex, const uint32_t trackIndex,
                                           const uint32_t keyIndex, char *errorOut, const int errorOutLen);
        API_IMPL void fixDefaultBoud();
        API_IMPL void release();
        API_IMPL void deleteExtraInfo();
      private:
        void fillAtLeastOneBound();
        bool fillInSubsetDebug(const MESH_MBM* meshMemory,
                               const int currentFrame,
                               const std::map<int, float>& lsLetterChangedValuesByCurFrameX,
                               const std::map<int, float>& lsLetterChangedValuesByCurFrameY,
                               util::HEADER_FRAME* headerFrame,
                               util::BUFFER_MESH_DEBUG* pBuffer);//need to be implemented by specific backend engine
        bool readDebugTriangleDetailCompat(util::MEM_CURSOR_V11 &fp, const char *fileNamePath, const int totalBounding);
        bool readFrameStaticV11Payload(util::MEM_CURSOR_V11 &fp, const util::BUFFER_MESH_DEBUG *frame0, util::BUFFER_MESH_DEBUG *&out,
                                       util::FRAME_HEADER_V11 &outFrameHeader);
        std::vector<std::string> getKnowPathsToExtraHeader();

        struct Impl;
        std::unique_ptr<Impl> impl;
    };


    class MESH_MBM
    {
        friend class MESH_MANAGER;
      public:
        API_IMPL BUFFER_MESH *getBuffer(const uint32_t index) const;
        API_IMPL TEXTURE *getTexture(const uint32_t indexFrame, const uint32_t indexSubset);
        API_IMPL bool setTexture(const uint32_t indexFrame, const uint32_t indexSubset, const char *fileNameTexture,
                               const bool hasAlpha);
        API_IMPL TEXTURE *getMaterialTexture(const uint32_t indexFrame, const uint32_t indexSubset, const TEXTURE_ROLE role) const noexcept;
        API_IMPL bool setMaterialTexture(const uint32_t indexFrame, const uint32_t indexSubset, const TEXTURE_ROLE role,
                               const char *fileNameTexture, const bool hasAlpha) const;
        API_IMPL const char *getFilenameMesh() const;
        API_IMPL INFO_PHYSICS &getPhysicsInfo() noexcept;
        API_IMPL const INFO_PHYSICS &getPhysicsInfo() const noexcept;
        API_IMPL void resetPhysicsInfo();
        API_IMPL void appendPhysicsCube(CUBE *cube) noexcept;
        API_IMPL void appendPhysicsSphere(SPHERE *sphere) noexcept;
        API_IMPL void appendPhysicsCubeComplex(CUBE_COMPLEX *cubeComplex) noexcept;
        API_IMPL void appendPhysicsTriangle(TRIANGLE *triangle) noexcept;
        API_IMPL util::INFO_ANIMATION &getAnimationInfo() noexcept;
        API_IMPL const util::INFO_ANIMATION &getAnimationInfo() const noexcept;
        API_IMPL uint32_t getTotalAnimations() const noexcept;
        API_IMPL util::INFO_ANIMATION::INFO_HEADER_ANIM *getAnimationHeader(const uint32_t index) const noexcept;
        API_IMPL virtual ~MESH_MBM();
        API_IMPL void release();
        API_IMPL void deleteExtraInfo();
        API_IMPL bool isLoaded() const;
        API_IMPL bool render(const uint32_t indexFrame,const SHADER *pShader,
                             const RENDERIZABLE *renderizableOwner = nullptr);
        API_IMPL bool hasArticulatedAnimationData() const noexcept;
        API_IMPL bool hasActiveArticulatedAnimations(const ARTICULATED_ANIMATION_PLAYER &player) const noexcept;
        API_IMPL bool playArticulatedAnimation(ARTICULATED_ANIMATION_PLAYER &player,
                                               const char *name, const int priority,
                                               const float blendDuration = 0.0f,
                                               const float weight = 1.0f) const;
        API_IMPL bool pauseArticulatedAnimation(ARTICULATED_ANIMATION_PLAYER &player,
                                                const char *name) const noexcept;
        API_IMPL bool resumeArticulatedAnimation(ARTICULATED_ANIMATION_PLAYER &player,
                                                 const char *name) const noexcept;
        API_IMPL bool disableArticulatedAnimation(ARTICULATED_ANIMATION_PLAYER &player,
                                                  const char *name) const noexcept;
        API_IMPL bool seekArticulatedAnimation(ARTICULATED_ANIMATION_PLAYER &player,
                                               const char *name, const float time) const noexcept;
        API_IMPL bool getArticulatedAnimationTime(const ARTICULATED_ANIMATION_PLAYER &player,
                                                  const char *name, float *time) const noexcept;
        API_IMPL void updateArticulatedAnimations(ARTICULATED_ANIMATION_PLAYER &player,
                                                  const float delta, RENDERIZABLE *owner,
                                                  OnEndAnimation onEndAnimation) const;
        API_IMPL bool getArticulatedTransform(const ARTICULATED_ANIMATION_PLAYER &player,
                                              const uint32_t frameIndex, const uint32_t subsetIndex,
                                              VEC3 *translation, float rotationQuaternion[4], VEC3 *scale,
                                              VEC3 *pivot, float pivotQuaternion[4]) const noexcept;
        API_IMPL bool renderArticulatedStatic(const ARTICULATED_ANIMATION_PLAYER &player,
                                              const uint32_t indexFrame, const SHADER *pShader,
                                              const MATRIX &viewMatrix, const MATRIX &perspectiveMatrix,
                                              const RENDERIZABLE *renderizableOwner = nullptr);
        API_IMPL bool renderDynamic(const uint32_t indexFrame, SHADER *pShader, VEC3 *vertex, VEC3 *normal,
                                        VEC2 *uv,
                                        const RENDERIZABLE *renderizableOwner = nullptr);
        API_IMPL util::TYPE_MESH getTypeMesh() const;
        API_IMPL VEC2 getZoomEditorSprite() const;
        API_IMPL uint32_t getTotalFrame() const;
        API_IMPL uint32_t getTotalSubset(const uint32_t indexFrame) const;
        API_IMPL const INFO_BOUND_FONT* getInfoFont()const;
        const std::vector<util::STAGE_PARTICLE*>* getInfoParticle()const;
        API_IMPL const util::BTILE_INFO* getInfoTile()const;
        API_IMPL const util::DYNAMIC_SHAPE* getInfoShape()const;
        
      private:
        MESH_MBM();
        bool buildArticulatedTransformMatrix(const ARTICULATED_ANIMATION_PLAYER &player,
                                             const uint32_t frameIndex, const uint32_t subsetIndex,
                                             MATRIX *out) const noexcept;
        bool load(const char *fileNamePath);
        bool load(const char *fileNamePath, RENDERIZABLE *renderizable);
        // Reads the v11 section/TLV format. This is the only mesh format - it backs
        // load()/MESH_MANAGER::load() directly.
        bool loadV11(const char *fileNamePath);
        // Main-thread-only GPU-finish half of loadV11 (async loading) - see mesh-manager.cpp's
        // IntermediateMeshV11/finishLoadFromIntermediate comments. Takes the forward-declared struct
        // below by reference; defined in mesh-manager.cpp only, same forward-declare-in-header
        // pattern as Impl.
        bool finishLoadFromIntermediate(MESH_LOAD_INTERMEDIATE_V11 &in, const char *fileNamePath);

        struct Impl;
        std::unique_ptr<Impl> impl;
    };

    using MeshAsyncLoadCallback = std::function<void(MESH_MBM *mesh, bool success)>;

    class MESH_MANAGER
    {
      private:
        static MESH_MANAGER *instanceMeshManager;

      public:

        API_IMPL static MESH_MANAGER *getInstance();
        API_IMPL static void release();
        API_IMPL void fakeRelease(const char* fileName);
        API_IMPL MESH_MBM *load(const char *fileName);
        API_IMPL MESH_MBM *load(const char *fileName, RENDERIZABLE *renderizable);
        API_IMPL MESH_MBM *loadTrueTypeFont(const char *fileNameTtf, const float heightLetter, const short spaceWidth,const short spaceHeight,const bool saveTextureAsPng,TEXTURE ** texture_loaded);
        API_IMPL MESH_MBM *load(const char *nickName, float *pPosition, float *pNormal, float *pTexture,const uint32_t sizeVertexBuffer,const util::INFO_DRAW_MODE * info_mode);
        API_IMPL MESH_MBM *loadIndex(const char *nickName, float *pPosition, float *pNormal, float *pTexture,const uint32_t sizeVertexBuffer, uint16_t *index,const uint32_t sizeIndex,const util::INFO_DRAW_MODE * info_draw_mode);
        API_IMPL MESH_MBM *loadDynamicIndex(const char *nickName, const uint32_t sizeVertexBuffer,uint16_t *index, const uint32_t sizeIndex,const util::INFO_DRAW_MODE * info_draw_mode, const util::DYNAMIC_SHAPE & dynamic_shape_info);
        API_IMPL MESH_MBM *getIfExists(const char* fileName);
        API_IMPL static const char * typeClassName(const util::TYPE_MESH type) noexcept;
        // Milestone 6: async loading. loadAsync() does file I/O + v11 parsing on a worker thread
        // (lazily starts a small fixed pool on first use); onComplete always fires from
        // pumpAsyncLoads() on the main thread (never inline, even on a cache hit) - call
        // pumpAsyncLoads() once per frame (CORE_MANAGER::update() does this) to finish the load
        // (GPU buffer/texture creation, which must happen on the thread owning the GL context) and
        // dispatch completions. Not wired into any render call site yet - load()/loadIndex()/etc.
        // are unaffected.
        API_IMPL void loadAsync(const char *fileName, MeshAsyncLoadCallback onComplete);
        API_IMPL void pumpAsyncLoads();
      private:
        struct Impl;
        std::unique_ptr<Impl> impl;
        MESH_MANAGER();
        virtual ~MESH_MANAGER();
    };
}

#endif
