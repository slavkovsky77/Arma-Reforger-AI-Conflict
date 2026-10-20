// ZCR (20 Oct, Martin: "making logistics work is very important - if supply points are missing on the
// map, add them!"; "the green ones, usable by every side"). Arland's Conflict has no supply depot (no
// SOURCE_BASE): the supply trucks could load only at the two main bases at the map's ends, so a run was
// a drive across the island and back. The bases nearest the middle between the two main bases become
// supply depots:
//   - they gain supplies every minute (-aicfSupplyDepotIncome, default 150; up to their storage);
//   - while neutral (FIA or nobody) any side's trucks load there, like Conflict's green depots;
//   - a side that holds one loads there as at its main base - the enemy can't: worth capturing.
// -aicfSupplyDepots N (default 2; 0 = off). Logs SUPPLY_DEPOT_SELECTED once per depot, SUPPLY_DEPOT_INCOME
// every 5 minutes.
class AICF_SupplyDepots
{
	protected static ref array<SCR_CampaignMilitaryBaseComponent> s_aDepots = {};
	protected static bool s_bPicked;
	protected static int s_iTicks;

	static void Start()
	{
		if (!Replication.IsServer())
			return;
		GetGame().GetCallqueue().CallLater(Pick, 2000, false);
		GetGame().GetCallqueue().CallLater(Income, 60000, true);
	}

	static int Count()
	{
		string value;
		if (System.GetCLIParam("aicfSupplyDepots", value) && !value.IsEmpty())
			return Math.ClampInt(value.ToInt(), 0, 6);
		return 2;
	}

	static bool Is(SCR_CampaignMilitaryBaseComponent base)
	{
		return base && s_aDepots.Contains(base);
	}

	// Once: the non-HQ bases nearest the middle between the two main bases (smallest difference of the
	// distances to them). Safe to call again; the first call decides.
	static void Pick()
	{
		if (s_bPicked || !Replication.IsServer())
			return;
		FactionManager fm = GetGame().GetFactionManager();
		SCR_MilitaryBaseSystem system = SCR_MilitaryBaseSystem.GetInstance();
		if (!fm || !system)
			return;
		SCR_CampaignFaction us = SCR_CampaignFaction.Cast(fm.GetFactionByKey("US"));
		SCR_CampaignFaction ussr = SCR_CampaignFaction.Cast(fm.GetFactionByKey("USSR"));
		if (!us || !ussr || !us.GetMainBase() || !ussr.GetMainBase() || !us.GetMainBase().GetOwner() || !ussr.GetMainBase().GetOwner())
			return;
		s_bPicked = true;
		int count = Count();
		if (count <= 0)
			return;
		vector a = us.GetMainBase().GetOwner().GetOrigin();
		vector b = ussr.GetMainBase().GetOwner().GetOrigin();
		array<SCR_MilitaryBaseComponent> all = {};
		system.GetBases(all);
		array<SCR_CampaignMilitaryBaseComponent> sorted = {};
		array<float> keys = {};
		foreach (SCR_MilitaryBaseComponent m : all)
		{
			SCR_CampaignMilitaryBaseComponent base = SCR_CampaignMilitaryBaseComponent.Cast(m);
			if (!base || base.IsHQ() || !base.GetOwner() || base.GetType() != SCR_ECampaignBaseType.BASE || base.GetSuppliesMax() <= 0)
				continue;
			vector at = base.GetOwner().GetOrigin();
			float key = Math.AbsFloat(vector.DistanceXZ(at, a) - vector.DistanceXZ(at, b));
			int i;
			while (i < keys.Count() && keys[i] < key)
				i++;
			keys.InsertAt(key, i);
			sorted.InsertAt(base, i);
		}
		for (int k = 0; k < sorted.Count() && k < count; k++)
		{
			SCR_CampaignMilitaryBaseComponent depot = sorted[k];
			s_aDepots.Insert(depot);
			vector p = depot.GetOwner().GetOrigin();
			string owner = "none";
			if (depot.GetFaction())
				owner = depot.GetFaction().GetFactionKey();
			AICF_Stage4Diagnostics.Info("SUPPLY_DEPOT_SELECTED", string.Format("base=%1 x=%2 z=%3 owner=%4 off_middle_m=%5 storage=%6",
				depot.GetOwner().GetID(), Math.Round(p[0]), Math.Round(p[2]), owner, Math.Round(keys[k] / 2), Math.Round(depot.GetSuppliesMax())));
		}
	}

	static int IncomePerMinute()
	{
		string value;
		if (System.GetCLIParam("aicfSupplyDepotIncome", value) && !value.IsEmpty())
			return Math.ClampInt(value.ToInt(), 0, 2000);
		return 150;
	}

	protected static void Income()
	{
		int add = IncomePerMinute();
		s_iTicks++;
		foreach (SCR_CampaignMilitaryBaseComponent depot : s_aDepots)
		{
			if (!depot || !depot.GetOwner())
				continue;
			int room = Math.Round(depot.GetSuppliesMax() - depot.GetSupplies());
			if (room > 0 && add > 0)
				depot.AddSupplies(Math.Min(add, room));
			if (s_iTicks % 5 == 1)
			{
				string owner = "none";
				if (depot.GetFaction())
					owner = depot.GetFaction().GetFactionKey();
				AICF_Stage4Diagnostics.Info("SUPPLY_DEPOT_INCOME", string.Format("base=%1 owner=%2 supplies=%3/%4 added_per_min=%5",
					depot.GetOwner().GetID(), owner, Math.Round(depot.GetSupplies()), Math.Round(depot.GetSuppliesMax()), add));
			}
		}
	}

	// Neutral: no owner, or an owner outside the two armies (FIA) - every side's trucks may load.
	static bool Neutral(SCR_CampaignMilitaryBaseComponent base)
	{
		if (!Is(base))
			return false;
		Faction owner = base.GetFaction();
		return !owner || AICF_ContentProfile.GetActive().GetStableFactionKey(owner.GetFactionKey()).IsEmpty();
	}
}
