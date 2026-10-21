// ZCR (21 Oct): maps without an AI navmesh get one built while the match runs. SpearHead's Yunakivka is a
// PvP map: "Navmesh Init 'Soldiers': No navmesh file specified! Will initialize empty navmesh world" - every
// infantry move failed (971 in a match, 37 on Arland) and the soldiers stayed at their main bases. The game
// can rebuild navmesh tiles at run time (AIWorld.RequestNavmeshRebuild - Conflict uses it when a building
// goes up): squares of CHUNK m are requested one by one, round the bases first (main bases, then the rest,
// out to RING m), then the rest of the map. Only when a base centre has no navmesh tile at the start.
// Logs NAVMESH_BUILD_STARTED, NAVMESH_BUILD_PROGRESS every 30 s (requested squares, bases with a tile).
// -aicfNoNavmeshBuild turns it off.
class AICF_NavmeshBuild
{
	static const float CHUNK = 200;
	static const float RING = 600;
	protected static ref array<vector> s_aQueue;
	protected static ref array<vector> s_aProbes;
	protected static ref map<int, bool> s_mQueued = new map<int, bool>();
	protected static int s_iRequested;
	protected static int s_iStartedAt;
	protected static int s_iNextLog;

	static void Start()
	{
		// Opt-in (-aicfNavmeshBuild): the 21 Oct test on Yunakivka invalidated the tiles near the bases and
		// generated none (0 of 16 base centres with a tile after 10 min, nobody left the main bases) - the
		// rebuild only refreshes existing navmesh. A navmesh has to be generated offline (Workbench).
		if (!Replication.IsServer() || !System.IsCLIParam("aicfNavmeshBuild"))
			return;
		GetGame().GetCallqueue().CallLater(Begin, 5000, false);
	}

	protected static NavmeshWorldComponent Soldiers()
	{
		AIWorld world = GetGame().GetAIWorld();
		if (!world)
			return null;
		return world.GetNavmeshWorldComponent("Soldiers");
	}

	protected static void Begin()
	{
		NavmeshWorldComponent nav = Soldiers();
		SCR_MilitaryBaseSystem system = SCR_MilitaryBaseSystem.GetInstance();
		if (!nav || !system)
			return;
		array<SCR_MilitaryBaseComponent> bases = {};
		system.GetBases(bases);
		s_aProbes = {};
		array<vector> hq = {};
		array<vector> others = {};
		foreach (SCR_MilitaryBaseComponent b : bases)
		{
			SCR_CampaignMilitaryBaseComponent base = SCR_CampaignMilitaryBaseComponent.Cast(b);
			if (!base || !base.GetOwner())
				continue;
			vector at = base.GetOwner().GetOrigin();
			s_aProbes.Insert(at);
			if (base.IsHQ())
				hq.Insert(at);
			else
				others.Insert(at);
		}
		// A navmesh exists where the bases are: nothing to do.
		int valid;
		foreach (vector p : s_aProbes)
		{
			if (nav.IsTileValid(p))
				valid++;
		}
		if (s_aProbes.IsEmpty() || valid * 2 > s_aProbes.Count())
			return;
		s_aQueue = {};
		foreach (vector c : hq)
			AddRing(c);
		foreach (vector c : others)
			AddRing(c);
		// Then the corridors between neighbouring bases and the rest of the map, in rows.
		vector mins, maxs;
		GetGame().GetWorld().GetBoundBox(mins, maxs);
		for (float x = mins[0]; x < maxs[0]; x += CHUNK)
		{
			for (float z = mins[2]; z < maxs[2]; z += CHUNK)
				Add(Vector(x + CHUNK / 2, 0, z + CHUNK / 2));
		}
		s_iStartedAt = System.GetTickCount();
		AICF_Stage1Diagnostics.Info("NAVMESH_BUILD_STARTED", string.Format("bases=%1 bases_with_tile=%2 squares=%3 chunk_m=%4",
			s_aProbes.Count(), valid, s_aQueue.Count(), CHUNK));
		GetGame().GetCallqueue().CallLater(Step, 250, true);
	}

	protected static void AddRing(vector c)
	{
		for (float dx = -RING; dx <= RING; dx += CHUNK)
		{
			for (float dz = -RING; dz <= RING; dz += CHUNK)
				Add(c + Vector(dx, 0, dz));
		}
	}

	protected static void Add(vector c)
	{
		// One request per square of the grid (snapped), never twice.
		int ix = Math.Floor(c[0] / CHUNK);
		int iz = Math.Floor(c[2] / CHUNK);
		int id = ix * 100000 + iz;
		if (s_mQueued.Contains(id))
			return;
		s_mQueued.Insert(id, true);
		s_aQueue.Insert(Vector(ix * CHUNK, 0, iz * CHUNK));
	}

	protected static void Step()
	{
		AIWorld world = GetGame().GetAIWorld();
		if (!world || !s_aQueue)
			return;
		if (!s_aQueue.IsEmpty())
		{
			vector a = s_aQueue[0];
			s_aQueue.RemoveOrdered(0);
			float y = GetGame().GetWorld().GetSurfaceY(a[0] + CHUNK / 2, a[2] + CHUNK / 2);
			world.RequestNavmeshRebuild(Vector(a[0], y - 200, a[2]), Vector(a[0] + CHUNK, y + 200, a[2] + CHUNK));
			s_iRequested++;
		}
		int now = System.GetTickCount();
		if (now < s_iNextLog)
			return;
		s_iNextLog = now + 30000;
		NavmeshWorldComponent nav = Soldiers();
		int valid;
		if (nav)
		{
			foreach (vector p : s_aProbes)
			{
				if (nav.IsTileValid(p))
					valid++;
			}
		}
		AICF_Stage1Diagnostics.Info("NAVMESH_BUILD_PROGRESS", string.Format("requested=%1 left=%2 bases_with_tile=%3/%4 elapsed_s=%5",
			s_iRequested, s_aQueue.Count(), valid, s_aProbes.Count(), (now - s_iStartedAt) / 1000));
		if (s_aQueue.IsEmpty())
			GetGame().GetCallqueue().Remove(Step);
	}
}
