// ZCR (25 Oct - Martin: "supply lines should be planned up front... trucks follow precomputed routes"; review notes 394,
// 397, 398): the road network as a graph, and supply trucks sent along road routes. The game's road network has no links
// between roads, so the graph is ours: each road's points in order, each road's two ends joined to the nearest point of
// another road within JOIN_M (the junction pieces). A stretch costs its length times a factor for its road (main roads
// cheaper, farm tracks dearer); a point within TRAP_M of a road point a vehicle couldn't reach this match
// (AICF_BadRoadPoints) costs TRAP_COST more. A* from the road nearest the truck to the road nearest its endpoint.
// The truck gets the route's waypoints ahead of its logistics waypoint: the first FIRST_M along (onto the road at once),
// then where the route turns at a junction (TURN_DEG or more) and every STEP_M, each passed within 30 m. Headless test
// (ZeusCommanderReforger lab, arland-supply, 22 depot spots): 1 van lost against 14 with the game's planner alone, trips
// as fast where the route is the shortest road. Legs under MIN_LEG_M keep the game's planner. Off: -aicfNoRoadRoutes.
//   LOGISTICS_ROAD_GRAPH nodes= links= junctions= roads= ms=
//   LOGISTICS_ROAD_ROUTE slot= faction= length_m= straight_m= waypoints= start_road_m= end_road_m= expanded= ms=
class AICF_RoadGraph
{
	static const float CELL = 25;
	static const float JOIN_M = 12;
	static const float STEP_M = 300;
	static const float FIRST_M = 100;
	static const float TURN_DEG = 30;
	static const float MIN_LEG_M = 300;
	static const float TRAP_M = 20;
	static const float TRAP_COST = 150;
	static const int MAX_EXPANDED = 250000;
	protected static ref AICF_RoadGraph s_Instance;

	protected ref array<float> m_aX = {};
	protected ref array<float> m_aZ = {};
	protected ref array<float> m_aW = {};
	protected ref array<int> m_aRoad = {};
	protected ref array<ref array<int>> m_aNext = {};
	protected ref array<ref array<float>> m_aCost = {};
	protected ref map<int, ref array<int>> m_mGrid = new map<int, ref array<int>>();
	protected int m_iLinks;
	protected int m_iJunctions;

	static bool Enabled()
	{
		return !System.IsCLIParam("aicfNoRoadRoutes");
	}

	static AICF_RoadGraph Get()
	{
		if (!s_Instance)
		{
			s_Instance = new AICF_RoadGraph();
			s_Instance.Build();
		}
		return s_Instance;
	}

	protected static float Factor(float width)
	{
		if (width >= 6)
			return 0.85;          // main road
		if (width >= 4)
			return 1.0;
		if (width >= 2.5)
			return 1.35;          // farm and forest tracks
		return 1.0;               // the 1 m junction pieces
	}

	protected static int Key(int ix, int iz)
	{
		return (ix + 2000) * 10000 + iz + 2000;
	}

	protected void Build()
	{
		int started = System.GetTickCount();
		ChimeraAIWorld aiWorld = ChimeraAIWorld.Cast(GetGame().GetAIWorld());
		if (!aiWorld || !aiWorld.GetRoadNetworkManager())
			return;
		vector wmin, wmax;
		GetGame().GetWorld().GetBoundBox(wmin, wmax);
		map<string, bool> seen = new map<string, bool>();
		array<int> ends = {};
		int roads;
		for (float x = wmin[0]; x < wmax[0]; x += 512)
		{
			for (float z = wmin[2]; z < wmax[2]; z += 512)
			{
				array<BaseRoad> found = {};
				aiWorld.GetRoadNetworkManager().GetRoadsInAABB(Vector(x, -500, z), Vector(x + 512, 2000, z + 512), found);
				foreach (BaseRoad road : found)
				{
					if (!road)
						continue;
					array<vector> points = {};
					road.GetPoints(points);
					if (points.Count() < 2)
						continue;
					string key = string.Format("%1_%2_%3_%4", Math.Round(points[0][0] * 10), Math.Round(points[0][2] * 10), points.Count(), Math.Round(road.GetWidth() * 10));
					if (seen.Contains(key))
						continue;
					seen.Set(key, true);
					float f = Factor(road.GetWidth());
					int first = m_aX.Count();
					foreach (int i, vector p : points)
					{
						int n = Add(p[0], p[2], road.GetWidth(), roads);
						if (i > 0)
							Link(n - 1, n, f);
					}
					ends.Insert(first);
					ends.Insert(m_aX.Count() - 1);
					roads++;
				}
			}
		}
		foreach (int e : ends)
		{
			int best = -1;
			float bestM = JOIN_M;
			array<int> near = {};
			Near(m_aX[e], m_aZ[e], JOIN_M, near);
			foreach (int j : near)
			{
				if (m_aRoad[j] == m_aRoad[e])
					continue;
				float d = Math.Sqrt((m_aX[j] - m_aX[e]) * (m_aX[j] - m_aX[e]) + (m_aZ[j] - m_aZ[e]) * (m_aZ[j] - m_aZ[e]));
				if (d < bestM)
				{
					bestM = d;
					best = j;
				}
			}
			if (best >= 0)
			{
				Link(e, best, 1.0);
				m_iJunctions++;
			}
		}
		AICF_Stage3Diagnostics.Info("LOGISTICS_ROAD_GRAPH", string.Format("nodes=%1 links=%2 junctions=%3 roads=%4 ms=%5", m_aX.Count(), m_iLinks,
			m_iJunctions, roads, System.GetTickCount() - started));
	}

	protected int Add(float x, float z, float w, int road)
	{
		int n = m_aX.Count();
		m_aX.Insert(x);
		m_aZ.Insert(z);
		m_aW.Insert(w);
		m_aRoad.Insert(road);
		m_aNext.Insert(new array<int>());
		m_aCost.Insert(new array<float>());
		int key = Key(Math.Floor(x / CELL), Math.Floor(z / CELL));
		array<int> cell = m_mGrid.Get(key);
		if (!cell)
		{
			cell = new array<int>();
			m_mGrid.Set(key, cell);
		}
		cell.Insert(n);
		return n;
	}

	protected void Link(int a, int b, float f)
	{
		float d = Math.Sqrt((m_aX[a] - m_aX[b]) * (m_aX[a] - m_aX[b]) + (m_aZ[a] - m_aZ[b]) * (m_aZ[a] - m_aZ[b]));
		m_aNext[a].Insert(b);
		m_aCost[a].Insert(d * f);
		m_aNext[b].Insert(a);
		m_aCost[b].Insert(d * f);
		m_iLinks++;
	}

	protected void Near(float x, float z, float r, notnull array<int> into)
	{
		int k = Math.Ceil(r / CELL);
		int cx = Math.Floor(x / CELL);
		int cz = Math.Floor(z / CELL);
		for (int gx = cx - k; gx <= cx + k; gx++)
		{
			for (int gz = cz - k; gz <= cz + k; gz++)
			{
				array<int> cell = m_mGrid.Get(Key(gx, gz));
				if (cell)
					into.InsertAll(cell);
			}
		}
	}

	// The nearest point of a road at least minWidth wide within maxM; -1 without one. 'd' = its distance.
	int Nearest(vector p, float minWidth, float maxM, out float d)
	{
		array<int> near = {};
		Near(p[0], p[2], maxM, near);
		int best = -1;
		d = maxM;
		foreach (int j : near)
		{
			if (m_aW[j] < minWidth)
				continue;
			float m = Math.Sqrt((m_aX[j] - p[0]) * (m_aX[j] - p[0]) + (m_aZ[j] - p[2]) * (m_aZ[j] - p[2]));
			if (m < d)
			{
				d = m;
				best = j;
			}
		}
		return best;
	}

	protected float Penalty(int n)
	{
		if (AICF_BadRoadPoints.IsBad(Vector(m_aX[n], 0, m_aZ[n]), TRAP_M))
			return TRAP_COST;
		return 0;
	}

	// A* over the graph; the nodes from src to dst in 'path'. 'expanded' = nodes taken off the heap.
	bool Path(int src, int dst, notnull array<int> path, out int expanded)
	{
		map<int, float> g = new map<int, float>();
		map<int, int> prev = new map<int, int>();
		map<int, bool> closed = new map<int, bool>();
		array<int> heapNode = {};
		array<float> heapCost = {};
		g.Set(src, 0);
		Push(heapNode, heapCost, src, Heuristic(src, dst));
		while (!heapNode.IsEmpty() && expanded < MAX_EXPANDED)
		{
			float f;
			int u = Pop(heapNode, heapCost, f);
			if (closed.Contains(u))
				continue;
			closed.Set(u, true);
			expanded++;
			if (u == dst)
				break;
			float gu = g.Get(u);
			array<int> next = m_aNext[u];
			array<float> cost = m_aCost[u];
			foreach (int i, int v : next)
			{
				if (closed.Contains(v))
					continue;
				float gv = gu + cost[i] + Penalty(v);
				float old;
				if (g.Find(v, old) && old <= gv)
					continue;
				g.Set(v, gv);
				prev.Set(v, u);
				Push(heapNode, heapCost, v, gv + Heuristic(v, dst));
			}
		}
		if (!closed.Contains(dst))
			return false;
		array<int> back = {dst};
		int at = dst;
		while (at != src)
		{
			at = prev.Get(at);
			back.Insert(at);
		}
		for (int k = back.Count() - 1; k >= 0; k--)
			path.Insert(back[k]);
		return true;
	}

	protected float Heuristic(int a, int b)
	{
		return 0.85 * Math.Sqrt((m_aX[a] - m_aX[b]) * (m_aX[a] - m_aX[b]) + (m_aZ[a] - m_aZ[b]) * (m_aZ[a] - m_aZ[b]));
	}

	protected static void Push(array<int> hn, array<float> hc, int n, float c)
	{
		hn.Insert(n);
		hc.Insert(c);
		int i = hn.Count() - 1;
		while (i > 0)
		{
			int p = (i - 1) / 2;
			if (hc[p] <= hc[i])
				break;
			int tn = hn[p];
			float tc = hc[p];
			hn[p] = hn[i];
			hc[p] = hc[i];
			hn[i] = tn;
			hc[i] = tc;
			i = p;
		}
	}

	protected static int Pop(array<int> hn, array<float> hc, out float c)
	{
		int top = hn[0];
		c = hc[0];
		int last = hn.Count() - 1;
		hn[0] = hn[last];
		hc[0] = hc[last];
		hn.Remove(last);
		hc.Remove(last);
		int n = hn.Count();
		int i = 0;
		while (true)
		{
			int l = 2 * i + 1;
			int r = l + 1;
			int m = i;
			if (l < n && hc[l] < hc[m])
				m = l;
			if (r < n && hc[r] < hc[m])
				m = r;
			if (m == i)
				break;
			int tn = hn[m];
			float tc = hc[m];
			hn[m] = hn[i];
			hc[m] = hc[i];
			hn[i] = tn;
			hc[i] = tc;
			i = m;
		}
		return top;
	}

	protected float Heading(int a, int b)
	{
		return Math.Atan2(m_aX[b] - m_aX[a], m_aZ[b] - m_aZ[a]) * Math.RAD2DEG;
	}

	protected float Turn(array<int> nodes, int i)
	{
		int a = nodes[Math.Max(0, i - 3)];
		int b = nodes[Math.Min(nodes.Count() - 1, i + 3)];
		if (a == nodes[i] || b == nodes[i])
			return 0;
		float d = Math.AbsFloat(Heading(nodes[i], b) - Heading(a, nodes[i]));
		while (d >= 360)
			d -= 360;
		return Math.Min(d, 360 - d);
	}

	// The waypoints a truck gets along a route: FIRST_M along, then at turns and every STEP_M; none closer than 60 m.
	void Waypoints(array<int> nodes, notnull array<vector> into)
	{
		float run;
		for (int i = 1; i < nodes.Count(); i++)
		{
			int n = nodes[i];
			int p = nodes[i - 1];
			run += Math.Sqrt((m_aX[n] - m_aX[p]) * (m_aX[n] - m_aX[p]) + (m_aZ[n] - m_aZ[p]) * (m_aZ[n] - m_aZ[p]));
			bool turn = i + 1 < nodes.Count() && m_aRoad[nodes[i + 1]] != m_aRoad[n] && Turn(nodes, i) >= TURN_DEG;
			float step = STEP_M;
			if (into.IsEmpty())
				step = FIRST_M;
			if (run < step && !turn && i != nodes.Count() - 1)
				continue;
			vector w = Vector(m_aX[n], 0, m_aZ[n]);
			if (!into.IsEmpty() && vector.DistanceXZ(w, into[into.Count() - 1]) < 60 && i != nodes.Count() - 1)
				continue;
			into.Insert(w);
			run = 0;
		}
	}

	// A logistics truck's leg: the route's waypoints into its group ahead of its logistics waypoint (index 0..k-1), kept
	// on the worker so they go when its waypoint goes (AICF_VehicleTaskHandoff.ClearLogisticsWaypoint).
	static void RouteWorker(AICF_LogisticsWorker w, AIWaypoint waypoint)
	{
		if (!Enabled() || !w || !w.m_Group || !w.m_Vehicle || !waypoint)
			return;
		vector from = w.m_Vehicle.GetOrigin();
		vector to = waypoint.GetOrigin();
		float straight = vector.DistanceXZ(from, to);
		if (straight < MIN_LEG_M)
			return;
		int started = System.GetTickCount();
		AICF_RoadGraph graph = Get();
		float startM, endM;
		int src = graph.Nearest(from, 3, 300, startM);
		int dst = graph.Nearest(to, 3, 300, endM);
		if (src < 0 || dst < 0 || src == dst)
			return;
		array<int> nodes = {};
		int expanded;
		if (!graph.Path(src, dst, nodes, expanded))
		{
			AICF_Stage3Diagnostics.Info("LOGISTICS_ROAD_ROUTE", string.Format("slot=%1 none=1 expanded=%2 x=%3 z=%4", w.m_iSlot, expanded, Math.Round(from[0]),
				Math.Round(from[2])));
			return;
		}
		float length;
		for (int i = 1; i < nodes.Count(); i++)
			length += Math.Sqrt((graph.m_aX[nodes[i]] - graph.m_aX[nodes[i - 1]]) * (graph.m_aX[nodes[i]] - graph.m_aX[nodes[i - 1]]) +
				(graph.m_aZ[nodes[i]] - graph.m_aZ[nodes[i - 1]]) * (graph.m_aZ[nodes[i]] - graph.m_aZ[nodes[i - 1]]));
		array<vector> points = {};
		graph.Waypoints(nodes, points);
		BaseWorld world = GetGame().GetWorld();
		int index;
		foreach (vector p : points)
		{
			if (vector.DistanceXZ(p, to) < 40)
				continue;                      // the logistics waypoint itself follows
			EntitySpawnParams params = new EntitySpawnParams();
			params.TransformMode = ETransformMode.WORLD;
			params.Transform[3] = Vector(p[0], world.GetSurfaceY(p[0], p[2]), p[2]);
			AIWaypoint rw = AIWaypoint.Cast(GetGame().SpawnEntityPrefabEx("{750A8D1695BD6998}Prefabs/AI/Waypoints/AIWaypoint_Move.et", false, params: params));
			if (!rw)
				continue;
			rw.SetCompletionRadius(30);
			w.m_Group.AddWaypointAt(rw, index);
			index++;
			w.m_aRouteWaypoints.Insert(rw);
		}
		string faction = "?";
		if (w.m_Faction)
			faction = w.m_Faction.GetFactionKey();
		AICF_Stage3Diagnostics.Info("LOGISTICS_ROAD_ROUTE", string.Format("slot=%1 faction=%2 length_m=%3 straight_m=%4 waypoints=%5 start_road_m=%6 end_road_m=%7 expanded=%8 ms=%9",
			w.m_iSlot, faction, Math.Round(length), Math.Round(straight), index, Math.Round(startM), Math.Round(endM), expanded, System.GetTickCount() - started));
	}

	// The route's waypoints off the truck's group (its leg ended, was cancelled or replaced).
	static void Unroute(AICF_LogisticsWorker w)
	{
		if (!w)
			return;
		foreach (AIWaypoint rw : w.m_aRouteWaypoints)
		{
			if (!rw)
				continue;
			if (w.m_Group)
				w.m_Group.RemoveWaypoint(rw);
			RplComponent.DeleteRplEntity(rw, false);
		}
		w.m_aRouteWaypoints.Clear();
	}
}
