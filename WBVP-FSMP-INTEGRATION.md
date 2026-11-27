# WBVP FSMP integration

This fork exposes a versioned, read-only diagnostic API for WBVP. It does not
expose FSMP's private `Vertex` layout or `SkinnedMeshBody` object to third-party
code.

## Versioning

- FSMP product/CMake version: `3.0.0` (unchanged).
- FSMP Plugin API version: `2.2.0`.
- WBVP consumer version for this integration: `0.2.17-beta2`.

These are separate version namespaces. The Plugin API minor version is bumped
for this backwards-compatible extension; the FSMP product version is not
changed by this integration-only API addition.

## API 2.1: pre-skinning

An `IPreSkinningListener` receives `PreSkinningEvent` immediately before
FSMP applies bone transforms in `SkinnedMeshBody::internalUpdate()`. It
provides bounded access to skin-space positions through
`getSkinPosition` / `setSkinPosition`, plus body identity and owner metadata.

## API 2.2: post-skinning collision snapshot

An `IPostSkinningListener` receives `PostSkinningEvent` after the candidate
body's final skinned vertex positions and required collider data have been
updated, immediately before FSMP's BVH/narrowphase collision-pair checks.

The event exposes:

- Body identity, owner FormID, body name, and final skinned vertex count.
- `getCollisionPosition`: final FSMP collision vertex position (`m_vpos`).
- `transformSkinPosition`: transforms a supplied skin-space position through
  the same body's current bone weights/transforms.
- `getPointCollider`: point primitive vertex index, flexibility, effective
  radius, and its AABB.
- `getTriangleCollider`: triangle vertex indices, flexibility, effective
  margin/penetration, and its AABB.
- `usesBoneSphereBroadphase`: whether bone spheres are used for broadphase
  bounds. This does not mean detailed point/triangle primitives are absent.
- `usesTriangleShape`: whether the body's detailed shape is triangle-based.

The point-collider companion shape for a triangle body is reported only when
FSMP updated that companion shape for the current candidate-pair set.
Otherwise `pointColliderCount` is zero. The triangle primitive set is still
reported. The API exports active primitive lists and their AABBs, not the
internal BVH node hierarchy.

## Candidate-pair and threading contract

The post-skinning callback is emitted only for bodies present in the
broadphase-selected candidate-pair body list for that dispatcher update. A
body that is not part of a candidate pair in a frame does not emit an event
for that frame. This is not a global per-frame snapshot of every equipped body.

The callback runs on the FSMP physics/update thread. The event and its
accessors are valid only until the listener returns. Do not block, allocate
memory, perform file I/O, log synchronously, call game-thread-only APIs, or
register/unregister listeners inside the callback. Do not retain `context`,
`bodyName`, or any event-owned pointer. Keep the callback bounded and
real-time safe. Unregister listeners during plugin shutdown, outside callbacks.

## Compatibility

A plugin requiring final collision diagnostics must verify interface major
version 2 and minor version at least 2 before registering an
`IPostSkinningListener`. API 2.1 remains usable for pre-skinning deformation,
but does not provide final collision snapshots. Do not fall back to private
function hooks or memory scanning.

## Upstream maintenance

- Upstream: `https://github.com/DaymareOn/hdtSMP64`
- Tracking branch: `dev`
- Fork integration branch: `wbvp-integration`
- API extension branch: `wbvp-pluginapi-2.2.0-final-collision-diagnostics`

WBVP-specific changes are isolated to Plugin API/dispatcher declarations,
the pre- and post-skinning dispatch points, and body metadata/accessors. When
upstream changes, merge or rebase upstream first, review these integration
points, confirm callback timing and threading, then build and run API smoke
tests. If FSMP changes its body update lifecycle, re-evaluate the event
location instead of mechanically resolving conflicts.

## Status

The API implementation is present on the API 2.2 development branch. It still
requires an FSMP build and in-game validation with WBVP Beta 2.
