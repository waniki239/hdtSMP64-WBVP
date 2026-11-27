# WBVP Plugin API 2.2.0 — final collision geometry diagnostics

## Scope

This FSMP-side integration adds a read-only post-skinning diagnostic event for
WBVP v0.2.17-beta2. The FSMP product/CMake version remains 3.0.0; only the
Plugin API minor version changes from 2.1.0 to 2.2.0.

## Changes

- Added `PostSkinningEvent` and `IPostSkinningListener`.
- Added accessors for final FSMP collision vertex positions and transforming
  arbitrary skin-space positions through the current vertex bone weights.
- Added active point/triangle primitive metadata, effective radii/margins/
  penetration, and primitive AABBs.
- Dispatches after candidate-body skinning and required triangle companion
  point-shape updates, immediately before BVH/narrowphase collision checks.
- Reports companion point primitives only when FSMP updated that companion
  shape for the current candidate-pair set.
- Exposes no contact points, contact depth, manifold, or contact-stage data.

## Runtime and compatibility notes

- The callback is emitted only for bodies in the broadphase-selected
  candidate-pair list, not every body every frame.
- The callback runs on the physics/update thread and must remain bounded,
  allocation-free, non-blocking, and free of I/O/logging.
- API 2.1 clients continue to use the existing API prefix. API 2.2 consumers
  must check major 2/minor >= 2 before registering the new listener.
- No bone-skinning math, Bullet collision response, or physics scheduling is
  intentionally changed.

## Validation status

The source changes are committed on the dedicated development branch.
Windows/MSVC build and Skyrim in-game validation remain required.
