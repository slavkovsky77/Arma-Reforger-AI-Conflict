// Construction-only factory for transient vehicle waypoints. Queue ownership
// and deletion belong exclusively to AICF_VehicleTaskHandoff.
class AICF_VehicleWaypointFactory
{
	protected static const ResourceName MOVE_WAYPOINT_PREFAB = "{750A8D1695BD6998}Prefabs/AI/Waypoints/AIWaypoint_Move.et";
	protected static const ResourceName GET_OUT_WAYPOINT_PREFAB = "{C40316EE26846CAB}Prefabs/AI/Waypoints/AIWaypoint_GetOut.et";
	protected static const float DISMOUNT_COMPLETION_RADIUS_METERS = 12.0;
	protected static const float MOVE_COMPLETION_RADIUS_METERS = 25.0;
	protected static const float MINIMUM_STAGING_COMPLETION_RADIUS_METERS = 5.0;

	AIWaypoint CreateSpawnStagingWaypoint(
		vector stagingPosition,
		float completionRadiusMeters)
	{
		if (completionRadiusMeters < MINIMUM_STAGING_COMPLETION_RADIUS_METERS)
			completionRadiusMeters = MINIMUM_STAGING_COMPLETION_RADIUS_METERS;
		AIWaypoint waypoint = SpawnWaypoint(MOVE_WAYPOINT_PREFAB, stagingPosition);
		if (waypoint)
		{
			waypoint.SetCompletionRadius(completionRadiusMeters);
			// The stock Move prefab may complete when the first member arrives.
			// Staging is an all-living-members contract, so the engine waypoint and
			// the explicit spawn proof must use the same completion semantics.
			waypoint.SetCompletionType(EAIWaypointCompletionType.All);
		}
		return waypoint;
	}

	AIWaypoint CreateMoveWaypoint(
		vector fromPosition,
		vector targetPosition,
		float targetRangeMeters,
		out vector resolvedPosition,
		out string routeMode)
	{
		resolvedPosition = targetPosition;
		routeMode = "DIRECT_NO_AI_WORLD";
		if (targetRangeMeters < MOVE_COMPLETION_RADIUS_METERS)
			targetRangeMeters = MOVE_COMPLETION_RADIUS_METERS;

		SCR_AIWorld aiWorld = SCR_AIWorld.Cast(GetGame().GetAIWorld());
		if (aiWorld)
		{
			RoadNetworkManager roadNetworkManager = aiWorld.GetRoadNetworkManager();
			if (roadNetworkManager)
			{
				vector roadPosition;
				if (roadNetworkManager.GetReachableWaypointInRoad(fromPosition, targetPosition, targetRangeMeters, roadPosition))
				{
					resolvedPosition = roadPosition;
					routeMode = "ROAD_REACHABLE";
					// ZCR (22 Oct): a road point a vehicle couldn't reach earlier this match (a farm road behind fences and a
					// gate - Everon (5067,5413), match after match): one on the approach side instead.
					if (AICF_BadRoadPoints.IsBad(roadPosition, 30))
					{
						vector back = fromPosition - targetPosition;
						back[1] = 0;
						vector alternative;
						if (back.Length() > 1)
						{
							back.Normalize();
							if (roadNetworkManager.GetReachableWaypointInRoad(fromPosition, targetPosition + back * 80, targetRangeMeters + 40, alternative) &&
								!AICF_BadRoadPoints.IsBad(alternative, 30))
							{
								resolvedPosition = alternative;
								routeMode = "ROAD_REACHABLE_AVOIDED";
							}
						}
					}
				}
				else
				{
					routeMode = "DIRECT_NO_REACHABLE_ROAD";
				}
			}
			else
			{
				routeMode = "DIRECT_NO_ROAD_NETWORK";
			}
		}

		AIWaypoint waypoint = SpawnWaypoint(MOVE_WAYPOINT_PREFAB, resolvedPosition);
		if (waypoint)
		{
			float completionRadius = targetRangeMeters - vector.Distance(resolvedPosition, targetPosition);
			if (completionRadius < 1.0)
				completionRadius = 1.0;
			waypoint.SetCompletionRadius(completionRadius);
		}
		return waypoint;
	}

	SCR_BoardingWaypoint CreateDismountWaypoint(Vehicle vehicle)
	{
		if (!vehicle)
			return null;

		SCR_BoardingWaypoint waypoint = SCR_BoardingWaypoint.Cast(SpawnWaypoint(GET_OUT_WAYPOINT_PREFAB, vehicle.GetOrigin()));
		if (!waypoint)
			return null;

		waypoint.SetAllowance(true, true, true);
		waypoint.SetCompletionRadius(DISMOUNT_COMPLETION_RADIUS_METERS);
		return waypoint;
	}

	protected AIWaypoint SpawnWaypoint(ResourceName prefab, vector position)
	{
		Resource resource = Resource.Load(prefab);
		if (!resource || !resource.IsValid())
		{
			AICF_Stage3Diagnostics.Error("VEHICLE_WAYPOINT_PREFAB_INVALID", string.Format("prefab=%1", prefab));
			return null;
		}

		position[1] = GetGame().GetWorld().GetSurfaceY(position[0], position[2]);
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = position;
		AIWaypoint waypoint = AIWaypoint.Cast(GetGame().SpawnEntityPrefabEx(prefab, false, params: params));
		if (!waypoint)
			AICF_Stage3Diagnostics.Error("VEHICLE_WAYPOINT_SPAWN_FAILED", string.Format("prefab=%1", prefab));
		return waypoint;
	}

	// Вспомогательные physical logistics jobs того же domain owner.

	AIWaypoint CreateLogisticsWaypoint(vector endpoint)
	{
		AIWaypoint waypoint = SpawnWaypoint(MOVE_WAYPOINT_PREFAB, endpoint);
		// ZCR (22 Oct): 8 m (was 3) - a truck that drove past a 3-m point (its depot's exit 12 m ahead, most often) had to
		// turn back to it, and the game's driver can't reach a point behind it: UNREACHABLE, the driver gets out
		// (Everon w13: 13 of 23 failed truck moves). Arrival at a load/unload point is still the worker's 15 m.
		if (waypoint) waypoint.SetCompletionRadius(8);
		return waypoint;
	}

}

// ZCR (22 Oct): road points a vehicle's move couldn't reach this match (the game's UNREACHABLE - ZCR's OnMoveFailed hook
// adds them). The same few points failed match after match on Everon: farm roads inside fenced yards.
class AICF_BadRoadPoints
{
	protected static ref array<vector> s_aPoints = {};

	static void Add(vector point)
	{
		if (IsBad(point, 5))
			return;
		s_aPoints.Insert(point);
		AICF_Stage3Diagnostics.Info("VEHICLE_ROAD_POINT_AVOIDED", string.Format("x=%1 z=%2 count=%3", Math.Round(point[0]), Math.Round(point[2]), s_aPoints.Count()));
	}

	static bool IsBad(vector point, float radius)
	{
		foreach (vector bad : s_aPoints)
		{
			if (vector.DistanceXZ(bad, point) < radius)
				return true;
		}
		return false;
	}
}
