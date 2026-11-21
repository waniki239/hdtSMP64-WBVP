# WBVP FSMP integration changelog

## 0.1.0 — initial API scaffold

- Add `PreSkinningEvent` and listener registration/removal to the Plugin API.
- Bump Plugin API from 2.0.0 to 2.1.0 (backwards-compatible additive feature).
- Dispatch immediately before FSMP consumes skin-space positions in the
  `SkinnedMeshBody::internalUpdate()` skinning pass.
- Expose stable body-instance ID, skeleton-owner FormID, body name, compacted
  vertex count, bounding-sphere mode, and bounded get/set skin-position accessors.
- Keep FSMP's private `Vertex` layout out of the public Plugin API.
- Do not change bone-skinning math, vertex compaction, Bullet collision logic,
  or update scheduling.

This is an FSMP-side integration scaffold, not a finished WBVP runtime plugin.
