// ZCR (3 Nov, note 528 - Martin watching the Farm battle: "team a6, soviet one is stuck under the rock, we should maybe check
// that and maybe some unstacking, its a bug in the game"): soldiers wedged in rocks or props. In 53 Farm battles 16 teams on
// foot stood within 5 m from 2 min before their field hold to 3 min after and never moved again; A6 (4 men) stood at one
// rock for 12 minutes while AI Conflict rebuilt its order 5 times. AI Conflict's own AICF_IsolatedNavmeshRecovery takes only
// a lone man, out of combat, once a slot, and needs a clear path out - a wedged man has none.
// On each stuck detection (the group made no route progress for the stuck timeout), every living member on foot that has
// not moved 1.5 m since the group's previous detection hops 2-7 m to free ground the navmesh connects onward, the side
// towards its waypoint first - in combat too; once per member per stuck episode. Logs [ZCR][UNSTICK].
// -aicfZcrUnstick 0 turns it off.
class AICF_ZcrUnstick
{
	protected static ref map<EntityID, vector> s_mLast = new map<EntityID, vector>();
	protected static ref map<EntityID, int> s_mNudged = new map<EntityID, int>();

	static bool Enabled()
	{
		string value;
		return !(System.GetCLIParam("aicfZcrUnstick", value) && value == "0");
	}

	// No field hold in front of the objective (3 Nov, note 530 - Martin: "continue developing so farm is actually captured
	// and both sides actually attack and defend the point"). AI Conflict's stuck check reads "no route progress for the stuck
	// timeout, three times" as stuck and then parks the team for good on its own spot (FIELD_HOLD). Near its objective that
	// is a team fighting its way in: in 55 Farm battles 145 of 179 field holds were within 400 m of Farm - 2.6 attack teams a
	// battle frozen in front of it (no-drones battle, 3 Nov 18:22: Soviet A1, A2, A5 parked ~300 m south, Farm never
	// contested). An attack team within -aicfZcrNoHoldNearM (default 450 m; 0 = off) of its target base is never parked:
	// each stuck detection rebuilds its order instead (and wedged men hop free).
	static bool NearAttackTarget(AICF_GroupSlot slot)
	{
		if (!slot || slot.GetRole() != AICF_EGroupRole.ATTACK || !slot.GetTargetBase() || !slot.GetGroup())
			return false;
		float nearM = 450;
		string value;
		if (System.GetCLIParam("aicfZcrNoHoldNearM", value) && !value.IsEmpty())
			nearM = value.ToFloat();
		if (nearM <= 0)
			return false;
		IEntity leader = AICF_GroupRuntime.ResolveAliveLeader(slot.GetGroup());
		if (!leader || !slot.GetTargetBase().GetOwner())
			return false;
		return vector.DistanceXZ(leader.GetOrigin(), slot.GetTargetBase().GetOwner().GetOrigin()) <= nearM;
	}

	// A stuck attack team near its objective gets a new leg toward it (3 Nov, note 530). AI Conflict's leg is a random
	// reachable point up to 140 m away, the one ending nearest the objective but up to 80 m farther allowed - for a team
	// stopped 200 m out by a fight, a point beside it (Farm, 3 Nov 19:51: US A0's four legs ended 130-230 m from Farm, the
	// team 190 m out; 20 minutes, nobody in the farm). Within -aicfZcrNoHoldNearM of the target the leg ends up to 80 m
	// closer on the line to the objective (the objective itself inside 100 m), on the navmesh, the straight line first,
	// then 20 and 40 degrees off it. -aicfZcrTowardLeg 0 = AI Conflict's leg.
	static bool TowardObjective(AICF_GroupSlot slot, vector objective, out vector endpoint)
	{
		string value;
		if ((System.GetCLIParam("aicfZcrTowardLeg", value) && value == "0") || !NearAttackTarget(slot))
			return false;
		SCR_AIGroup group = slot.GetGroup();
		AIPathfindingComponent path = AIPathfindingComponent.Cast(group.FindComponent(AIPathfindingComponent));
		IEntity leader = AICF_GroupRuntime.ResolveAliveLeader(group);
		if (!path || !path.GetNavmeshComponent() || !leader)
			return false;
		NavmeshWorldComponent navmesh = path.GetNavmeshComponent();
		BaseWorld world = GetGame().GetWorld();
		vector origin = leader.GetOrigin();
		float dist = vector.DistanceXZ(origin, objective);
		float step = dist;
		if (dist > 100)
			step = Math.Min(80, dist - 60);
		float baseAngle = Math.Atan2(objective[2] - origin[2], objective[0] - origin[0]);
		for (int k; k < 5; k++)
		{
			// 0, +20, -20, +40, -40 degrees off the line to the objective
			float turn = ((k + 1) / 2) * 20;
			if (k % 2 == 0)
				turn = -turn;
			float angle = baseAngle + turn * Math.DEG2RAD;
			vector query = origin + Vector(Math.Cos(angle) * step, 0, Math.Sin(angle) * step);
			query[1] = world.GetSurfaceY(query[0], query[2]);
			vector spot;
			if (!navmesh.IsTileLoaded(query) || !path.GetClosestPositionOnNavmesh(query, "8 4 8", spot))
				continue;
			if (ChimeraWorldUtils.TryGetWaterSurfaceSimple(world, spot - "0 0.5 0"))
				continue;
			if (dist > 100 && vector.DistanceXZ(spot, objective) > dist - 25)
				continue;
			endpoint = spot;
			return true;
		}
		return false;
	}

	//! The members of slot's group that stood still since its previous stuck detection, moved; returns how many.
	static int TryNudge(AICF_GroupSlot slot, int episodeId)
	{
		if (!Replication.IsServer() || !slot || !Enabled())
			return 0;
		SCR_AIGroup group = slot.GetGroup();
		if (!group)
			return 0;
		AIPathfindingComponent path = AIPathfindingComponent.Cast(group.FindComponent(AIPathfindingComponent));
		if (!path || !path.GetNavmeshComponent())
			return 0;
		NavmeshWorldComponent navmesh = path.GetNavmeshComponent();
		vector toward = vector.Zero;
		bool hasToward;
		if (slot.GetWaypoint())
		{
			toward = slot.GetWaypoint().GetOrigin();
			hasToward = true;
		}
		array<AIAgent> agents = {};
		group.GetAgents(agents);
		int moved;
		foreach (AIAgent agent : agents)
		{
			if (!agent)
				continue;
			ChimeraCharacter character = ChimeraCharacter.Cast(agent.GetControlledEntity());
			if (!character || !AICF_GroupRuntime.IsAliveCharacter(character) || CompartmentAccessComponent.GetVehicleIn(character))
				continue;
			CompartmentAccessComponent access = CompartmentAccessComponent.Cast(character.FindComponent(CompartmentAccessComponent));
			if (access && (access.IsGettingIn() || access.IsGettingOut()))
				continue;
			EntityID id = character.GetID();
			vector now = character.GetOrigin();
			vector before;
			bool had = s_mLast.Find(id, before);
			s_mLast.Set(id, now);
			if (!had || vector.DistanceXZ(before, now) > 1.5)
				continue;
			// Only a man jammed into something (no room where he stands - a rock, a prop): one merely holding still in a fight
			// stays (the first no-drones battle: 2-4 m hops of attack teams standing 300 m from Farm, none wedged).
			if (RoomAt(GetGame().GetWorld(), character, now))
				continue;
			int done;
			if (s_mNudged.Find(id, done) && done == episodeId)
				continue;
			vector destination;
			bool clearPath;
			if (!FindSpot(character, path, navmesh, now, toward, hasToward, destination, clearPath))
			{
				Print(string.Format("[ZCR][UNSTICK] faction=%1 slot=%2 episode=%3 result=no_spot x=%4 z=%5", group.GetFaction().GetFactionKey(),
					slot.GetSlotId(), episodeId, Math.Round(now[0]), Math.Round(now[2])), LogLevel.NORMAL);
				s_mNudged.Set(id, episodeId);
				continue;
			}
			s_mNudged.Set(id, episodeId);
			vector transform[4];
			character.GetWorldTransform(transform);
			transform[3] = destination;
			character.Teleport(transform);
			moved++;
			Print(string.Format("[ZCR][UNSTICK] faction=%1 slot=%2 episode=%3 result=moved from=%4,%5 to=%6,%7 m=%8 clear_path=%9", group.GetFaction().GetFactionKey(),
				slot.GetSlotId(), episodeId, Math.Round(now[0]), Math.Round(now[2]), Math.Round(destination[0]), Math.Round(destination[2]),
				vector.DistanceXZ(now, destination).ToString(-1, 1), clearPath), LogLevel.NORMAL);
		}
		return moved;
	}

	//! Free ground 2-7 m from origin on the navmesh, connected onward (not the same little island), about the same height, not
	//! water, room for a man; a clear walk there first, else any such spot; the side towards the waypoint first.
	protected static bool FindSpot(ChimeraCharacter character, AIPathfindingComponent path, NavmeshWorldComponent navmesh, vector origin,
		vector toward, bool hasToward, out vector destination, out bool clearPath)
	{
		BaseWorld world = GetGame().GetWorld();
		float baseAngle = 0;
		if (hasToward && vector.DistanceXZ(origin, toward) > 1)
			baseAngle = Math.Atan2(toward[2] - origin[2], toward[0] - origin[0]);
		for (int pass; pass < 2; pass++)
		{
			for (int ring; ring < 3; ring++)
			{
				float radius = 2.5 + ring * 2;
				// 0, +45, -45, +90, -90, +135, -135, 180 from the waypoint's side
				for (int k; k < 8; k++)
				{
					float turn = ((k + 1) / 2) * 45;
					if (k % 2 == 0)
						turn = -turn;
					float angle = baseAngle + turn * Math.DEG2RAD;
					vector query = origin + Vector(Math.Cos(angle) * radius, 0, Math.Sin(angle) * radius);
					query[1] = world.GetSurfaceY(query[0], query[2]);
					vector spot;
					if (!navmesh.IsTileLoaded(query) || !path.GetClosestPositionOnNavmesh(query, "1 2 1", spot))
						continue;
					float d = vector.DistanceXZ(origin, spot);
					if (d < 2 || d > 7 || Math.AbsFloat(spot[1] - origin[1]) > 1.5 || ChimeraWorldUtils.TryGetWaterSurfaceSimple(world, spot))
						continue;
					vector onward;
					if (!navmesh.GetReachablePoint(spot, 40, onward) || vector.DistanceXZ(spot, onward) < 10)
						continue;
					if (!RoomAt(world, character, spot))
						continue;
					bool clear = PathClear(world, character, origin, spot);
					if (pass == 0 && !clear)
						continue;
					destination = spot;
					clearPath = clear;
					return true;
				}
			}
		}
		return false;
	}

	protected static bool RoomAt(BaseWorld world, IEntity character, vector spot)
	{
		TraceBox trace = new TraceBox();
		trace.Start = spot + "0 0.15 0";
		trace.End = trace.Start;
		trace.Mins = "-0.3 0 -0.3";
		trace.Maxs = "0.3 1.6 0.3";
		trace.Exclude = character;
		trace.Flags = TraceFlags.ENTS | TraceFlags.WORLD | TraceFlags.OCEAN;
		trace.LayerMask = EPhysicsLayerPresets.Projectile;
		return world.TracePosition(trace, null) >= 0;
	}

	protected static bool PathClear(BaseWorld world, IEntity character, vector origin, vector spot)
	{
		TraceBox trace = new TraceBox();
		trace.Start = origin + "0 0.15 0";
		trace.End = spot + "0 0.15 0";
		trace.Mins = "-0.3 0 -0.3";
		trace.Maxs = "0.3 1.6 0.3";
		trace.Exclude = character;
		trace.Flags = TraceFlags.ENTS | TraceFlags.WORLD | TraceFlags.OCEAN;
		trace.LayerMask = EPhysicsLayerPresets.Projectile;
		return world.TraceMove(trace, null) >= 1;
	}
}
