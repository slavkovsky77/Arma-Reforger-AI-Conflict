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

	// A way out, not only a straight one (a van steers round a hay dryer): best-first over a STEP_M grid within 'reach', from
	// the cell of 'from' to a cell within STEP_M of a road point (roads 2 m wide or more), each step a box sweep to one of the 8
	// neighbouring cells; at most MAX_CELLS cells. 1: a way found ('road' = the road point reached); 0: none ('road' = the
	// road point nearest the closest cell reached, 'blocker' = what stopped the last sweep from that cell); -1: no road within
	// 'reach'. 'cells' = cells reached, 'sweeps' = sweeps made.
	static const float STEP_M = 6;
	static const int MAX_CELLS = 250;

	// The wooden pole fences (PoleFence_01) don't count: the vans from the Farm's depot spot 2124,2948 drove straight through
	// that fence line to the road 47 m away, 6 of 6 (farm-depots sub-lab, 24 Oct); the wire fences (PoleFence_02), stone walls
	// and buildings do. Against the vans of the sub-lab: the spots 2292,2927 and 2294,2982 refused (2 of 9 and 0 of 9 vans
	// got out; the Farm's depot stood there in 8 of the last 13 Arland matches), 2124,2948 passed (6 of 6). The spot 2154,2909
	// passes, but its vans (0 of 9) drive to a gap in the walls instead of the way out 30 m south-west - the game's route,
	// not the ground. ~5-100 ms a spot.
	static const string PASSABLE = "PoleFence_01";

	static int WayOut(vector from, float reach, out vector road, out string blocker, out int cells, out int sweeps)
	{
		return WayOutWith(from, reach, STEP_M, PASSABLE, road, blocker, cells, sweeps);
	}

	// The same with another grid step, and objects whose prefab name contains 'ignore' left out (lab: which fences do vans
	// push through?).
	protected static string s_sIgnore;
	static float s_fHalfWidth = 1.1;   // lab: the box's half width (the van's 1.1, the game's BTR-sized vehicle navmesh wider)

	static int WayOutWith(vector from, float reach, float step, string ignore, out vector road, out string blocker, out int cells, out int sweeps)
	{
		s_sIgnore = ignore;
		array<vector> points = {};
		RoadPoints(from, reach, points);
		if (points.IsEmpty())
			return -1;
		BaseWorld world = GetGame().GetWorld();
		map<int, bool> seen = new map<int, bool>();
		array<int> openX = {0};
		array<int> openZ = {0};
		array<float> openH = {Nearest(from, points, road)};
		seen.Set(Key(0, 0), true);
		float bestH = openH[0];
		int maxCells = MAX_CELLS * (STEP_M / step) * (STEP_M / step);
		while (!openX.IsEmpty() && cells < maxCells)
		{
			int pick;
			for (int i = 1; i < openH.Count(); i++)
			{
				if (openH[i] < openH[pick])
					pick = i;
			}
			int cx = openX[pick];
			int cz = openZ[pick];
			float h = openH[pick];
			openX.Remove(pick);
			openZ.Remove(pick);
			openH.Remove(pick);
			cells++;
			vector at = from + Vector(cx * step, 0, cz * step);
			vector near;
			if (Nearest(at, points, near) <= Math.Max(step, 5))
			{
				road = near;
				return 1;
			}
			for (int dx = -1; dx <= 1; dx++)
			{
				for (int dz = -1; dz <= 1; dz++)
				{
					if (dx == 0 && dz == 0)
						continue;
					int nx = cx + dx;
					int nz = cz + dz;
					int key = Key(nx, nz);
					if (seen.Contains(key))
						continue;
					vector next = from + Vector(nx * step, 0, nz * step);
					if (vector.DistanceXZ(next, from) > reach)
						continue;
					sweeps++;
					IEntity hit;
					if (!Sweep(world, at, next, hit))
					{
						if (h <= bestH)
						{
							bestH = h;
							blocker = Describe(hit);
						}
						continue;
					}
					seen.Set(key, true);
					vector nearNext;
					float hn = Nearest(next, points, nearNext);
					if (hn < bestH)
					{
						bestH = hn;
						road = nearNext;
					}
					openX.Insert(nx);
					openZ.Insert(nz);
					openH.Insert(hn);
				}
			}
		}
		return 0;
	}

	protected static int Key(int x, int z)
	{
		return (x + 500) * 1000 + z + 500;
	}

	protected static float Nearest(vector p, array<vector> points, out vector nearest)
	{
		float best = 1e9;
		foreach (vector q : points)
		{
			float d = vector.DistanceXZ(p, q);
			if (d < best)
			{
				best = d;
				nearest = q;
			}
		}
		return best;
	}

	// The road points (roads 2 m wide or more) within 'reach' of 'from'.
	static void RoadPoints(vector from, float reach, notnull array<vector> points)
	{
		ChimeraAIWorld aiWorld = ChimeraAIWorld.Cast(GetGame().GetAIWorld());
		if (!aiWorld || !aiWorld.GetRoadNetworkManager())
			return;
		array<BaseRoad> found = {};
		aiWorld.GetRoadNetworkManager().GetRoadsInAABB(from - Vector(reach, 60, reach), from + Vector(reach, 60, reach), found);
		foreach (BaseRoad r : found)
		{
			if (!r || r.GetWidth() < 2)
				continue;
			array<vector> roadPoints = {};
			r.GetPoints(roadPoints);
			foreach (vector p : roadPoints)
			{
				if (vector.DistanceXZ(p, from) <= reach)
					points.Insert(p);
			}
		}
	}

	// A van-sized box (2.2 m wide, 0.3 to 1.8 m above the ground at both ends) from 'a' to 'b'; the terrain itself doesn't
	// count. True when it gets through; 'hit' = what stopped it.
	protected static bool Sweep(BaseWorld world, vector a, vector b, out IEntity hit)
	{
		TraceBox trace = new TraceBox();
		a[1] = world.GetSurfaceY(a[0], a[2]) + 1.2;
		b[1] = world.GetSurfaceY(b[0], b[2]) + 1.2;
		trace.Start = a;
		trace.End = b;
		trace.Mins = Vector(-s_fHalfWidth, -0.9, -s_fHalfWidth);
		trace.Maxs = Vector(s_fHalfWidth, 0.6, s_fHalfWidth);
		trace.Flags = TraceFlags.ENTS;
		trace.LayerMask = EPhysicsLayerPresets.Projectile;
		float reached = world.TraceMove(trace, Solid);
		hit = trace.TraceEnt;
		return reached >= 1;
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
		if (!s_sIgnore.IsEmpty() && entity.GetPrefabData() && entity.GetPrefabData().GetPrefabName().Contains(s_sIgnore))
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
