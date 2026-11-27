#include "hdtSkinnedMeshBody.h"
#include "../PluginInterfaceImpl.h"
#include "hdtSkinnedMeshShape.h"

#include <tbb/tbb.h>

#include <atomic>
#include <cfloat>

namespace hdt
{

	SkinnedMeshBody::SkinnedMeshBody()
	{
		static std::atomic<std::uint64_t> nextWbvpBodyId{ 1 };
		m_wbvpBodyId = nextWbvpBodyId.fetch_add(1, std::memory_order_relaxed);
		m_collisionShape = &m_bulletShape;
	}

	SkinnedMeshBody::~SkinnedMeshBody()
	{
		//m_bonesGPU.discard_data();
		//m_verticesGPU.discard_data();
	}

	__forceinline __m128 calcVertexState(__m128 skinPos, const Bone& bone, __m128 w)
	{
		auto p = bone.m_vertexToWorld * skinPos;
		p = _mm_blend_ps(p.get128(), _mm_load_ps(bone.m_reserved), 0x8);
		return _mm_mul_ps(w, p.get128());
	}

#if defined(__AVX2__)
	__forceinline __m128 calcVertexStateFMA(__m128 skinPos, const Bone& bone, __m128 w)
	{
		__m128 px = pshufd<0x00>(skinPos);
		__m128 py = pshufd<0x55>(skinPos);
		__m128 pz = pshufd<0xAA>(skinPos);
		__m128 r = _mm_fmadd_ps(bone.m_vertexToWorld.m_col[2].get128(), pz, bone.m_vertexToWorld.m_col[3].get128());
		r = _mm_fmadd_ps(bone.m_vertexToWorld.m_col[1].get128(), py, r);
		r = _mm_fmadd_ps(bone.m_vertexToWorld.m_col[0].get128(), px, r);
		r = _mm_blend_ps(r, _mm_load_ps(bone.m_reserved), 0x8);
		return _mm_mul_ps(w, r);
	}
#endif

	void SkinnedMeshBody::internalUpdate()
	{
		const size_t numBones = m_skinnedBones.size();
		const auto* __restrict skinnedBones = m_skinnedBones.data();
		auto* __restrict bonesDst = m_bones.data();

		for (size_t i = 0; i < numBones; ++i) {
			if (i + 8 < numBones)
				_mm_prefetch((const char*)&skinnedBones[i + 8], _MM_HINT_T1);
			if (i + 4 < numBones)
				_mm_prefetch((const char*)skinnedBones[i + 4].ptr, _MM_HINT_T0);
			auto& v = skinnedBones[i];
			auto boneT = v.ptr->m_currentTransform;
			bonesDst[i].m_vertexToWorld = btMatrix4x3T(boneT) * v.vertexToBone;
			bonesDst[i].m_maginMultipler = v.ptr->m_marginMultipler * boneT.getScale();
		}

		// Allow plugins to deform skin-space input positions before FSMP performs
		// its normal bone skinning. The accessors deliberately hide Vertex layout.
		const PreSkinningEvent preSkinningEvent{
			m_wbvpBodyId,
			m_wbvpOwnerFormId,
			m_name.c_str(),
			m_vertices.size(),
			m_useBoundingSphere,
			this,
			[](void* context, std::size_t index, float outXYZ[3]) {
				const auto* body = static_cast<const SkinnedMeshBody*>(context);
				if (!body || index >= body->m_vertices.size() || !outXYZ) return;
				const auto& p = body->m_vertices[index].m_skinPos;
				outXYZ[0] = static_cast<float>(p.x());
				outXYZ[1] = static_cast<float>(p.y());
				outXYZ[2] = static_cast<float>(p.z());
			},
			[](void* context, std::size_t index, const float xyz[3]) {
				auto* body = static_cast<SkinnedMeshBody*>(context);
				if (!body || index >= body->m_vertices.size() || !xyz) return;
				body->m_vertices[index].m_skinPos.setValue(xyz[0], xyz[1], xyz[2]);
			}
		};
		g_pluginInterface.onPreSkinning(preSkinningEvent);

		const int size = static_cast<int>(m_vpos.size());
		const Vertex* __restrict verts = m_vertices.data();
		VertexPos* __restrict vpos = m_vpos.data();
		const Bone* __restrict bones = bonesDst;

		// We can use AVX2 here due to sequential memory reads..
#if defined(__AVX2__)

		constexpr int PF = 6;
		int idx = 0;
		for (; idx + 1 < size; idx += 2) {
			if (idx + PF < size) {
				_mm_prefetch((const char*)&verts[idx + PF], _MM_HINT_T0);
				_mm_prefetch((const char*)&verts[idx + PF + 1], _MM_HINT_T0);
			}

			{
				auto& v = verts[idx];
				auto p = v.m_skinPos.get128();
				auto w = _mm_load_ps(v.m_weight);
				auto pm = calcVertexStateFMA(p, bones[v.getBoneIdx(0)], setAll0(w));
				pm += calcVertexStateFMA(p, bones[v.getBoneIdx(1)], setAll1(w));
				pm += calcVertexStateFMA(p, bones[v.getBoneIdx(2)], setAll2(w));
				pm += calcVertexStateFMA(p, bones[v.getBoneIdx(3)], setAll3(w));
				vpos[idx].set(pm);
			}
			{
				auto& v = verts[idx + 1];
				auto p = v.m_skinPos.get128();
				auto w = _mm_load_ps(v.m_weight);
				auto pm = calcVertexStateFMA(p, bones[v.getBoneIdx(0)], setAll0(w));
				pm += calcVertexStateFMA(p, bones[v.getBoneIdx(1)], setAll1(w));
				pm += calcVertexStateFMA(p, bones[v.getBoneIdx(2)], setAll2(w));
				pm += calcVertexStateFMA(p, bones[v.getBoneIdx(3)], setAll3(w));
				vpos[idx + 1].set(pm);
			}
		}
		for (; idx < size; ++idx) {
			auto& v = verts[idx];
			auto p = v.m_skinPos.get128();
			auto w = _mm_load_ps(v.m_weight);
			auto pm = calcVertexStateFMA(p, bones[v.getBoneIdx(0)], setAll0(w));
			pm += calcVertexStateFMA(p, bones[v.getBoneIdx(1)], setAll1(w));
			pm += calcVertexStateFMA(p, bones[v.getBoneIdx(2)], setAll2(w));
			pm += calcVertexStateFMA(p, bones[v.getBoneIdx(3)], setAll3(w));
			vpos[idx].set(pm);
		}

#else

		constexpr int PF = 8;
		int idx = 0;
		for (; idx + 3 < size; idx += 4) {
			if (idx + PF + 3 < size) {
				_mm_prefetch((const char*)&verts[idx + PF], _MM_HINT_T0);
				_mm_prefetch((const char*)&verts[idx + PF + 1], _MM_HINT_T0);
				_mm_prefetch((const char*)&verts[idx + PF + 2], _MM_HINT_T0);
				_mm_prefetch((const char*)&verts[idx + PF + 3], _MM_HINT_T0);
			}

			{
				auto& v = verts[idx];
				auto p = v.m_skinPos.get128();
				auto w = _mm_load_ps(v.m_weight);
				auto pm = calcVertexState(p, bones[v.getBoneIdx(0)], setAll0(w));
				pm += calcVertexState(p, bones[v.getBoneIdx(1)], setAll1(w));
				pm += calcVertexState(p, bones[v.getBoneIdx(2)], setAll2(w));
				pm += calcVertexState(p, bones[v.getBoneIdx(3)], setAll3(w));
				vpos[idx].set(pm);
			}
			{
				auto& v = verts[idx + 1];
				auto p = v.m_skinPos.get128();
				auto w = _mm_load_ps(v.m_weight);
				auto pm = calcVertexState(p, bones[v.getBoneIdx(0)], setAll0(w));
				pm += calcVertexState(p, bones[v.getBoneIdx(1)], setAll1(w));
				pm += calcVertexState(p, bones[v.getBoneIdx(2)], setAll2(w));
				pm += calcVertexState(p, bones[v.getBoneIdx(3)], setAll3(w));
				vpos[idx + 1].set(pm);
			}
			{
				auto& v = verts[idx + 2];
				auto p = v.m_skinPos.get128();
				auto w = _mm_load_ps(v.m_weight);
				auto pm = calcVertexState(p, bones[v.getBoneIdx(0)], setAll0(w));
				pm += calcVertexState(p, bones[v.getBoneIdx(1)], setAll1(w));
				pm += calcVertexState(p, bones[v.getBoneIdx(2)], setAll2(w));
				pm += calcVertexState(p, bones[v.getBoneIdx(3)], setAll3(w));
				vpos[idx + 2].set(pm);
			}
			{
				auto& v = verts[idx + 3];
				auto p = v.m_skinPos.get128();
				auto w = _mm_load_ps(v.m_weight);
				auto pm = calcVertexState(p, bones[v.getBoneIdx(0)], setAll0(w));
				pm += calcVertexState(p, bones[v.getBoneIdx(1)], setAll1(w));
				pm += calcVertexState(p, bones[v.getBoneIdx(2)], setAll2(w));
				pm += calcVertexState(p, bones[v.getBoneIdx(3)], setAll3(w));
				vpos[idx + 3].set(pm);
			}
		}
		for (; idx < size; ++idx) {
			auto& v = verts[idx];
			auto p = v.m_skinPos.get128();
			auto w = _mm_load_ps(v.m_weight);
			auto pm = calcVertexState(p, bones[v.getBoneIdx(0)], setAll0(w));
			pm += calcVertexState(p, bones[v.getBoneIdx(1)], setAll1(w));
			pm += calcVertexState(p, bones[v.getBoneIdx(2)], setAll2(w));
			pm += calcVertexState(p, bones[v.getBoneIdx(3)], setAll3(w));
			vpos[idx].set(pm);
		}

#endif

		m_shape->internalUpdate();
		m_bulletShape.m_aabb = m_shape->m_tree.aabbAll;
	}


	void SkinnedMeshBody::notifyPostSkinning(bool pointCollidersUpdated)
	{
		const PostSkinningEvent postSkinningEvent = [this, pointCollidersUpdated] {
			PostSkinningEvent event{};
			event.bodyId = m_wbvpBodyId;
			event.ownerFormId = m_wbvpOwnerFormId;
			event.bodyName = m_name.c_str();
			event.vertexCount = m_vpos.size();
			auto* pointShape = m_shape->asPerVertexShape();
			auto* triangleShape = m_shape->asPerTriangleShape();
			event.pointColliderCount = pointShape && pointCollidersUpdated ? pointShape->m_colliders.size() : 0;
			event.triangleColliderCount = triangleShape ? triangleShape->m_colliders.size() : 0;
			event.usesBoneSphereBroadphase = m_useBoundingSphere;
			event.usesTriangleShape = triangleShape != nullptr;
			event.context = this;
			event.getCollisionPosition = [](void* context, std::size_t index, float outXYZ[3]) -> bool {
				const auto* body = static_cast<const SkinnedMeshBody*>(context);
				if (!body || !outXYZ || index >= body->m_vpos.size()) return false;
				const auto p = body->m_vpos[index].pos();
				outXYZ[0] = static_cast<float>(p.x());
				outXYZ[1] = static_cast<float>(p.y());
				outXYZ[2] = static_cast<float>(p.z());
				return true;
			};
			event.transformSkinPosition = [](void* context, std::size_t index, const float skinXYZ[3], float outXYZ[3]) -> bool {
				const auto* body = static_cast<const SkinnedMeshBody*>(context);
				if (!body || !skinXYZ || !outXYZ || index >= body->m_vertices.size()) return false;
				const auto& vertex = body->m_vertices[index];
				const __m128 skinPos = _mm_set_ps(0.0f, skinXYZ[2], skinXYZ[1], skinXYZ[0]);
				__m128 result = _mm_setzero_ps();
				for (int i = 0; i < 4; ++i) {
					const auto boneIndex = vertex.getBoneIdx(i);
					if (boneIndex >= body->m_bones.size()) return false;
					result = _mm_add_ps(result, calcVertexState(skinPos, body->m_bones[boneIndex], _mm_set1_ps(vertex.m_weight[i])));
				}
				alignas(16) float xyz[4]{};
				_mm_store_ps(xyz, result);
				outXYZ[0] = xyz[0];
				outXYZ[1] = xyz[1];
				outXYZ[2] = xyz[2];
				return true;
			};
			event.getPointCollider = [](void* context, std::size_t index, PointColliderInfo* outInfo) -> bool {
				auto* body = static_cast<SkinnedMeshBody*>(context);
				if (!body || !outInfo) return false;
				auto* shapeBase = body->m_shape.get();
				if (!shapeBase) return false;
				const auto* shape = shapeBase->asPerVertexShape();
				if (!shape || index >= shape->m_colliders.size() || index >= shape->m_aabb.size()) return false;
				const auto& collider = shape->m_colliders[index];
				if (collider.vertex >= body->m_vpos.size()) return false;
				outInfo->vertexIndex = collider.vertex;
				outInfo->flexible = collider.flexible;
				outInfo->effectiveRadius = body->m_vpos[collider.vertex].marginMultiplier() * shape->m_shapeProp.margin;
				const auto& aabb = shape->m_aabb[index];
				for (int axis = 0; axis < 3; ++axis) {
					outInfo->aabbMin[axis] = aabb.m_min.m128_f32[axis];
					outInfo->aabbMax[axis] = aabb.m_max.m128_f32[axis];
				}
				return true;
			};
			event.getTriangleCollider = [](void* context, std::size_t index, TriangleColliderInfo* outInfo) -> bool {
				auto* body = static_cast<SkinnedMeshBody*>(context);
				if (!body || !outInfo) return false;
				auto* shapeBase = body->m_shape.get();
				if (!shapeBase) return false;
				const auto* shape = shapeBase->asPerTriangleShape();
				if (!shape || index >= shape->m_colliders.size() || index >= shape->m_aabb.size()) return false;
				const auto& collider = shape->m_colliders[index];
				for (int i = 0; i < 3; ++i) {
					if (collider.vertices[i] >= body->m_vpos.size()) return false;
					outInfo->vertexIndices[i] = collider.vertices[i];
				}
				const float averageMarginMultiplier = (
					body->m_vpos[collider.vertices[0]].marginMultiplier() +
					body->m_vpos[collider.vertices[1]].marginMultiplier() +
					body->m_vpos[collider.vertices[2]].marginMultiplier()) / 3.0f;
				outInfo->flexible = collider.flexible;
				outInfo->effectiveMargin = averageMarginMultiplier * shape->m_shapeProp.margin;
				outInfo->effectivePenetration = averageMarginMultiplier * shape->m_shapeProp.penetration;
				const auto& aabb = shape->m_aabb[index];
				for (int axis = 0; axis < 3; ++axis) {
					outInfo->aabbMin[axis] = aabb.m_min.m128_f32[axis];
					outInfo->aabbMax[axis] = aabb.m_max.m128_f32[axis];
				}
				return true;
			};
			return event;
		}();
		g_pluginInterface.onPostSkinning(postSkinningEvent);
	}

	float SkinnedMeshBody::flexible(const Vertex& v)
	{
		float ret = 0.f;
		for (int i = 0; i < 4; ++i) {
			if (v.m_weight[i] < FLT_EPSILON)
				break;
			int boneIdx = v.getBoneIdx(i);
			if (!m_skinnedBones[boneIdx].isKinematic)
				ret += v.m_weight[i];
		}
		return ret;
	}

	int SkinnedMeshBody::addBone(SkinnedMeshBone* bone, const btQsTransform& verticesToBone,
		const BoundingSphere& boundingSphere)
	{
		m_skinnedBones.push_back(SkinnedBone());
		auto& v = m_skinnedBones.back();
		v.ptr = bone;
		v.vertexToBone = verticesToBone;
		v.localBoundingSphere = boundingSphere;
		v.isKinematic = bone->m_rig.isStaticOrKinematicObject();
		return static_cast<int>(m_skinnedBones.size() - 1);
	}

	void SkinnedMeshBody::finishBuild()
	{
		m_bones.resize(m_skinnedBones.size());
		m_shape->clipColliders();
		m_vpos.resize(m_vertices.size());

		m_isKinematic = true;
		for (auto& i : m_skinnedBones) {
			i.isKinematic = i.ptr->m_rig.isStaticOrKinematicObject();
			if (!i.isKinematic)
				m_isKinematic = false;
		}

		m_shape->finishBuild();

		bool* flags = new bool[m_vertices.size()];
		ZeroMemory(flags, m_vertices.size());
		m_shape->markUsedVertices(flags);

		UINT numUsed = 0;
		std::vector<UINT> map(m_vertices.size());
		for (int i = 0; i < m_vertices.size(); ++i) {
			if (flags[i]) {
				m_vertices[numUsed] = m_vertices[i];
				m_vpos[numUsed] = m_vpos[i];
				map[i] = numUsed++;
			}
		}
		delete[] flags;
		m_shape->remapVertices(map.data());
		m_vertices.resize(numUsed);
		m_vpos.resize(numUsed);

		m_useBoundingSphere = m_shape->m_colliders.size() > 10;
	}

	bool SkinnedMeshBody::canCollideWith(const SkinnedMeshBody* body) const
	{
		if (m_isKinematic && body->m_isKinematic)
			return false;

		if (m_canCollideWithTags.empty()) {
			for (auto& i : body->m_tags)
				if (m_noCollideWithTags.find(i) != m_noCollideWithTags.end())
					return false;
			return true;
		}
		for (auto& i : body->m_tags)
			if (m_canCollideWithTags.find(i) != m_canCollideWithTags.end())
				return true;
		return false;
	}

	void SkinnedMeshBody::updateBoundingSphereAabb()
	{
		m_bulletShape.m_aabb.invalidate();
		for (auto& i : m_skinnedBones) {
			auto sp = i.localBoundingSphere;
			auto tr = i.ptr->m_currentTransform;
			i.worldBoundingSphere = BoundingSphere(tr * sp.center(), tr.getScale() * sp.radius());
			m_bulletShape.m_aabb.merge(i.worldBoundingSphere.getAabb());
		}

		if (!m_useBoundingSphere)
			internalUpdate();
	}

	bool SkinnedMeshBody::isBoundingSphereCollided(SkinnedMeshBody* rhs)
	{
		if (canCollideWith(rhs) && rhs->canCollideWith(this)) {
			return true;
		}
		return false;
	}
}
