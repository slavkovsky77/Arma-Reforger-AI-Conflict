// ZCR (21 Oct): a supply cache at every base that has no supply storage, placed at the start. SpearHead's
// Yunakivka bases have none: they showed 0/1000 supplies, the game's supply generator had nowhere to put
// supplies (our start stock and income vanished), nothing could be built there and no truck could unload -
// a dead economy. Conflict's own buildable cache (E_SupplyCache_S_US_01 / _USSR_01), in the owner's
// version (FIA bases: the US one), on a level spot clear of objects 8-60 m from the base's centre.
// Only bases without a supplies container; -aicfNoBaseStorage turns it off. Logs BASE_STORAGE_PLACED.
class AICF_BaseStorage
{
	protected static ref array<IEntity> s_aNear;

	static void Start()
	{
		if (!Replication.IsServer() || System.IsCLIParam("aicfNoBaseStorage"))
			return;
		GetGame().GetCallqueue().CallLater(Place, 4000, false);
	}

	protected static void Place()
	{
		SCR_MilitaryBaseSystem system = SCR_MilitaryBaseSystem.GetInstance();
		if (!system)
			return;
		array<SCR_MilitaryBaseComponent> bases = {};
		system.GetBases(bases);
		int placed, failed;
		foreach (SCR_MilitaryBaseComponent b : bases)
		{
			SCR_CampaignMilitaryBaseComponent base = SCR_CampaignMilitaryBaseComponent.Cast(b);
			if (!base || !base.GetOwner() || base.GetType() == SCR_ECampaignBaseType.RELAY)
				continue;
			SCR_ResourceComponent resources = base.GetResourceComponent();
			if (resources && resources.GetContainer(EResourceType.SUPPLIES))
				continue;
			vector spot;
			if (!FindSpot(base.GetOwner().GetOrigin(), spot))
			{
				failed++;
				AICF_Stage1Diagnostics.Info("BASE_STORAGE_FAILED", string.Format("base=%1 reason=NO_CLEAR_SPOT", base.GetOwner().GetID()));
				continue;
			}
			ResourceName prefab = "{94D9E9B690F98756}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_SupplyCache_S_US_01.et";
			if (base.GetFaction() && base.GetFaction().GetFactionKey() == "USSR")
				prefab = "{EB63BCC0AA844B17}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_SupplyCache_S_USSR_01.et";
			Resource res = Resource.Load(prefab);
			if (!res || !res.IsValid())
				continue;
			EntitySpawnParams params = new EntitySpawnParams();
			params.TransformMode = ETransformMode.WORLD;
			params.Transform[3] = spot;
			IEntity cache = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
			if (!cache)
				continue;
			placed++;
			int stocked = Stock(cache, StartStock());
			AICF_Stage1Diagnostics.Info("BASE_STORAGE_PLACED", string.Format("base=%1 x=%2 z=%3 from_centre_m=%4 stocked=%5 prefab=%6", base.GetOwner().GetID(),
				Math.Round(spot[0]), Math.Round(spot[2]), Math.Round(vector.DistanceXZ(spot, base.GetOwner().GetOrigin())), stocked, prefab));
		}
		AICF_Stage1Diagnostics.Info("BASE_STORAGE_SCAN", string.Format("placed=%1 failed=%2", placed, failed));
	}

	// The new cache starts with supplies so the base can build and recruit (-aicfBaseStorageStock N, 1000).
	static int StartStock()
	{
		string value;
		if (System.GetCLIParam("aicfBaseStorageStock", value) && !value.IsEmpty())
			return value.ToInt();
		return 1000;
	}

	protected static int Stock(IEntity root, int amount)
	{
		if (amount <= 0 || !root)
			return 0;
		array<IEntity> todo = {root};
		while (!todo.IsEmpty())
		{
			IEntity e = todo[0];
			todo.Remove(0);
			SCR_ResourceComponent rc = SCR_ResourceComponent.Cast(e.FindComponent(SCR_ResourceComponent));
			if (rc)
			{
				SCR_ResourceContainer container = rc.GetContainer(EResourceType.SUPPLIES);
				if (container)
				{
					float put = Math.Min(amount, container.GetMaxResourceValue());
					if (container.SetResourceValue(put))
						return Math.Round(put);
				}
			}
			IEntity child = e.GetChildren();
			while (child)
			{
				todo.Insert(child);
				child = child.GetSibling();
			}
		}
		return 0;
	}

	// A level spot (corners within 0.8 m of each other over 8 m) with nothing standing within 4 m.
	protected static bool FindSpot(vector centre, out vector spot)
	{
		BaseWorld world = GetGame().GetWorld();
		array<float> dists = {8, 14, 20, 28, 36, 46, 60};
		foreach (float d : dists)
		{
			for (int a = 0; a < 12; a++)
			{
				float r = a * 30 * Math.DEG2RAD;
				vector p = centre + Vector(Math.Cos(r) * d, 0, Math.Sin(r) * d);
				p[1] = world.GetSurfaceY(p[0], p[2]);
				if (world.IsOcean() && p[1] < world.GetOceanBaseHeight() + 1)
					continue;
				float lo = p[1], hi = p[1];
				for (int c = 0; c < 4; c++)
				{
					float cr = (45 + c * 90) * Math.DEG2RAD;
					float h = world.GetSurfaceY(p[0] + Math.Cos(cr) * 5.6, p[2] + Math.Sin(cr) * 5.6);
					lo = Math.Min(lo, h);
					hi = Math.Max(hi, h);
				}
				if (hi - lo > 0.8)
					continue;
				s_aNear = {};
				world.QueryEntitiesBySphere(p + Vector(0, 1, 0), 4, Near, null, EQueryEntitiesFlags.ALL);
				if (!s_aNear.IsEmpty())
					continue;
				spot = p;
				return true;
			}
		}
		return false;
	}

	// Anything with a body that would stand in the way (ground clutter without physics doesn't count).
	protected static bool Near(IEntity e)
	{
		if (e && e.GetPhysics() && !e.ClassName().Contains("Terrain"))
			s_aNear.Insert(e);
		return true;
	}
}
