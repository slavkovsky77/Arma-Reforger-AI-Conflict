// Геометрия подхода к BASE. Entity и waypoint lifecycle остаются у planner/slot.
class AICF_InfantryApproachRoute
{
	protected SCR_CampaignMilitaryBaseComponent m_Target;
	protected EntityID m_TargetId;
	protected vector m_vOrigin;
	protected bool m_bDisabled;
	protected int m_iTileWaitStartedAtMs;
	protected string m_sSide;          // ZCR: the marching group's side, for its kill zones
	protected vector m_vTileCandidate;
	protected int m_iLastCover = -1;   // ZCR: the cover score of the last leg's end point (cover= on INFANTRY_APPROACH_ASSIGNED)
	protected bool m_bZcrHolding;      // ZCR: waiting at the attack plan's hold line (AICF_ZcrAttackPlan)
	protected string m_sZcrHoldKey;    // ZCR: ... of that plan (several plans run at once in the normal matches)

	int GetLastCover()
	{
		return m_iLastCover;
	}

	bool ZcrHolding()
	{
		return m_bZcrHolding;
	}

	bool CanResumeTileWait(SCR_AIGroup group)
	{
		// ZCR: a team at the hold line goes on when its plan's teams go.
		if (m_bZcrHolding)
			return AICF_ZcrAttackPlan.Go(m_sZcrHoldKey);
		if (!IsWaitingForTile() || !group) return false;
		AIPathfindingComponent pathfinding = AIPathfindingComponent.Cast(group.FindComponent(AIPathfindingComponent));
		if (!pathfinding) return false;
		NavmeshWorldComponent navmesh = pathfinding.GetNavmeshComponent();
		return navmesh && (navmesh.IsTileLoaded(m_vTileCandidate) || System.GetTickCount(m_iTileWaitStartedAtMs) >= 30000);
	}

	void AICF_InfantryApproachRoute(SCR_CampaignMilitaryBaseComponent target, vector origin)
	{
		m_Target = target;
		m_TargetId = target.GetOwner().GetID();
		m_vOrigin = origin;
	}

	bool IsFor(SCR_CampaignMilitaryBaseComponent target)
	{
		return target && target == m_Target && target.GetOwner() && target.GetOwner().GetID() == m_TargetId;
	}

	void Disable()
	{
		m_bDisabled = true;
	}

	bool IsWaitingForTile()
	{
		return !m_bDisabled && m_iTileWaitStartedAtMs > 0;
	}

	// Чередование левого/правого фланга не зависит от role-local имени и tick.
	static float GetLaneOffset(int numericSlot)
	{
		int pair = numericSlot / 2;
		float offset = 40.0 + pair * 40.0;
		if (numericSlot % 2 == 0)
			return -offset;
		return offset;
	}

	bool BuildCandidate(vector leaderPosition, vector objective, int numericSlot, out vector candidate)
	{
		candidate = objective;
		// ZCR, 29 Oct: the team's attack plan read from the map (AICF_ZcrAttackPlan, lab/attackplan.py) - its lanes; the plan
		// from the origin nearest where this approach started.
		AICF_ZcrPlan plan = AICF_ZcrAttackPlan.ForTeam(m_sSide, numericSlot, objective, m_vOrigin);
		if (plan && !m_bDisabled && numericSlot >= 0)
			return PlanCandidate(plan, leaderPosition, objective, numericSlot, candidate);
		if (m_bDisabled || numericSlot < 0 || vector.DistanceXZ(leaderPosition, objective) <= 250.0)
			return false;
		vector direction = objective - m_vOrigin;
		direction[1] = 0;
		float length = direction.Length();
		if (length <= 300.0)
			return false;
		direction = direction / length;
		vector fromOrigin = leaderPosition - m_vOrigin;
		float progress = fromOrigin[0] * direction[0] + fromOrigin[2] * direction[2];
		if (progress >= length - 250.0)
			return false;
		float nextProgress = Math.Min(length - 180.0, Math.Max(0, progress) + StepM());
		vector lateral = Vector(-direction[2], 0, direction[0]);
		float offset = GetLaneOffset(numericSlot);
		float maxOffset = Math.Min(200.0, length * 0.2);
		offset = Math.Clamp(offset, -maxOffset, maxOffset);
		candidate = m_vOrigin + direction * nextProgress + lateral * offset;
		candidate = TowardCover(candidate, direction, lateral, leaderPosition);
		return vector.DistanceXZ(candidate, objective) + 40.0 < vector.DistanceXZ(leaderPosition, objective);
	}

	// The next leg on the team's lane of the plan (team N on lane N mod lanes): far from the lane and farther out than its
	// start - to the start (the assembly point); on it - the first lane point 120 m+ along that is 40 m+ nearer the
	// objective; within 80 m of the objective at the lane's end - straight in (false).
	protected bool PlanCandidate(AICF_ZcrPlan plan, vector leader, vector objective, int numericSlot, out vector candidate)
	{
		candidate = objective;
		m_bZcrHolding = false;
		if (plan.m_aLanes.IsEmpty())
			return false;
		AICF_ZcrPlanLane lane = plan.m_aLanes[numericSlot % plan.m_aLanes.Count()];
		int n = lane.m_aPoints.Count();
		if (n < 2)
			return false;
		// The hold line: at it and the side not gone yet - wait there (a timed Defend leg, see AICF_OrderPlanner).
		m_sZcrHoldKey = AICF_ZcrAttackPlan.HoldKey(m_sSide, plan);
		bool go = AICF_ZcrAttackPlan.Go(m_sZcrHoldKey);
		if (!go && vector.DistanceXZ(leader, lane.m_vHold) <= 35)
		{
			AICF_ZcrAttackPlan.AtHold(m_sZcrHoldKey, numericSlot);
			m_bZcrHolding = true;
			candidate = lane.m_vHold;
			return true;
		}
		float toObjective = vector.DistanceXZ(leader, objective);
		int nearest;
		int holdIndex;
		float nearestM = 1e9;
		float holdM = 1e9;
		for (int i; i < n; i++)
		{
			float d = vector.DistanceXZ(lane.m_aPoints[i], leader);
			if (d < nearestM)
			{
				nearestM = d;
				nearest = i;
			}
			float h = vector.DistanceXZ(lane.m_aPoints[i], lane.m_vHold);
			if (h < holdM)
			{
				holdM = h;
				holdIndex = i;
			}
		}
		if (nearestM > 150)
		{
			if (toObjective > vector.DistanceXZ(lane.m_aPoints[0], objective) + 50)
			{
				candidate = lane.m_aPoints[0];
				return true;
			}
			return false;
		}
		float along;
		for (int j = nearest + 1; j < n; j++)
		{
			along += vector.DistanceXZ(lane.m_aPoints[j - 1], lane.m_aPoints[j]);
			// The side not gone yet: no leg past the hold line - it ends there.
			if (!go && j >= holdIndex && nearest < holdIndex)
			{
				candidate = lane.m_aPoints[holdIndex];
				return true;
			}
			if ((along >= 120 && vector.DistanceXZ(lane.m_aPoints[j], objective) + 40 < toObjective) || j == n - 1)
			{
				if (j == n - 1 && toObjective <= 80)
					return false;
				candidate = lane.m_aPoints[j];
				return true;
			}
		}
		return false;
	}

	// ZCR, 19 Oct (Martin: "they just walk to the points through no cover"): each step of the march
	// ends at the best cover near its lane point - under trees or a roof, beside a building,
	// a wall or a rock - so a team goes from tree line to tree line instead of stopping in a field.
	// No cover there: the lane point as before.
	// 21 Oct (Martin's /bad: "us team a8 is exposed as hell" - A8 crossed an open hillside on a 220-m leg): legs of
	// StepM() (150 m, -aicfApproachStepM), end points searched 105 m to either side, and scored by the cover there
	// (x3) plus how much of the way to it runs under trees or a roof (4 points on the line from the leader).
	protected static ref TraceParam s_CoverTrace;

	static float StepM()
	{
		string value;
		if (System.GetCLIParam("aicfApproachStepM", value) && !value.IsEmpty())
			return Math.Clamp(value.ToFloat(), 80, 300);
		return 150;
	}

	protected vector TowardCover(vector point, vector direction, vector lateral, vector leader)
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return point;
		vector best = point;
		int bestScore = 0;
		float bestDistance = 1e9;
		array<float> along = {0, -30, 30};
		array<float> across = {0, -35, 35, -70, 70, -105, 105};
		foreach (float a : along)
		{
			foreach (float b : across)
			{
				vector p = point + direction * a + lateral * b;
				p[1] = world.GetSurfaceY(p[0], p[2]);
				if (ChimeraWorldUtils.TryGetWaterSurfaceSimple(world, p - "0 0.5 0"))
					continue;
				// ZCR: never into the side's own kill zone (AICF_DangerZones).
				if (!m_sSide.IsEmpty() && AICF_DangerZones.Near(p, m_sSide))
					continue;
				int score = CoverScore(world, p) * 3 + RouteCover(world, leader, p);
				float d = vector.DistanceXZ(p, point);
				if (score > bestScore || (score == bestScore && score > 0 && d < bestDistance))
				{
					best = p;
					bestScore = score;
					bestDistance = d;
				}
			}
		}
		m_iLastCover = bestScore;
		if (bestScore == 0 && !m_sSide.IsEmpty() && AICF_DangerZones.Near(point, m_sSide))
		{
			foreach (float side : {80.0, -80.0, 120.0, -120.0})
			{
				vector q = point + lateral * side;
				q[1] = world.GetSurfaceY(q[0], q[2]);
				if (!AICF_DangerZones.Near(q, m_sSide) && !ChimeraWorldUtils.TryGetWaterSurfaceSimple(world, q - "0 0.5 0"))
					return q;
			}
		}
		return best;
	}

	// How many of 4 points on the way from the leader to p have something overhead (a tree crown, a roof).
	protected static int RouteCover(BaseWorld world, vector from, vector to)
	{
		if (!s_CoverTrace)
			s_CoverTrace = new TraceParam();
		s_CoverTrace.Flags = TraceFlags.ENTS | TraceFlags.WORLD;
		s_CoverTrace.TargetLayers = EPhysicsLayerDefs.ViewGeometry | EPhysicsLayerDefs.Foliage | EPhysicsLayerDefs.FireGeometry;
		s_CoverTrace.Exclude = null;
		int covered;
		for (int i = 1; i <= 4; i++)
		{
			vector q = from + (to - from) * (i * 0.2);
			q[1] = world.GetSurfaceY(q[0], q[2]);
			s_CoverTrace.Start = q + "0 1.5 0";
			s_CoverTrace.End = q + "0 16 0";
			if (world.TraceMove(s_CoverTrace, null) < 1.0)
				covered++;
		}
		return covered;
	}

	// 2 for something overhead (a tree crown, a roof), 1 for something solid within 5 m around at
	// chest height on at least two sides (a building, a wall, rocks, trunks, a crest).
	static int CoverScore(BaseWorld world, vector p)
	{
		if (!s_CoverTrace)
			s_CoverTrace = new TraceParam();
		s_CoverTrace.Flags = TraceFlags.ENTS | TraceFlags.WORLD;
		s_CoverTrace.TargetLayers = EPhysicsLayerDefs.ViewGeometry | EPhysicsLayerDefs.Foliage | EPhysicsLayerDefs.FireGeometry;
		s_CoverTrace.Exclude = null;
		int score;
		s_CoverTrace.Start = p + "0 1.5 0";
		s_CoverTrace.End = p + "0 16 0";
		if (world.TraceMove(s_CoverTrace, null) < 1.0)
			score += 2;
		int sides;
		for (int k; k < 8; k++)
		{
			float r = k * 45 * Math.DEG2RAD;
			s_CoverTrace.Start = p + "0 1 0";
			s_CoverTrace.End = p + Vector(Math.Cos(r) * 5, 1, Math.Sin(r) * 5);
			if (world.TraceMove(s_CoverTrace, null) < 1.0)
				sides++;
		}
		if (sides >= 2)
			score += 1;
		return score;
	}

	bool TryResolve(SCR_AIGroup group, int numericSlot, vector objective, out vector endpoint, out bool pending)
	{
		pending = false;
		endpoint = objective;
		IEntity leader = AICF_GroupRuntime.ResolveAliveLeader(group);
		if (group && group.GetFaction())
			m_sSide = group.GetFaction().GetFactionKey();
		vector candidate;
		if (!leader || !BuildCandidate(leader.GetOrigin(), objective, numericSlot, candidate))
			return false;
		AIPathfindingComponent pathfinding = AIPathfindingComponent.Cast(group.FindComponent(AIPathfindingComponent));
		if (!pathfinding)
			return false;
		NavmeshWorldComponent navmesh = pathfinding.GetNavmeshComponent();
		BaseWorld world = GetGame().GetWorld();
		if (!navmesh || !world)
			return false;
		candidate[1] = world.GetSurfaceY(candidate[0], candidate[2]);
		bool waitingForTile;
		if (!navmesh.IsTileLoaded(candidate))
		{
			m_vTileCandidate = candidate;
			if (m_iTileWaitStartedAtMs == 0)
				m_iTileWaitStartedAtMs = System.GetTickCount();
			if (System.GetTickCount(m_iTileWaitStartedAtMs) >= 30000)
			{
				Disable();
				return false;
			}
			if (!navmesh.IsTileRequested(candidate) && !navmesh.LoadTileIn(candidate))
				return false;
			// Пока грузится следующая полоса, держим действующий Move на проверенной
			// navmesh у живого leader. READY slot не остаётся без meaningful task.
			waitingForTile = true;
			candidate = leader.GetOrigin();
			candidate[1] = world.GetSurfaceY(candidate[0], candidate[2]);
			if (!navmesh.IsTileLoaded(candidate))
			{
				pending = true;
				return false;
			}
		}
		else
			m_iTileWaitStartedAtMs = 0;
		// Проекция не должна схлопывать соседние полосы в одну общую точку.
		if (!pathfinding.GetClosestPositionOnNavmesh(candidate, "8 5 8", endpoint) ||
			vector.DistanceSqXZ(candidate, endpoint) > 64.0 ||
			Math.AbsFloat(endpoint[1] - candidate[1]) > 3.0 ||
			ChimeraWorldUtils.TryGetWaterSurfaceSimple(world, endpoint - "0 0.5 0") ||
			(!waitingForTile && !m_bZcrHolding && vector.DistanceXZ(endpoint, objective) + 30.0 >= vector.DistanceXZ(leader.GetOrigin(), objective)))
		{
			Disable();
			AICF_Stage2Diagnostics.Info("INFANTRY_APPROACH_FALLBACK",
				string.Format("numeric_slot=%1 target=%2 reason=NO_SAFE_LANE_ENDPOINT", numericSlot, AICF_Stage1Diagnostics.BaseKey(m_Target)));
			return false;
		}
		return true;
	}
}
