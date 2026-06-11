# Renderizable Architecture

This document explains the `RENDERIZABLE` class hierarchy, how the engine manages
rendering order, and the exact steps required to introduce a new renderable type —
including every corner case.

---

## 1. What Is a RENDERIZABLE?

`mbm::RENDERIZABLE` (`include/core_mbm/renderizable.h`) is the abstract base class
for every object that can be drawn on screen.  The engine maintains three **render
list categories** inside `DEVICE`:

| Category | Objects |
|------|---------|
| 3-D render list | Objects with `is3D == true` |
| 2-D world render list | 2-D world-space objects (`is3D == false`, `is2dS == false`) |
| 2-D screen render list | 2-D screen-space objects (`is2dS == true`) |

An object is added to the correct list automatically by
`DEVICE::addRenderizable(this)` — call this in your constructor.
Remove it with `DEVICE::removeRenderizable(this)` — call this in your destructor.

### Key public members

| Member | Purpose |
|--------|---------|
| `position` | World position. **`position.z` is the depth-sort key.** |
| `scale` / `angle` | Transform |
| `bounding_AABB` | Populated by `updateAABB()`, used for frustum culling |
| `enableRender` | Set to `false` to hide without removing from the list |
| `alwaysRenderize` | Skip frustum culling when `true` |
| `blend` | Blend state for this object |

### Pure virtual methods you **must** implement

| Method | Purpose |
|--------|---------|
| `render()` | Draw the object. Returns `false` on error. |
| `isOnFrustum()` | Returns `true` if the object is visible. When `false`, the engine does **not** call `render()`. If animation needs to keep ticking off-screen, update it here. |
| `onRestoreDevice()` | Reload GPU resources after a context loss. Call `load()` internally. The engine calls `onRestoreAnimationsState()` afterward (do **not** call it yourself). |
| `getFvfFromBuffer()` | Return the FVF of this object's primary GPU buffer, or `FVF_NONE`. |
| `getInfoPhysics()` | Return physics bounds, or `nullptr`. |
| `getMesh()` | Return the underlying `MESH_MBM`, or `nullptr`. |
| `isLoaded()` | Return `true` when GPU resources are ready. Used by `updateAABB()`. |
| `getFx()` | Return the active `FX` (shader effect), or `nullptr`. |
| `getAnimationManager()` | Return `this` if you inherit `ANIMATION_MANAGER`, else `nullptr`. |

---

## 2. How the Engine Sorts and Renders

Each frame `CORE_MANAGER::render()` calls `prepareRender2d()` /
`prepareRender3d()` which:

1. Calls `ptr->updateAABB()` for every registered object.
2. Calls `ptr->isOnFrustum()` — adds the object to a *per-frame* visible list if
   it returns `true`.
3. Sets `ptr->__distFromView = ptr->position.z` (2-D) or camera distance (3-D).
4. **Sorts the visible list descending by `__distFromView`** — higher `z` = further
   back = drawn first.

The engine then iterates the sorted list and calls `ptr->render()` for each object.

### Depth rules for 2-D world objects

`DEVICE::addRenderizable()` auto-assigns `position.z` via `ORDER_RENDER` if it is
`0.0` at registration time.  Values decrease by `0.1` per object, so the first
object registered gets `z = -0.1`, the next `z = -0.2`, and so on (drawn front-to-
back from a z-sort perspective).

> **Tip:** If you want a specific z value, set `position.z` to a non-zero value
> *before* calling `DEVICE::addRenderizable(this)`.

For **3-D** objects the sort key is the Euclidean distance from the camera, not `z`.

---

## 3. Step-by-Step: Adding a New RENDERIZABLE Type

### Step 1 — Add a `TYPE_CLASS` constant

In `include/core_mbm/renderizable.h`, append to the `TYPE_CLASS` enum:

```cpp
TYPE_CLASS_MY_TYPE = 19,  // use the next available integer
```

### Step 2 — Register the type name

In `src/core_mbm/renderizable.cpp`, add a case in `RENDERIZABLE::getTypeClassName()`:

```cpp
case TYPE_CLASS_MY_TYPE : return "my-type";
```

### Step 3 — Declare the class

Create a header (or add to an existing one) inheriting `RENDERIZABLE`:

```cpp
class MY_TYPE : public RENDERIZABLE
{
public:
    MY_TYPE(const SCENE* scene);
    virtual ~MY_TYPE();
    FVF_PROVIDE_BY_ENGINE getFvfFromBuffer() const noexcept override;
    const INFO_PHYSICS*  getInfoPhysics()   const override;
    const MESH_MBM*      getMesh()          const override;
    bool                 isLoaded()         const override;
    FX*                  getFx()            const override;
    ANIMATION_MANAGER*   getAnimationManager() override;
protected:
    bool isOnFrustum()     override;
    bool render()          override;
    bool onRestoreDevice() override;
};
```

### Step 4 — Register / deregister with DEVICE

```cpp
MY_TYPE::MY_TYPE(const SCENE* scene)
: RENDERIZABLE(scene->getIdScene(), TYPE_CLASS_MY_TYPE,
               /*is3D=*/false, /*is2dS=*/false)
{
    // Optionally set position.z before this call if you need a specific depth.
    mbm::DEVICE::getInstance()->addRenderizable(this);
}

MY_TYPE::~MY_TYPE()
{
    mbm::DEVICE::getInstance()->removeRenderizable(this);
}
```

### Step 5 — Choose the spatial mode

Pass the correct flags to the `RENDERIZABLE` constructor:

| Mode | `is3D` | `is2dS` |
|------|--------|---------|
| 2-D world-space | `false` | `false` |
| 2-D screen-space | `false` | `true` |
| 3-D | `true` | `false` |

---

## 4. Corner Case: Child / Proxy Renderizables

Sometimes you need a sub-object that **shares resources** with a parent but occupies
its own **z-sort slot** (e.g. `TILE_LAYER`, `TILE_OBJ`).

Pattern:

1. Store a `PARENT_TYPE* ptr_parent` member.
2. Delegate `getFvfFromBuffer`, `getMesh`, `isLoaded`, `getFx`,
   `getAnimationManager`, `getInfoPhysics` to `ptr_parent`.
3. Implement `isOnFrustum()` using `IS_ON_FRUSTUM verify(ptr_parent)` so the parent's
   bounding box drives culling. Update the sub-object's specific animation when
   off-frustum.
4. Implement `render()` to call an internal method on the parent, passing
   `this->position.z` so the parent builds the MVP matrix with the correct depth.
5. Implement `onRestoreDevice()` to return `true` — the parent handles all GPU
   resource reloading.
6. Because the proxy calls private methods on the parent, declare
   `friend class MY_PROXY_TYPE;` inside the parent class.

**Lifecycle:**

- The parent creates proxy objects after its own `load()` succeeds and deletes
  them in its `release()` (or equivalent cleanup).
- Each proxy's constructor/destructor calls `addRenderizable` / `removeRenderizable`,
  so they automatically participate in z-sorting.
- During `onRestoreDevice()`, the parent must:
  1. Save proxy `position.z` values (the user may have changed them at runtime).
  2. Delete all proxies (their destructors deregister from the device).
  3. Reload its mesh (`load()`), which recreates the proxies.
  4. Restore the saved `position.z` values.

Example — `TILE` with `TILE_LAYER`:

```cpp
// TILE::onRestoreDevice (simplified)
std::vector<float> savedZ(lsLayerRenderizables.size());
for (size_t i = 0; i < lsLayerRenderizables.size(); ++i)
    savedZ[i] = lsLayerRenderizables[i]->position.z;
for (auto* l : lsLayerRenderizables) delete l;
lsLayerRenderizables.clear();

this->mesh = nullptr;
if (this->load(this->fileName.c_str()))
{
    for (size_t i = 0; i < lsLayerRenderizables.size() && i < savedZ.size(); ++i)
        lsLayerRenderizables[i]->position.z = savedZ[i];
    return true;
}
return false;
```

---

## 5. Corner Case: Lua Exposure — YES

If the new type must be accessible from Lua scripts as its own userdata object:

### 5a — Add a Lua user-type identifier

In `include/core_mbm/class-identifier.h`, add before `L_USER_TYPE_END`:

```cpp
L_USER_TYPE_MY_TYPE ,
```

### 5b — Register the string tag

In `src/core_mbm/class-identifier.cpp`, add a case in `getUserTypeAsString()`:

```cpp
case L_USER_TYPE_MY_TYPE : return "_usertype_my_type";
```

### 5c — Register as a renderizable (if applicable)

In the same file, add to `isRenderizableType()`:

```cpp
case L_USER_TYPE_MY_TYPE : return true;
```

### 5d — Create the Lua binding

Create `src/lua-wrap/render-table/my-type-lua.cpp` following the pattern in
`tile-lua.cpp`:

- Push userdata: `lua_newuserdata` + `luaL_getmetatable` using the string from 5b.
- Metatable: `__index`, `__gc` (calls `delete` or `device->removeRenderizable`).
- Method table: one C function per exposed method.
- Register the constructor in the global Lua environment (usually in
  `src/lua-wrap/register-all-lua.cpp`).

---

## 6. Corner Case: Lua Exposure — NO

If the type is an internal engine mechanism (not directly manipulable from Lua),
**skip Section 5 entirely**.

- Do **not** add `L_USER_TYPE_*` to `class-identifier.h`.
- Do **not** add cases to `getUserTypeAsString()` or `isRenderizableType()`.
- Expose the type's behavior through the parent object's Lua API instead
  (e.g. `tTile:getZLayer(n)` / `tTile:setZLayer(n, z)` for `TILE_LAYER`).

---

## 7. Corner Case: Parent Wrapper Objects

When a single logical entity needs to:
- Render shared/background content at its own z-position **and**
- Spawn N child renderizables that each occupy individual z-positions

do the following for the parent:

1. Keep the parent registered in the device — it renders only the shared/background
   part in its own `render()`.
2. **Do not render child elements inside the parent's `render()`** — each child
   renders itself when the engine calls its own `render()`.
3. The parent's `isOnFrustum()` should **not** perform animation updates for the
   children — each child's `isOnFrustum()` handles that independently.
4. The parent owns the shared resources (mesh, animations, textures) and manages
   the lifetime of all child proxies.

---

## 8. `onRestoreDevice()` Contract

Called by the engine after a GPU-context loss (e.g. window resize on some backends,
app resume on mobile).

**What you must do:**

1. Set `this->mesh = nullptr` (or equivalent GPU resource pointer).
2. Call your `load()` function to reload the resource.
3. Return `true` on success, `false` on failure.

**What the engine does after a successful restore:**

- Calls `onRestoreAnimationsState()` (declared `final` in `RENDERIZABLE`) which
  restores the animation state from the backup created by `onStop()`.
- Do **not** call these yourself.

**Child proxies:** return `true` immediately and let the parent handle the reload.
The parent is responsible for recreating all proxies (see Section 4).
