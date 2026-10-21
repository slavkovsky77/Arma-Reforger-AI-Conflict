// ZCR (20 Oct): the world's own minefields lying across roads are taken off the map at the start.
// Arland's Conflict world has rows of six TM-62M anti-tank mines (EffectModule_MineField_Rect_Row) across
// the exit roads ~300-500 m from both main operating bases; AI drivers do not see or avoid mines, so the
// first vehicles of every match blew up there: 191 of 342 vehicles lost on 20 Oct, the supply trucks
// included, and their crews (no killer). A minefield whose mines lie within ROAD_M of a road is removed;
// the others stay. -aicfKeepRoadMines keeps them all.
// Logs ROAD_MINEFIELD_REMOVED / ROAD_MINEFIELD_KEPT per minefield and ROAD_MINES_SCAN once.
class AICF_RoadMines
{
	static const float ROAD_M = 8;
	protected static ref array<IEntity> s_aFound;

	static void Start()
	{
		if (!Replication.IsServer())
			return;
		GetGame().GetCallqueue().CallLater(Clear, 3000, false);
	}

	protected static void Clear()
	{
		if (System.IsCLIParam("aicfKeepRoadMines"))
		{
			AICF_Stage1Diagnostics.Info("ROAD_MINES_SCAN", "skipped=1 reason=CLI_KEEP");
			return;
		}
		BaseWorld world = GetGame().GetWorld();
		SCR_AIWorld aiWorld = SCR_AIWorld.Cast(GetGame().GetAIWorld());
		if (!world || !aiWorld || !aiWorld.GetRoadNetworkManager())
			return;
		RoadNetworkManager roads = aiWorld.GetRoadNetworkManager();
		int started = System.GetTickCount();
		vector mins, maxs;
		world.GetBoundBox(mins, maxs);
		s_aFound = {};
		world.QueryEntitiesByAABB(mins, maxs, Add, IsMinefield, EQueryEntitiesFlags.ALL);
		int removed, kept, mines;
		foreach (IEntity field : s_aFound)
		{
			if (!field)
				continue;
			array<IEntity> parts = {};
			Collect(field, parts);
			float nearest = 1000000;
			int count;
			foreach (IEntity part : parts)
			{
				if (!IsMine(part))
					continue;
				count++;
				BaseRoad road;
				float distance;
				roads.GetClosestRoad(part.GetOrigin(), road, distance, true);
				if (road && distance < nearest)
					nearest = distance;
			}
			vector at = field.GetOrigin();
			string details = string.Format("x=%1 z=%2 mines=%3 road_m=%4 prefab=%5", Math.Round(at[0]), Math.Round(at[2]), count, Math.Round(nearest), Name(field));
			if (nearest <= ROAD_M)
			{
				removed++;
				mines += count;
				AICF_Stage1Diagnostics.Info("ROAD_MINEFIELD_REMOVED", details);
				SCR_EntityHelper.DeleteEntityAndChildren(field);
			}
			else
			{
				kept++;
				AICF_Stage1Diagnostics.Info("ROAD_MINEFIELD_KEPT", details);
			}
		}
		AICF_Stage1Diagnostics.Info("ROAD_MINES_SCAN", string.Format("minefields=%1 removed=%2 mines_removed=%3 kept=%4 scan_ms=%5",
			s_aFound.Count(), removed, mines, kept, System.GetTickCount() - started));
		s_aFound = null;
		ClearRoadBlocks(world, roads, mins, maxs);
	}

	// ZCR (21 Oct): closed gates and road blocks lying on a road are taken off too - Arland has a metal gate
	// in a walled checkpoint with Czech hedgehogs across the road east of the Airport (1332, 2360); AI
	// drivers can't open it (AI Conflict's open-gate step failed, teleported the truck a few metres and it
	// stuck again), and supply trucks waited there for minutes. A gate within 6 m of a road line, a
	// hedgehog or barrier within 3 m. -aicfKeepRoadBlocks keeps them.
	protected static void ClearRoadBlocks(BaseWorld world, RoadNetworkManager roads, vector mins, vector maxs)
	{
		if (System.IsCLIParam("aicfKeepRoadBlocks"))
			return;
		int started = System.GetTickCount();
		s_aFound = {};
		world.QueryEntitiesByAABB(mins, maxs, Add, IsRoadBlock, EQueryEntitiesFlags.ALL);
		int gates, blocks;
		foreach (IEntity e : s_aFound)
		{
			if (!e)
				continue;
			string name = Name(e);
			bool gate = name.Contains("Gate");
			vector road;
			if (!AICF_LogisticsPlanner.NearestRoadPoint(roads, e.GetOrigin(), 6, road))
				continue;
			if (!gate && vector.DistanceXZ(road, e.GetOrigin()) > 3)
				continue;
			vector at = e.GetOrigin();
			AICF_Stage1Diagnostics.Info("ROAD_BLOCK_REMOVED", string.Format("x=%1 z=%2 road_m=%3 prefab=%4", Math.Round(at[0]), Math.Round(at[2]),
				Math.Round(vector.DistanceXZ(road, at)), name));
			SCR_EntityHelper.DeleteEntityAndChildren(e);
			if (gate)
				gates++;
			else
				blocks++;
		}
		AICF_Stage1Diagnostics.Info("ROAD_BLOCKS_SCAN", string.Format("candidates=%1 gates_removed=%2 blocks_removed=%3 scan_ms=%4",
			s_aFound.Count(), gates, blocks, System.GetTickCount() - started));
		s_aFound = null;
	}

	protected static bool IsRoadBlock(IEntity e)
	{
		if (!e || e.GetParent())
			return false;
		string n = Name(e);
		return (n.Contains("Gate") && (n.Contains("DoorSet") || n.Contains("Barrier"))) || n.Contains("CzechHedgehog") ||
			n.Contains("RoadBarrier") || n.Contains("BarrierRoad") || n.Contains("Roadblock") || n.Contains("RoadBlock");
	}

	protected static bool IsMinefield(IEntity e)
	{
		return e && !e.GetParent() && Name(e).Contains("MineField");
	}

	protected static bool Add(IEntity e)
	{
		s_aFound.Insert(e);
		return true;
	}

	protected static bool IsMine(IEntity e)
	{
		return Name(e).Contains("/Explosives/Mine");
	}

	protected static void Collect(IEntity e, notnull array<IEntity> found)
	{
		IEntity child = e.GetChildren();
		while (child)
		{
			found.Insert(child);
			Collect(child, found);
			child = child.GetSibling();
		}
	}

	protected static string Name(IEntity e)
	{
		EntityPrefabData data = e.GetPrefabData();
		if (data && data.GetPrefabName())
			return data.GetPrefabName();
		return e.ClassName();
	}
}
