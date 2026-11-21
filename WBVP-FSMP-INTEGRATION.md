# WBVP FSMP integration

This fork adds a narrow, versioned extension point to FSMP. It does not expose
FSMP's private `Vertex` layout or `SkinnedMeshBody` object to third-party code.

## API

`PluginAPI.h` increments `INTERFACE_VERSION` from 2.0.0 to 2.1.0. A plugin
may register an `IPreSkinningListener` and receive `PreSkinningEvent` just
before FSMP applies bone transforms in `SkinnedMeshBody::internalUpdate()`.

The event provides:

- `bodyId`: unique identity for this body instance; a rebuilt body receives
  a new ID.
- `ownerFormId`: skeleton-owner FormID copied at body construction; zero if
  UserData is unavailable.
- `bodyName`: FSMP body name.
- `vertexCount`: current compacted FSMP skin-vertex count.
- `usesBoundingSphere`: current update mode, useful for diagnostics.
- `getSkinPosition` / `setSkinPosition`: bounded access to skin-space
  positions.
- `context`: opaque callback context valid only until the event returns.

## Threading contract

The callback runs on the FSMP physics/update thread. CommonLib's
`BSTEventSource::SendEvent` holds its spin lock while invoking listeners.
Callbacks sent through this dispatcher are therefore serialized with respect
to one another, but must remain very short and real-time safe.

Do not block, allocate memory, perform file I/O, log synchronously, call
game-thread-only APIs, or register/unregister listeners from inside the
callback. Do not retain `context`, `bodyName`, or any other event-owned
pointer. Publish precomputed deformation data as an immutable snapshot and
perform only bounded reads/writes in the callback. Unregister listeners during
plugin shutdown, outside the callback.

## Upstream maintenance

- Upstream: `https://github.com/DaymareOn/hdtSMP64`
- Tracking branch: `dev`
- Fork base: `e52ad960048c14e637d14fcbd65314ca017fc3b8`
- API extension version: `2.1.0`

WBVP changes are deliberately isolated to Plugin API/dispatcher declarations,
the pre-skinning dispatch point, body identity metadata, and skeleton-owner
FormID capture. When upstream changes, rebase or merge upstream first, review
these integration points, confirm that the callback still runs immediately
before the skinning loop, then build and run the API smoke test. If FSMP changes
its body update lifecycle, stop and re-evaluate the event location rather than
mechanically resolving the conflict.

## Compatibility behavior

WBVP must require interface major version 2 and minor version at least 1 before
registering. On older FSMP versions, log a warning and leave collision
deformation disabled. Do not fall back to private function hooks or memory
scanning.

## Status

This is the FSMP-side API scaffold. WBVP's listener implementation and mapping
to FSMP's compacted vertex order are subsequent work; this API does not assume
that NIF partition order equals FSMP's final compacted vertex order.
