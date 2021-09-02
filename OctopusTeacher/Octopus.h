#pragma once

#include "utils.h"
#include <vector>
#include <algorithm>

using namespace std;
using namespace wiECS;
using namespace wiScene;

const static float strideLength = 4;
const static float smoothSpeed = 1;

struct Octopus {

	vector<vector<Entity>> limbs;
	vector<XMFLOAT3> relativeTargetGoals;
	vector<XMFLOAT3> targets;
	vector<XMFLOAT3> previousTargets;
	vector<float> targetTimes;

	Entity octopusScene;

	vector<vector<Entity>> FindLimbsFromScene(Entity octopusScene)
	{
		const auto getOctopus = [](vector<Entity> entities)
		{
			for (const auto entity : entities)
				if (componentFromEntity<NameComponent>(entity)->name.compare("OctopusRiggedTopo.glb") == 0)
					return entity;
		};
		const auto getArmature = [](vector<Entity> entities)
		{
			for (const auto entity : entities)
				if (componentFromEntity<NameComponent>(entity)->name.compare("Armature") == 0)
					return entity;
		};
		const auto getArms = [](vector<Entity> entities)
		{
			vector<Entity> arms;
			for (const auto entity : entities)
			{
				auto name = componentFromEntity<NameComponent>(entity)->name;
				auto prefix = name.substr(0, 3);
				if (prefix.compare("Arm") == 0 && name.length() == 4)
					arms.push_back(entity);
			}
			return arms;
		};
		const auto getLimbs = [](vector<Entity> arms)
		{
			vector<vector<Entity>> limbs;
			transform(
				arms.begin(), arms.end(), back_inserter(limbs),
				[](auto entity) {
					vector<Entity> bones;
					Entity parent = entity;
					while (true)
					{
						vector<Entity> childEntities = getEntitiesForParent<TransformComponent>(parent);
						if (childEntities.size() == 0) break;
						Entity childEntity = childEntities.front();
						bones.push_back(childEntity);
						parent = childEntity;
					}
					return bones;
				});
			return limbs;
		};
		const auto namedEntsUnderOctopusScene = getEntitiesForParent<NameComponent>(octopusScene);
		const auto octopus = getOctopus(namedEntsUnderOctopusScene);
		const auto namesUnderOctopus = getEntitiesForParent<NameComponent>(octopus);
		const auto armature = getArmature(namesUnderOctopus);
		const auto namesUnderArmature = getEntitiesForParent<NameComponent>(armature);
		const auto armsEntities = getArms(namesUnderArmature);
		return getLimbs(armsEntities);
	}

	Octopus() {

	}

	Octopus(
		Entity octopusScene
	): octopusScene(octopusScene) {
		limbs = FindLimbsFromScene(octopusScene);

		// Size limb bones down closer to the tips
		int limbIndex = 0;
		for (const auto limb : limbs)
		{

			int boneIndex = 0;
			for (auto boneEnt : limb)
			{
				const auto bone = mutableComponentFromEntity<TransformComponent>(boneEnt);
				bone->SetDirty();
				auto scale = XMLoadFloat3(&bone->scale_local);
				scale = XMVectorScale(scale, 1.0 / (boneIndex * 0.05f + 1));
				XMStoreFloat3(&bone->scale_local, scale);
				boneIndex++;
			}
			limbIndex++;
		}

		// Initialize targets to tips
		const auto octopusMatrix = localToGlobalMatrix(getAncestryForEntity(octopusScene));
		for (const auto limb : limbs)
		{
			const Entity lastBone = limb[limb.size() - 1];
			const auto matrix = localToGlobalMatrix(getAncestryForParentChild(octopusScene, lastBone));
			XMFLOAT3 goal;
			XMStoreFloat3(&goal, XMVector4Transform({ 0, 0, 0, 1 }, matrix));
			goal.x *= 0.75;
			goal.y = -0.5;
			goal.z *= 0.75;
			relativeTargetGoals.push_back(goal);
			XMFLOAT3 target;
			XMStoreFloat3(&target, XMVector4Transform({ goal.x, goal.y, goal.z, 1 }, octopusMatrix));
			targets.push_back(target);
			previousTargets.push_back(target);
			targetTimes.push_back(0);
		}

		// Hard-coding octopus coloration for now
		GetScene().materials.Update(findWithName("Material"), {
			.baseColor = XMFLOAT4(153 / 255.0f, 164 / 255.0f, 255 / 255.0f, 1.0f),
			.subsurfaceScattering = XMFLOAT4(255 / 255.0f, 111 / 255.0f, 0 / 255.0f, 0.25f)
			});
	}

	void SimpleDance(float time)
	{
		int limbIndex = 0;
		for (const auto limb : limbs)
		{
			int boneIndex = 0;
			for (const auto boneEnt : limb)
			{
				const auto bone = mutableComponentFromEntity<TransformComponent>(boneEnt);
				XMVECTOR quat{ 0, 0, 0, 1 };
				
				const float angle = sin(time + limbIndex + boneIndex * 0.05f) * 0 * 3.14f / 180.0f;
				const auto x = XMQuaternionRotationNormal({ 1, 0, 0 }, angle);
				quat = XMQuaternionMultiply(x, quat);
				quat = XMQuaternionNormalize(quat);
				XMStoreFloat4(&bone->rotation_local, quat);
				boneIndex++;
			}
			limbIndex++;
		}
	}

	XMMATRIX previousOctopusMatrix;

	void Retargetting(float time)
	{
		const auto octopusMatrix = localToGlobalMatrix(getAncestryForEntity(octopusScene));

		vector<XMVECTOR> newTargets;
		vector<XMVECTOR> targetDeltas;
		vector<XMVECTOR> historyDeltas;
		vector<float> dots;
		for (int i = 0; i < limbs.size(); i++)
		{
			const auto limb = limbs[i];
			const auto goal = relativeTargetGoals[i];
			const Entity lastBone = limb[limb.size() - 1];
			const auto matrix = localToGlobalMatrix(getAncestryForParentChild(octopusScene, lastBone));
			const auto newTarget = XMVector4Transform({ goal.x, goal.y, goal.z, 1 }, octopusMatrix);
			const auto previousTarget = XMVector4Transform({ goal.x, goal.y, goal.z, 1 }, previousOctopusMatrix);
			const auto historyDelta = newTarget - previousTarget;
			const auto targetDelta = newTarget - XMLoadFloat3(&targets[i]);
			historyDeltas.push_back(historyDelta);
			targetDeltas.push_back(targetDelta);
			newTargets.push_back(newTarget);
			dots.push_back(XMVectorGetX(XMVector3Dot(historyDelta, targetDelta))); // If dot is positive, then we are on the end of a stride, if this number is negative we are at the start
		}
		for (int i = 0; i < limbs.size(); i++)
		{
			const auto neighborIndex = (1 - i % 2) + i / 2 * 2;
			const auto dominant = i % 2 == 0;
			const auto historyDistance = XMVectorGetX(XMVector3Length(historyDeltas[i]));
			const auto targetDistance = XMVectorGetX(XMVector3Length(targetDeltas[i]));
			if (dots[neighborIndex] >= 0
				&& dots[i] > 0
				&& (dots[i] > dots[neighborIndex] || dominant && abs(dots[i] - dots[neighborIndex]) < FLT_EPSILON)
				&& targetDistance > FLT_EPSILON)
			{
				const auto lastTarget = XMLoadFloat3(&targets[i]);
				const auto nextTarget = newTargets[i];
				const auto usedDelta = historyDistance > FLT_EPSILON
					? historyDeltas[i] / historyDistance
					: targetDeltas[i] / targetDistance;
				XMFLOAT3 result;
				XMStoreFloat3(&result, nextTarget + usedDelta * strideLength);
				previousTargets[i] = targets[i];
				targets[i] = result;
				targetTimes[i] = time;
			}
		}

		previousOctopusMatrix = octopusMatrix;
	}

	void BasicGrasp(float time)
	{
		struct Segment {
			float length;
		};

		const auto getRelativeTarget = [](XMFLOAT3 target, Entity bone) {
			const auto matrix = localToGlobalMatrix(getAncestryForEntity(bone));
			XMVECTOR S, R, start;
			XMMatrixDecompose(&S, &R, &start, matrix);
			return XMVECTOR{ target.x, target.y, target.z, 1 } - start;
		};

		const auto getSegments = [](vector<Entity> bones) {
			int boneIndex = 0;
			vector<Segment> segments;
			for (const auto bone : bones)
			{
				if (boneIndex >= bones.size() - 1)
				{
					segments.push_back({ .01f });
					break;
				}
				const auto first = componentFromEntity<TransformComponent>(bone)->GetPosition();
				const auto second = componentFromEntity<TransformComponent>(bones[boneIndex + 1])->GetPosition();
				const auto firstPosition = XMLoadFloat3(&first);
				const auto secondPosition = XMLoadFloat3(&second);
				const auto length = wiMath::Distance(firstPosition, secondPosition);
				segments.push_back({ length });
				boneIndex++;
			}
			return segments;
		};

		int boneIndex = 0;
		for (auto bones : limbs)
		{
			XMFLOAT3 smoothTarget;
			XMStoreFloat3(&smoothTarget, XMVectorLerp(
				XMLoadFloat3(&previousTargets[boneIndex]),
				XMLoadFloat3(&targets[boneIndex]),
				clamp((time - targetTimes[boneIndex])* smoothSpeed, 0.0f, 1.0f)));
			auto relativeTarget = getRelativeTarget(smoothTarget, bones[0]);

			{
				const auto segments = getSegments(bones);
				const float targetDistance = XMVectorGetX(XMVector3Length(relativeTarget));
				float distanceTravelled = 0;
				int boneIndex = 0;
				for (const auto boneEnt : bones)
				{
					const auto bone = mutableComponentFromEntity<TransformComponent>(boneEnt);
					const auto rotation = min(max(distanceTravelled - targetDistance, 0.f), 1.f);
					XMVECTOR quat = XMQuaternionRotationRollPitchYaw(rotation * 60 * 3.14f / 180.f, 0, 0);
					XMStoreFloat4(&bone->rotation_local, quat);
					distanceTravelled += segments[boneIndex].length; 
					boneIndex++;
				}
			}

			{
				const Entity parentEnt = componentFromEntity<HierarchyComponent>(bones[0])->parentID;
				const XMMATRIX localToGlobal = localToGlobalMatrix(getAncestryForEntity(parentEnt));
				const XMMATRIX globalToLocal = XMMatrixInverse(nullptr, localToGlobal);
				const XMVECTOR localStartDirection{ 0, 1, 0 };
				const XMVECTOR localTargetDirection = XMVector3Normalize(XMVector4Transform(relativeTarget, globalToLocal));
				const XMVECTOR axis = XMVector3Normalize(XMVector3Cross(localStartDirection, localTargetDirection));
				const float angle = XMScalarACos(XMVectorGetX(XMVector3Dot(localStartDirection, localTargetDirection)));
				const XMVECTOR localRotationTowardsTarget = XMQuaternionRotationNormal(axis, angle);
				auto bone = mutableComponentFromEntity<TransformComponent>(bones[0]);
				XMStoreFloat4(&bone->rotation_local, localRotationTowardsTarget);
			}

			boneIndex++;
		}
	}

	void Update(float time)
	{
		Retargetting(time);
		BasicGrasp(time);
	}
};
