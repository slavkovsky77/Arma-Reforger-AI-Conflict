// ZCR (24 Oct, report 27 and review note 386 - Martin: "the lab should make infantry build supply points in accessible
// places, the AI sometimes places them directly on the road or blocking the base"): the Farm's light vehicle depot at
// (2124, 2948) - 2 of the last 13 Arland matches - stood in a pasture closed by low stone walls and a pole fence; its vans
// drove to the corner where two walls meet (the navmesh has a gap there no van fits through), their crews got out, and
// the Farm got no supply trip. A vehicle depot needs a straight drive to a road: a van-sized box swept from the depot's
// centre to the road points around it (points of roads 2 m wide or more within REACH_M, nearest first, 8 m apart, up to
// TRIES sweeps) has to get through on at least one of them without touching a wall, fence, building or tree.
// Off: -aicfDepotNoAccess.
class AICF_DepotAccess
{
	static const float REACH_M = 70;
	static const int TRIES = 12;

	static bool Enabled()
	{
		return !System.IsCLIParam("aicfDepotNoAccess");
	}

	// 1: a road point within 'reach' can be driven to straight from 'from' ('road' = that point); 0: none can ('road' = the
	// nearest one tried, 'blocker' = what stopped that sweep); -1: no road point within 'reach'. 'tried' = sweeps made.
	static int RoadReachable(vector from, float reach, out vector road, out string blocker, out int tried)
	{
		ChimeraAIWorld aiWorld = ChimeraAIWorld.Cast(GetGame().GetAIWorld());
		if (!aiWorld || !aiWorld.GetRoadNetworkManager())
			return -1;
		array<BaseRoad> found = {};
		aiWorld.GetRoadNetworkManager().GetRoadsInAABB(from - Vector(reach, 60, reach), from + Vector(reach, 60, reach), found);
		array<vector> points = {};
		array<float> dists = {};
		foreach (BaseRoad r : found)
		{
			if (!r || r.GetWidth() < 2)
				continue;
			array<vector> roadPoints = {};
			r.GetPoints(roadPoints);
			foreach (vector p : roadPoints)
			{
				float d = vector.DistanceXZ(p, from);
				if (d > reach)
					continue;
				int at = dists.Count();
				while (at > 0 && dists[at - 1] > d)
					at--;
				dists.InsertAt(d, at);
				points.InsertAt(p, at);
			}
		}
		if (points.IsEmpty())
			return -1;
		BaseWorld world = GetGame().GetWorld();
		array<vector> done = {};
		foreach (vector p : points)
		{
			if (tried >= TRIES)
				break;
			bool close;
			foreach (vector q : done)
			{
				if (vector.DistanceXZ(p, q) < 8)
				{
					close = true;
					break;
				}
			}
			if (close)
				continue;
			done.Insert(p);
			tried++;
			// The box: 2.2 m wide, from 0.3 to 1.8 m above the ground at both ends (low stone walls are ~0.8 m); the terrain
			// itself doesn't count (ENTS), so a rise between the two ends doesn't block.
			TraceBox trace = new TraceBox();
			vector a = from;
			a[1] = world.GetSurfaceY(a[0], a[2]) + 1.2;
			vector b = p;
			b[1] = world.GetSurfaceY(b[0], b[2]) + 1.2;
			trace.Start = a;
			trace.End = b;
			trace.Mins = Vector(-1.1, -0.9, -1.1);
			trace.Maxs = Vector(1.1, 0.6, 1.1);
			trace.Flags = TraceFlags.ENTS;
			trace.LayerMask = EPhysicsLayerPresets.Projectile;
			float reached = world.TraceMove(trace, Solid);
			if (reached >= 1)
			{
				road = p;
				return 1;
			}
			if (tried == 1)
			{
				road = p;
				blocker = Describe(trace.TraceEnt);
			}
		}
		return 0;
	}

	// Soldiers and vehicles move away; everything else (walls, fences, buildings, trees, rocks) counts.
	protected static bool Solid(IEntity entity)
	{
		if (!entity)
			return false;
		IEntity root = entity.GetRootParent();
		if (entity.IsInherited(ChimeraCharacter) || entity.IsInherited(Vehicle))
			return false;
		if (root && (root.IsInherited(ChimeraCharacter) || root.IsInherited(Vehicle)))
			return false;
		return true;
	}

	static string Describe(IEntity entity)
	{
		if (!entity)
			return "-";
		string name;
		if (entity.GetPrefabData())
			name = entity.GetPrefabData().GetPrefabName();
		if (name.IsEmpty() && entity.GetVObject())
			name = entity.GetVObject().GetResourceName();
		if (name.IsEmpty())
			name = entity.ClassName();
		int slash = name.LastIndexOf("/");
		if (slash >= 0)
			name = name.Substring(slash + 1, name.Length() - slash - 1);
		return name;
	}
}
