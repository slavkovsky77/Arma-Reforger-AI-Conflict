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
