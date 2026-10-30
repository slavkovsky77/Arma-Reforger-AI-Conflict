// ZCR, 29 Oct night (Martin, review note 449: "attacks should be planned in some smarter way, we have to read the maps -
// driving route first, then coordination of teams where they actually attack, some timing, a set of waypoints under
// attack, where they have cover"): attack plans read from the map by lab/attackplan.py in $profile:zcr/attackplan.txt
// (-zcrAttackPlan; the lab seeds the profile with it):
//   plan <from_x> <from_z> <objective_x> <objective_z> <assembly_x> <assembly_z> <id>
//   lane <count> <hold_x> <hold_z>        then <count> lines "x z", from the assembly point to the objective
// The one-point sub-lab: one plan per HQ. The normal matches (29 Oct evening, Martin's go, note 451): a plan for every
// objective from every base its attacks can come from. A team's plan for an objective is the one from the origin nearest
// where the team started (within FROM_M), latched for that team and objective (ForTeam), so its ride and its legs agree.
// Its approach legs follow the lanes (AICF_InfantryApproachRoute; team N on lane N mod lanes) instead of a straight line
// with lane offsets; ZCR drives its ride to the assembly point (ZCR_Dismount).
class AICF_ZcrPlanLane
{
	ref array<vector> m_aPoints = {};
	vector m_vHold;
}

class AICF_ZcrPlan
{
	vector m_vHq;          // where the attack comes from (an HQ, or any base)
	vector m_vObjective;
	vector m_vAssembly;
	string m_sId;
	int m_iIndex;
	ref array<ref AICF_ZcrPlanLane> m_aLanes = {};
}

class AICF_ZcrAttackPlan
{
	static const float FROM_M = 800;       // a team's plan: from an origin this close to where it started
	static const float OBJECTIVE_M = 150;  // ... for an objective this close to the plan's

	protected static bool s_bLoaded;
	protected static ref array<ref AICF_ZcrPlan> s_aPlans = {};
	protected static ref map<string, int> s_mFor = new map<string, int>();    // faction -> plan index, -1 none (one-point)
	protected static ref map<string, int> s_mTeam = new map<string, int>();   // faction|slot|objective -> plan index, -1 none

	static bool On()
	{
		return System.IsCLIParam("zcrAttackPlan");
	}

	protected static void Load()
	{
		if (s_bLoaded)
			return;
		s_bLoaded = true;
		FileHandle f = FileIO.OpenFile("$profile:zcr/attackplan.txt", FileMode.READ);
		if (!f)
		{
			Print("[ZCR][ATTACKPLAN] no $profile:zcr/attackplan.txt - the attacks go as before", LogLevel.WARNING);
			return;
		}
		string line;
		AICF_ZcrPlan plan;
		AICF_ZcrPlanLane lane;
		// "world <name>" starts a map's plans: kept only when the world file's path has that name in it (one file for
		// every map the watched matches rotate through).
		string world = GetGame().GetWorldFile();
		world.ToLower();
		bool active = true;
		int skipped;
		while (f.ReadLine(line) >= 0)
		{
			line.TrimInPlace();
			if (line.IsEmpty())
				continue;
			array<string> v = {};
			line.Split(" ", v, true);
			if (v[0] == "world" && v.Count() >= 2)
			{
				string name = v[1];
				name.ToLower();
				active = world.Contains(name);
				plan = null;
				lane = null;
				continue;
			}
			if (!active)
			{
				if (v[0] == "plan")
					skipped++;
				continue;
			}
			if (v[0] == "plan" && v.Count() >= 8)
			{
				plan = new AICF_ZcrPlan();
				plan.m_vHq = Vector(v[1].ToFloat(), 0, v[2].ToFloat());
				plan.m_vObjective = Vector(v[3].ToFloat(), 0, v[4].ToFloat());
				plan.m_vAssembly = Vector(v[5].ToFloat(), 0, v[6].ToFloat());
				plan.m_sId = v[7];
				plan.m_iIndex = s_aPlans.Count();
				s_aPlans.Insert(plan);
				lane = null;
			}
			else if (v[0] == "lane" && v.Count() >= 4 && plan)
			{
				lane = new AICF_ZcrPlanLane();
				lane.m_vHold = Vector(v[2].ToFloat(), 0, v[3].ToFloat());
				plan.m_aLanes.Insert(lane);
			}
			else if (lane && v.Count() >= 2)
				lane.m_aPoints.Insert(Vector(v[0].ToFloat(), 0, v[1].ToFloat()));
		}
		f.Close();
		Print(string.Format("[ZCR][ATTACKPLAN] loaded plans=%1 other_maps=%2 world=%3", s_aPlans.Count(), skipped, world), LogLevel.NORMAL);
	}

	// The hold line (Martin: "coordination of teams where they would actually attack, maybe some timing"): a team at its
	// lane's hold point waits (AICF_InfantryApproachRoute) until GO_TEAMS teams on the same plan wait there, or
	// HOLD_MAX_MS of game time after the first got there; then all go in together. -zcrNoAttackHold: no waiting.
	static const int GO_TEAMS = 3;
	static const float HOLD_MAX_MS = 90000;
	protected static ref map<string, ref array<int>> s_mHolding = new map<string, ref array<int>>();
	protected static ref map<string, float> s_mFirstHoldAt = new map<string, float>();
	protected static ref map<string, bool> s_mGo = new map<string, bool>();

	// The hold line's key: the side and the plan (in the normal matches several plans run at once).
	static string HoldKey(string faction, AICF_ZcrPlan plan)
	{
		if (!plan)
			return faction;
		return faction + "|" + plan.m_iIndex;
	}

	static void AtHold(string key, int slot)
	{
		array<int> holding = s_mHolding.Get(key);
		if (!holding)
		{
			holding = {};
			s_mHolding.Set(key, holding);
		}
		if (holding.Contains(slot))
			return;
		holding.Insert(slot);
		float now = GetGame().GetWorld().GetWorldTime();
		if (!s_mFirstHoldAt.Contains(key))
			s_mFirstHoldAt.Set(key, now);
		Print(string.Format("[ZCR][HOLD] t=%1 side=%2 slot=%3 event=hold holding=%4 waited_s=%5", Math.Round(now / 1000), key, slot, holding.Count(),
			Math.Round((now - s_mFirstHoldAt.Get(key)) / 1000)), LogLevel.NORMAL);
	}

	static bool Go(string key)
	{
		if (System.IsCLIParam("zcrNoAttackHold") || s_mGo.Get(key))
			return true;
		array<int> holding = s_mHolding.Get(key);
		if (!holding || holding.IsEmpty())
			return false;
		float now = GetGame().GetWorld().GetWorldTime();
		if (holding.Count() < GO_TEAMS && now - s_mFirstHoldAt.Get(key) < HOLD_MAX_MS)
			return false;
		s_mGo.Set(key, true);
		Print(string.Format("[ZCR][HOLD] t=%1 side=%2 slot=-1 event=go holding=%3 waited_s=%4", Math.Round(now / 1000), key, holding.Count(),
			Math.Round((now - s_mFirstHoldAt.Get(key)) / 1000)), LogLevel.NORMAL);
		return true;
	}

	// The side's plan whatever its teams go for now, else null (the one-point sub-lab).
	static AICF_ZcrPlan Of(string faction)
	{
		if (!On() || faction.IsEmpty())
			return null;
		Load();
		int index;
		if (!s_mFor.Find(faction, index))
			For(faction, vector.Zero);
		if (!s_mFor.Find(faction, index) || index < 0)
			return null;
		return s_aPlans[index];
	}

	// The side's plan when its attack goes for the plan's objective (within OBJECTIVE_M), else null: the one from its main
	// base (the one-point sub-lab: one plan per HQ; Arland's HQs swap between matches).
	static AICF_ZcrPlan For(string faction, vector objective)
	{
		if (!On() || faction.IsEmpty())
			return null;
		Load();
		int index;
		if (!s_mFor.Find(faction, index))
		{
			index = -1;
			SCR_CampaignFaction f = SCR_CampaignFaction.Cast(GetGame().GetFactionManager().GetFactionByKey(faction));
			if (f && f.GetMainBase() && f.GetMainBase().GetOwner())
			{
				vector hq = f.GetMainBase().GetOwner().GetOrigin();
				for (int i; i < s_aPlans.Count(); i++)
				{
					if (vector.DistanceXZ(s_aPlans[i].m_vHq, hq) < 300)
						index = i;
				}
				Print(string.Format("[ZCR][ATTACKPLAN] faction=%1 hq=%2,%3 plan=%4", faction, Math.Round(hq[0]), Math.Round(hq[2]), index), LogLevel.NORMAL);
			}
			s_mFor.Set(faction, index);
		}
		if (index < 0)
			return null;
		AICF_ZcrPlan plan = s_aPlans[index];
		if (vector.DistanceXZ(plan.m_vObjective, objective) > OBJECTIVE_M || plan.m_sId == "none")
			return null;
		return plan;
	}

	// A team's plan for an objective: the plan for that objective from the origin nearest where the team started (from),
	// within FROM_M; else none - also when that origin's plan is "none". Latched per side, team and objective at the first ask - the ride (ZCR_Dismount) and the
	// approach legs ask at different moments and places. from = zero: where the team is not known - the main base.
	static AICF_ZcrPlan ForTeam(string faction, int slot, vector objective, vector from)
	{
		if (!On() || faction.IsEmpty() || objective == vector.Zero)
			return null;
		Load();
		string key = string.Format("%1|%2|%3_%4", faction, slot, Math.Round(objective[0] / 10), Math.Round(objective[2] / 10));
		int index;
		if (s_mTeam.Find(key, index))
		{
			if (index < 0)
				return null;
			return s_aPlans[index];
		}
		if (from == vector.Zero)
		{
			SCR_CampaignFaction f = SCR_CampaignFaction.Cast(GetGame().GetFactionManager().GetFactionByKey(faction));
			if (f && f.GetMainBase() && f.GetMainBase().GetOwner())
				from = f.GetMainBase().GetOwner().GetOrigin();
		}
		index = -1;
		float best = FROM_M;
		int forObjective;
		for (int i; i < s_aPlans.Count(); i++)
		{
			if (vector.DistanceXZ(s_aPlans[i].m_vObjective, objective) > OBJECTIVE_M)
				continue;
			forObjective++;
			float d = vector.DistanceXZ(s_aPlans[i].m_vHq, from);
			if (d < best)
			{
				best = d;
				index = i;
			}
		}
		// A "none" plan (lab/attackplan.py: the measured runs say no plan does better from that origin): no plan.
		if (index >= 0 && s_aPlans[index].m_sId == "none")
			index = -2;
		s_mTeam.Set(key, index);
		string id = "none";
		if (index >= 0)
			id = s_aPlans[index].m_sId;
		else if (index == -2)
			id = "none_measured";
		// origin= the plan's (lab app: which plan of .runtime/attack/<map>/plans.json each team plays - its lines of attack).
		string origin = "-";
		if (index >= 0)
			origin = string.Format("%1,%2", Math.Round(s_aPlans[index].m_vHq[0]), Math.Round(s_aPlans[index].m_vHq[2]));
		string where = string.Format("objective=%1,%2 from=%3,%4", Math.Round(objective[0]), Math.Round(objective[2]), Math.Round(from[0]), Math.Round(from[2]));
		Print(string.Format("[ZCR][ATTACKPLAN] side=%1 slot=%2 %3 plans_for_objective=%4 plan=%5 origin=%6 origin_m=%7",
			faction, slot, where, forObjective, id, origin, Math.Round(best)), LogLevel.NORMAL);
		if (index < 0)
			return null;
		return s_aPlans[index];
	}
}
