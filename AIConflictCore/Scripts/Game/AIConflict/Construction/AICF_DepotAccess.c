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

	// ZCR (2 Nov, Martin: "it's a bit laggy, like lag comes every nth second"): the way out above ran whole in one frame -
	// up to 250 cells and ~2000 box sweeps, ~80 ms a failed spot on average and up to ~250 ms - and a depot search tried
	// 48-191 spots, one every 0.6-3 s (the planner ticks every 250 ms). In Martin's solo match (2 Nov 18:25) 21 of the 22
	// ten-second windows with a frame of 200 ms or more were depot searches (US heavy depot at 18:54: 103 failed way outs
	// in 58 s, 11.4 s of CPU, FPS 28-37); the watched Farm test after it the same (24 of 25). The construction planner spent
	// 92 s of CPU in that 31-minute match, its worst update 257 ms. Now:
	// - one world-aligned STEP_M grid for every check, each sweep between two cells kept EDGE_TTL_S (the spots of one
	//   base are 10-50 m apart, so their 70 m way outs cross the same cells: a sweep is made once, not once a spot);
	// - each cell's distance to the nearest road kept for the match (roads don't move);
	// - WayOutSliced does at most FRAME_BUDGET_MS of new work a frame and answers PENDING until the spot's check is done
	//   (AICF_ConstructionSiteSearch.DepotRoadsOk lets the candidate wait, as for the query budget); a spot's answer is kept
	//   RESULT_TTL_MS, so its second and third live check (terrain pass, commit) don't redo it;
	// - a way found is checked again along its path when a sweep on it is older than FRESH_S (a building placed since).
	// [AICF][STAGE1][INFO][DEPOT_ACCESS_COST] once a minute (checks, sweeps, cache use, CPU, the dearest call).
	static const int PENDING = 2;
	static const int FRAME_BUDGET_MS = 5;
	static const int EDGE_TTL_S = 180;
	static const int FRESH_S = 30;
	static const int RESULT_TTL_MS = 60000;
	static const int SLOW_CALL_MS = 30;

	static int WayOut(vector from, float reach, out vector road, out string blocker, out int cells, out int sweeps)
	{
		// The lab's synchronous callers (ZCR_DriveTest): the whole check now, from the shared caches.
		if (s_fHalfWidth != 1.1)
			return WayOutWith(from, reach, STEP_M, PASSABLE, road, blocker, cells, sweeps);
		return Run(from, reach, -1, road, blocker, cells, sweeps);
	}

	// The construction planner's check: PENDING while the spot's way out is still being worked out (a few ms a frame).
	static int WayOutSliced(vector from, float reach, out vector road, out string blocker, out int cells, out int sweeps)
	{
		if (s_fHalfWidth != 1.1)
			return WayOutWith(from, reach, STEP_M, PASSABLE, road, blocker, cells, sweeps);
		return Run(from, reach, FRAME_BUDGET_MS, road, blocker, cells, sweeps);
	}

	// For the planner's slow-update line: what the way-out checks did in this frame.
	static string FrameReport()
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world || world.GetWorldTime() != s_fFrameTime)
			return "way_out_ms=0 way_out_sweeps=0 way_out_calls=0";
		return string.Format("way_out_ms=%1 way_out_sweeps=%2 way_out_calls=%3", s_iFrameMs, s_iFrameSweeps, s_iFrameCalls);
	}

	protected static ref map<int, int> s_mEdge = new map<int, int>();             // edge key -> stamp_s * 4 + 1 open / 2 blocked
	protected static ref map<int, string> s_mEdgeBlocker = new map<int, string>();
	protected static ref map<int, float> s_mRoadDist = new map<int, float>();     // cell key -> metres to the nearest road point
	protected static ref map<int, vector> s_mRoadNear = new map<int, vector>();
	protected static ref map<int, ref AICF_DepotWayOutJob> s_mJobs = new map<int, ref AICF_DepotWayOutJob>();
	protected static BaseWorld s_World;
	protected static float s_fFrameTime = -1;
	protected static int s_iFrameMs;
	protected static int s_iFrameSweeps;
	protected static int s_iFrameCalls;
	// The minute's totals for DEPOT_ACCESS_COST.
	protected static int s_iReportAt;
	protected static int s_iCalls;
	protected static int s_iPendingCalls;
	protected static int s_iCachedAnswers;
	protected static int s_iChecksDone;
	protected static int s_iChecksOpen;
	protected static int s_iSweeps;
	protected static int s_iCachedEdges;
	protected static int s_iRechecks;
	protected static int s_iCpuMs;
	protected static int s_iMaxCallMs;
	protected static int s_iMaxCheckMs;
	protected static int s_iMaxCheckFrames;

	protected static int Run(vector from, float reach, int budgetMs, out vector road, out string blocker, out int cells, out int sweeps)
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return -1;
		if (world != s_World)
		{
			s_World = world;
			s_mEdge.Clear();
			s_mEdgeBlocker.Clear();
			s_mRoadDist.Clear();
			s_mRoadNear.Clear();
			s_mJobs.Clear();
		}
		int started = System.GetTickCount();
		float frameTime = world.GetWorldTime();
		if (frameTime != s_fFrameTime)
		{
			s_fFrameTime = frameTime;
			s_iFrameMs = 0;
			s_iFrameSweeps = 0;
			s_iFrameCalls = 0;
		}
		s_iFrameCalls++;
		s_iCalls++;
		int jobKey = (int)Math.Round(from[0]) * 32768 + (int)Math.Round(from[2]);
		AICF_DepotWayOutJob job = s_mJobs.Get(jobKey);
		if (job && (vector.DistanceXZ(job.m_vFrom, from) > 0.5 || job.m_fReach != reach))
			job = null;
		if (job && job.m_iResult != -2 && started - job.m_iDoneAt > RESULT_TTL_MS)
			job = null;
		if (job && job.m_iResult != -2)
		{
			s_iCachedAnswers++;
			road = job.m_vRoad;
			blocker = job.m_sBlocker;
			cells = job.m_iCells;
			sweeps = job.m_iSweeps;
			Report(started);
			return job.m_iResult;
		}
		if (budgetMs >= 0 && s_iFrameMs >= budgetMs)
		{
			s_iPendingCalls++;
			Report(started);
			return PENDING;
		}
		if (!job)
		{
			if (s_mJobs.Count() > 256)
				Forget(started);
			job = new AICF_DepotWayOutJob();
			job.m_vFrom = from;
			job.m_fReach = reach;
			RoadPoints(from, reach + STEP_M + 2, job.m_aPoints);
			if (job.m_aPoints.IsEmpty())
			{
				job.m_iResult = -1;
				job.m_iDoneAt = started;
			}
			else
				Begin(job);
			s_mJobs.Set(jobKey, job);
		}
		job.m_iTouchedAt = started;
		int callBudget = -1;
		if (budgetMs >= 0)
			callBudget = budgetMs - s_iFrameMs;
		int sweepsBefore = job.m_iSweeps;
		int result = job.m_iResult;
		if (result == -2)
			result = Advance(job, world, started, callBudget);
		int took = System.GetTickCount() - started;
		s_iFrameMs += took;
		s_iFrameSweeps += job.m_iSweeps - sweepsBefore;
		job.m_iCpuMs += took;
		job.m_iFrames++;
		s_iCpuMs += took;
		s_iMaxCallMs = Math.Max(s_iMaxCallMs, took);
		if (took >= SLOW_CALL_MS)
			AICF_Stage1Diagnostics.Info("DEPOT_ACCESS_SLOW_CALL", string.Format("ms=%1 budget_ms=%2 at=%3 cells=%4 sweeps=%5 result=%6", took, budgetMs,
				Math.Round(from[0]).ToString() + "," + Math.Round(from[2]).ToString(), job.m_iCells, job.m_iSweeps, result));
		if (result == PENDING)
		{
			s_iPendingCalls++;
			Report(started);
			return PENDING;
		}
		s_iChecksDone++;
		if (result == 1)
			s_iChecksOpen++;
		s_iMaxCheckMs = Math.Max(s_iMaxCheckMs, job.m_iCpuMs);
		s_iMaxCheckFrames = Math.Max(s_iMaxCheckFrames, job.m_iFrames);
		road = job.m_vRoad;
		blocker = job.m_sBlocker;
		cells = job.m_iCells;
		sweeps = job.m_iSweeps;
		Report(started);
		return result;
	}

	protected static void Begin(AICF_DepotWayOutJob job)
	{
		job.m_iStartKey = CellKey(CellOf(job.m_vFrom[0]), CellOf(job.m_vFrom[2]));
		job.m_aHeapH.Clear();
		job.m_aHeapK.Clear();
		job.m_mSeen.Clear();
		job.m_mParent.Clear();
		job.m_iCurKey = -1;
		job.m_iCells = 0;
		job.m_fBestH = RoadDist(job, job.m_iStartKey);
		job.m_vRoad = s_mRoadNear.Get(job.m_iStartKey);
		job.m_mSeen.Set(job.m_iStartKey, true);
		job.Push(job.m_fBestH, job.m_iStartKey);
	}

	// The best-first search of WayOutWith on the shared grid, from where it stopped; PENDING when 'budgetMs' is used up.
	protected static int Advance(AICF_DepotWayOutJob job, BaseWorld world, int started, int budgetMs)
	{
		float goal = Math.Max(STEP_M, 5);
		while (true)
		{
			if (job.m_iCurKey < 0)
			{
				if (job.m_aHeapK.IsEmpty() || job.m_iCells >= MAX_CELLS)
					return Finish(job, 0);
				float h;
				int key = job.Pop(h);
				job.m_iCells++;
				if (RoadDist(job, key) <= goal)
				{
					if (PathOpen(job, world, key))
					{
						job.m_vRoad = s_mRoadNear.Get(key);
						return Finish(job, 1);
					}
					// A sweep on the way found is blocked now: look again from the start (the other sweeps are kept).
					s_iRechecks++;
					job.m_iRestarts++;
					if (job.m_iRestarts > 3)
						return Finish(job, 0);
					Begin(job);
					continue;
				}
				job.m_iCurKey = key;
				job.m_fCurH = h;
				job.m_iCurDir = 0;
			}
			int cx = CellX(job.m_iCurKey);
			int cz = CellZ(job.m_iCurKey);
			while (job.m_iCurDir < 8)
			{
				if (budgetMs >= 0 && System.GetTickCount() - started >= budgetMs)
					return PENDING;
				int dir = job.m_iCurDir;
				job.m_iCurDir++;
				int dx = NeighbourX(dir);
				int dz = NeighbourZ(dir);
				int nx = cx + dx;
				int nz = cz + dz;
				int next = CellKey(nx, nz);
				if (job.m_mSeen.Contains(next))
					continue;
				if (vector.DistanceXZ(Vector(nx * STEP_M, 0, nz * STEP_M), job.m_vFrom) > job.m_fReach)
					continue;
				int edge = EdgeKey(cx, cz, dx, dz);
				if (!EdgeOpen(job, world, edge, cx, cz, nx, nz, EDGE_TTL_S))
				{
					if (job.m_fCurH <= job.m_fBestH)
					{
						job.m_fBestH = job.m_fCurH;
						job.m_sBlocker = s_mEdgeBlocker.Get(edge);
					}
					continue;
				}
				job.m_mSeen.Set(next, true);
				job.m_mParent.Set(next, job.m_iCurKey);
				float hn = RoadDist(job, next);
				if (hn < job.m_fBestH)
				{
					job.m_fBestH = hn;
					job.m_vRoad = s_mRoadNear.Get(next);
				}
				job.Push(hn, next);
			}
			job.m_iCurKey = -1;
		}
		return PENDING;
	}

	protected static int Finish(AICF_DepotWayOutJob job, int result)
	{
		job.m_iResult = result;
		job.m_iDoneAt = System.GetTickCount();
		// The search state isn't needed for the kept answer.
		job.m_aHeapH.Clear();
		job.m_aHeapK.Clear();
		job.m_mSeen.Clear();
		job.m_mParent.Clear();
		return result;
	}

	// True when the sweep between two neighbouring cells gets through: from the cache when it is younger than 'ttl'
	// seconds, else swept now (and kept).
	protected static bool EdgeOpen(AICF_DepotWayOutJob job, BaseWorld world, int edge, int ax, int az, int bx, int bz, int ttl)
	{
		int nowS = System.GetTickCount() / 1000;
		int value;
		if (s_mEdge.Find(edge, value) && nowS - value / 4 <= ttl)
		{
			s_iCachedEdges++;
			return value % 4 == 1;
		}
		job.m_iSweeps++;
		s_iSweeps++;
		s_sIgnore = PASSABLE;
		IEntity hit;
		bool open = Sweep(world, Vector(ax * STEP_M, 0, az * STEP_M), Vector(bx * STEP_M, 0, bz * STEP_M), hit);
		if (open)
		{
			s_mEdge.Set(edge, nowS * 4 + 1);
			s_mEdgeBlocker.Remove(edge);
		}
		else
		{
			s_mEdge.Set(edge, nowS * 4 + 2);
			s_mEdgeBlocker.Set(edge, Describe(hit));
		}
		if (s_mEdge.Count() > 300000)
		{
			// A long match on a big map: start the sweep cache afresh rather than grow without end.
			s_mEdge.Clear();
			s_mEdgeBlocker.Clear();
		}
		return open;
	}

	// The way found, from the road cell back to the start: every sweep on it younger than FRESH_S, swept again if older.
	protected static bool PathOpen(AICF_DepotWayOutJob job, BaseWorld world, int goalKey)
	{
		int key = goalKey;
		int steps;
		while (key != job.m_iStartKey && steps++ < MAX_CELLS)
		{
			int parent;
			if (!job.m_mParent.Find(key, parent))
				return true;
			int px = CellX(parent);
			int pz = CellZ(parent);
			int kx = CellX(key);
			int kz = CellZ(key);
			if (!EdgeOpen(job, world, EdgeKey(px, pz, kx - px, kz - pz), px, pz, kx, kz, FRESH_S))
				return false;
			key = parent;
		}
		return true;
	}

	// Metres from a cell's centre to the nearest road point (roads 2 m wide or more); kept for the match. A cell is only
	// asked while it is within 'reach' of the job's spot, and the job holds every road point within reach + STEP_M + 2 of
	// it, so a road point within STEP_M of the cell is always among them.
	protected static float RoadDist(AICF_DepotWayOutJob job, int key)
	{
		float d;
		if (s_mRoadDist.Find(key, d))
			return d;
		vector near;
		d = Nearest(Vector(CellX(key) * STEP_M, 0, CellZ(key) * STEP_M), job.m_aPoints, near);
		s_mRoadDist.Set(key, d);
		s_mRoadNear.Set(key, near);
		return d;
	}

	// Drop answers past RESULT_TTL_MS and checks nobody asked about for two minutes.
	protected static void Forget(int now)
	{
		array<int> drop = {};
		foreach (int key, AICF_DepotWayOutJob job : s_mJobs)
		{
			if (!job || (job.m_iResult != -2 && now - job.m_iDoneAt > RESULT_TTL_MS) || now - job.m_iTouchedAt > 120000)
				drop.Insert(key);
		}
		foreach (int dropKey : drop)
			s_mJobs.Remove(dropKey);
	}

	protected static void Report(int now)
	{
		if (s_iReportAt == 0)
			s_iReportAt = now;
		if (now - s_iReportAt < 60000)
			return;
		s_iReportAt = now;
		AICF_Stage1Diagnostics.Info("DEPOT_ACCESS_COST", string.Format("calls=%1 pending=%2 cached_answers=%3 checks=%4 open=%5 sweeps=%6 cached_sweeps=%7 rechecks=%8 cpu_ms=%9",
			s_iCalls, s_iPendingCalls, s_iCachedAnswers, s_iChecksDone, s_iChecksOpen, s_iSweeps, s_iCachedEdges, s_iRechecks, s_iCpuMs)
			+ string.Format(" max_call_ms=%1 max_check_ms=%2 max_check_frames=%3 edges_kept=%4 checks_kept=%5", s_iMaxCallMs, s_iMaxCheckMs, s_iMaxCheckFrames,
			s_mEdge.Count(), s_mJobs.Count()));
		s_iCalls = 0;
		s_iPendingCalls = 0;
		s_iCachedAnswers = 0;
		s_iChecksDone = 0;
		s_iChecksOpen = 0;
		s_iSweeps = 0;
		s_iCachedEdges = 0;
		s_iRechecks = 0;
		s_iCpuMs = 0;
		s_iMaxCallMs = 0;
		s_iMaxCheckMs = 0;
		s_iMaxCheckFrames = 0;
	}

	// The shared grid: cell (x, z) has its centre at (x * STEP_M, z * STEP_M).
	protected static int CellOf(float coordinate)
	{
		return (int)Math.Round(coordinate / STEP_M);
	}

	protected static int CellKey(int x, int z)
	{
		return (x + 4096) * 8192 + z + 4096;
	}

	protected static int CellX(int key)
	{
		return key / 8192 - 4096;
	}

	protected static int CellZ(int key)
	{
		return key % 8192 - 4096;
	}

	// The 8 neighbours in WayOutWith's order (dx -1..1, dz -1..1, without 0,0).
	protected static int NeighbourX(int dir)
	{
		if (dir < 3)
			return -1;
		if (dir < 5)
			return 0;
		return 1;
	}

	protected static int NeighbourZ(int dir)
	{
		if (dir == 3)
			return -1;
		if (dir == 4)
			return 1;
		if (dir < 3)
			return dir - 1;
		return dir - 6;
	}

	// One key for the sweep a->b and b->a: four directions (1,0) (1,1) (0,1) (-1,1) from the cell they start in.
	protected static int EdgeKey(int ax, int az, int dx, int dz)
	{
		int x = ax;
		int z = az;
		int ex = dx;
		int ez = dz;
		if (ez < 0 || (ez == 0 && ex < 0))
		{
			x = ax + dx;
			z = az + dz;
			ex = -dx;
			ez = -dz;
		}
		int d = 3;
		if (ez == 0)
			d = 0;
		else if (ex == 1)
			d = 1;
		else if (ex == 0)
			d = 2;
		return CellKey(x, z) * 4 + d;
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

// ZCR (2 Nov): one spot's way-out check, carried over frames (AICF_DepotAccess.WayOutSliced); the open cells in a binary
// heap by their distance to the road (WayOutWith picked the best by a scan of all of them).
class AICF_DepotWayOutJob
{
	vector m_vFrom;
	float m_fReach;
	ref array<vector> m_aPoints = new array<vector>();
	ref array<float> m_aHeapH = new array<float>();
	ref array<int> m_aHeapK = new array<int>();
	ref map<int, bool> m_mSeen = new map<int, bool>();
	ref map<int, int> m_mParent = new map<int, int>();
	int m_iStartKey;
	int m_iCurKey = -1;
	int m_iCurDir;
	float m_fCurH;
	float m_fBestH;
	vector m_vRoad;
	string m_sBlocker = "-";
	int m_iCells;
	int m_iSweeps;
	int m_iRestarts;
	int m_iResult = -2;   // -2 still working; else WayOut's 1 / 0 / -1
	int m_iDoneAt;
	int m_iTouchedAt;
	int m_iCpuMs;
	int m_iFrames;

	void Push(float h, int key)
	{
		m_aHeapH.Insert(h);
		m_aHeapK.Insert(key);
		int i = m_aHeapH.Count() - 1;
		while (i > 0)
		{
			int parent = (i - 1) / 2;
			if (m_aHeapH[parent] <= m_aHeapH[i])
				break;
			Swap(i, parent);
			i = parent;
		}
	}

	// The open cell nearest the road ('h' = its distance); the heap must not be empty.
	int Pop(out float h)
	{
		int key = m_aHeapK[0];
		h = m_aHeapH[0];
		int last = m_aHeapH.Count() - 1;
		m_aHeapH[0] = m_aHeapH[last];
		m_aHeapK[0] = m_aHeapK[last];
		m_aHeapH.Remove(last);
		m_aHeapK.Remove(last);
		int i;
		while (true)
		{
			int left = 2 * i + 1;
			if (left >= last)
				break;
			int best = left;
			if (left + 1 < last && m_aHeapH[left + 1] < m_aHeapH[left])
				best = left + 1;
			if (m_aHeapH[i] <= m_aHeapH[best])
				break;
			Swap(i, best);
			i = best;
		}
		return key;
	}

	protected void Swap(int a, int b)
	{
		float h = m_aHeapH[a];
		int k = m_aHeapK[a];
		m_aHeapH[a] = m_aHeapH[b];
		m_aHeapK[a] = m_aHeapK[b];
		m_aHeapH[b] = h;
		m_aHeapK[b] = k;
	}
}
