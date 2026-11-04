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
	// ZCR, 2 Nov night: one team's live plan (lab/liveplan.py, -zcrLivePlan - see AICF_ZcrAttackPlan.LoadLive)
	bool m_bLive;
	bool m_bGo;            // go in; else wait at the lane's hold point
	int m_iHoldS;          // ... at most this long, then go anyway
	string m_sKey;         // "live|<faction>|<slot>"
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

	// The hold line's key: the side and the plan (in the normal matches several plans run at once); a live plan: its team.
	static string HoldKey(string faction, AICF_ZcrPlan plan)
	{
		if (!plan)
			return faction;
		if (plan.m_bLive)
			return plan.m_sKey;
		return faction + "|" + plan.m_iIndex;
	}

	// ZCR, 2 Nov night (note 515 - Martin: "attack routes should be planned based on the context (positions of soldiers,
	// cover)", "points where soldiers are dropped should be planned by LLM", "only one side can use it, the other one will
	// not, and we will compare"): live plans per team, rewritten while the battle runs by lab/liveplan.py - a threat map
	// from the enemies that side knows, candidate drop points and cover-to-cover routes, and the code's or Gemini's pick -
	// in $profile:zcr/liveplan.txt (-zcrLivePlan), read again when its version changes (looked at every LIVE_CHECK_MS):
	//   version <n>
	//   team <faction> <slot> <objective_x> <objective_z> <drop_x> <drop_z> <go> <hold_x> <hold_z> <hold_s> <route_id> <count>
	//   <count> lines "x z": the route from the drop point (or from where the team stands) into the objective
	//   end
	// A team with a live entry takes it instead of the static plan, not latched: each new leg and ride reads the latest.
	// drop 0 0: no drop point (the team is on foot). go 0: along the route to its hold point, wait there until go 1 or
	// hold_s seconds. A file without "end" (half written) is read again at the next look.
	static const float LIVE_CHECK_MS = 5000;
	protected static int s_iLiveVersion = -1;
	protected static float s_fLiveCheckedAt = -1e9;
	protected static ref map<string, ref AICF_ZcrPlan> s_mLive = new map<string, ref AICF_ZcrPlan>();
	protected static ref map<string, float> s_mLiveHoldAt = new map<string, float>();   // key@hold -> world ms the team got there
	protected static ref map<string, string> s_mLiveSeen = new map<string, string>();   // faction|slot -> route|go|drop last logged

	static bool LiveOn()
	{
		return System.IsCLIParam("zcrLivePlan");
	}

	// The live planner's drone order for a side (2 Nov night, note 515 - Martin: "maybe even the LLM commander should decide
	// when to use drones"): operator teams wanted (0: none given - the drone rule decides) and the base node to send them at
	// (-1: none). ZCR_Strategist hands them to the drone plan (ZCR_Drones.SetTeamsWanted, the plan's drone targets).
	protected static ref map<string, int> s_mLiveDroneTeams = new map<string, int>();
	protected static ref map<string, int> s_mLiveDroneTarget = new map<string, int>();

	static int LiveDroneTeams(string faction)
	{
		if (!LiveOn() || faction.IsEmpty())
			return 0;
		LoadLive();
		return s_mLiveDroneTeams.Get(faction);
	}

	// The live general's strategy for a side (4 Nov, note 542 - Martin: "llm general should also think about not only tactics
	// but battle strategy ... what soviet should try is (as it holds the points) for example I think I would try built
	// bunkers because there is currently no cover and every defender gets killed by drones"): assault, fortify,
	// hunt_drones or counter_drone ("strategy <faction> <stance>" in the plan file); empty: none given.
	protected static ref map<string, string> s_mLiveStrategy = new map<string, string>();

	static string LiveStrategy(string faction)
	{
		if (!LiveOn() || faction.IsEmpty())
			return string.Empty;
		LoadLive();
		return s_mLiveStrategy.Get(faction);
	}

	// The live general's order for one of its fighting vehicles (4 Nov, note 544 - Martin: "i notice us apc not being maybe
	// used properly do they even know how to shoot?"): "armour <faction> <unit> overwatch|assault|hold <x> <z>" - ZCR_Armour
	// drives it there (overwatch, hold) or lets it close in with the infantry (assault).
	protected static ref map<string, string> s_mLiveArmourMode = new map<string, string>();
	protected static ref map<string, vector> s_mLiveArmourAt = new map<string, vector>();

	static bool LiveArmour(string faction, int unit, out string mode, out vector at)
	{
		if (!LiveOn() || faction.IsEmpty())
			return false;
		LoadLive();
		string key = faction + "|" + unit;
		if (!s_mLiveArmourMode.Find(key, mode))
			return false;
		at = s_mLiveArmourAt.Get(key);
		return true;
	}

	static int LiveDroneTarget(string faction)
	{
		int target = -1;
		if (!s_mLiveDroneTarget.Find(faction, target))
			return -1;
		return target;
	}

	protected static void LoadLive()
	{
		float now = GetGame().GetWorld().GetWorldTime();
		if (now - s_fLiveCheckedAt < LIVE_CHECK_MS)
			return;
		s_fLiveCheckedAt = now;
		FileHandle f = FileIO.OpenFile("$profile:zcr/liveplan.txt", FileMode.READ);
		if (!f)
			return;
		string line;
		array<string> head = {};
		if (f.ReadLine(line) >= 0)
		{
			line.TrimInPlace();
			line.Split(" ", head, true);
		}
		if (head.Count() < 2 || head[0] != "version" || head[1].ToInt() == s_iLiveVersion)
		{
			f.Close();
			return;
		}
		int version = head[1].ToInt();
		map<string, ref AICF_ZcrPlan> plans = new map<string, ref AICF_ZcrPlan>();
		map<string, int> droneTeams = new map<string, int>();
		map<string, int> droneTarget = new map<string, int>();
		map<string, string> strategy = new map<string, string>();
		map<string, string> armourMode = new map<string, string>();
		map<string, vector> armourAt = new map<string, vector>();
		AICF_ZcrPlanLane lane;
		bool complete;
		while (f.ReadLine(line) >= 0)
		{
			line.TrimInPlace();
			if (line.IsEmpty())
				continue;
			array<string> v = {};
			line.Split(" ", v, true);
			if (v[0] == "end")
			{
				complete = true;
				break;
			}
			// "drones <faction> <teams> <target node, -1 none>": the live planner's drone order (ZCR_Strategist applies it).
			if (v[0] == "armour" && v.Count() >= 6)
			{
				armourMode.Set(v[1] + "|" + v[2], v[3]);
				armourAt.Set(v[1] + "|" + v[2], Vector(v[4].ToFloat(), 0, v[5].ToFloat()));
				lane = null;
				continue;
			}
			if (v[0] == "strategy" && v.Count() >= 3)
			{
				strategy.Set(v[1], v[2]);
				lane = null;
				continue;
			}
			if (v[0] == "drones" && v.Count() >= 4)
			{
				droneTeams.Set(v[1], v[2].ToInt());
				droneTarget.Set(v[1], v[3].ToInt());
				lane = null;
				continue;
			}
			if (v[0] == "team" && v.Count() >= 12)
			{
				AICF_ZcrPlan plan = new AICF_ZcrPlan();
				plan.m_bLive = true;
				plan.m_vObjective = Vector(v[3].ToFloat(), 0, v[4].ToFloat());
				plan.m_vAssembly = Vector(v[5].ToFloat(), 0, v[6].ToFloat());
				plan.m_bGo = v[7].ToInt() != 0;
				plan.m_iHoldS = v[10].ToInt();
				plan.m_sId = v[11];
				plan.m_iIndex = -1;
				plan.m_sKey = "live|" + v[1] + "|" + v[2];
				lane = new AICF_ZcrPlanLane();
				lane.m_vHold = Vector(v[8].ToFloat(), 0, v[9].ToFloat());
				plan.m_aLanes.Insert(lane);
				plans.Set(v[1] + "|" + v[2], plan);
			}
			else if (lane && v.Count() >= 2)
				lane.m_aPoints.Insert(Vector(v[0].ToFloat(), 0, v[1].ToFloat()));
		}
		f.Close();
		if (!complete)
			return;
		foreach (string teamKey, AICF_ZcrPlan loaded : plans)
		{
			if (loaded.m_bGo)
				LiveClearHold(loaded.m_sKey);
		}
		s_mLive = plans;
		s_mLiveDroneTeams = droneTeams;
		s_mLiveStrategy = strategy;
		s_mLiveArmourMode = armourMode;
		s_mLiveArmourAt = armourAt;
		s_mLiveDroneTarget = droneTarget;
		s_iLiveVersion = version;
		Print(string.Format("[ZCR][LIVEPLAN] t=%1 event=loaded version=%2 teams=%3", Math.Round(now / 1000), version, plans.Count()), LogLevel.NORMAL);
	}

	// A team's live plan for this objective (within OBJECTIVE_M; objective zero: any), else null. Logs each change of its
	// route, go or drop point: [ZCR][LIVEPLAN] event=team.
	static AICF_ZcrPlan Live(string faction, int slot, vector objective)
	{
		if (!LiveOn() || faction.IsEmpty())
			return null;
		LoadLive();
		string key = faction + "|" + slot.ToString();
		AICF_ZcrPlan plan;
		if (!s_mLive.Find(key, plan) || !plan || plan.m_aLanes.IsEmpty() || plan.m_aLanes[0].m_aPoints.Count() < 2)
			return null;
		if (objective != vector.Zero && vector.DistanceXZ(plan.m_vObjective, objective) > OBJECTIVE_M)
			return null;
		string seen = string.Format("%1|%2|%3_%4", plan.m_sId, plan.m_bGo, Math.Round(plan.m_vAssembly[0]), Math.Round(plan.m_vAssembly[2]));
		if (s_mLiveSeen.Get(key) != seen)
		{
			s_mLiveSeen.Set(key, seen);
			Print(string.Format("[ZCR][LIVEPLAN] t=%1 event=team side=%2 slot=%3 route=%4 go=%5 drop=%6,%7 hold=%8,%9", Math.Round(GetGame().GetWorld().GetWorldTime() / 1000),
				faction, slot, plan.m_sId, plan.m_bGo, Math.Round(plan.m_vAssembly[0]), Math.Round(plan.m_vAssembly[2]), Math.Round(plan.m_aLanes[0].m_vHold[0]),
				Math.Round(plan.m_aLanes[0].m_vHold[2])), LogLevel.NORMAL);
		}
		return plan;
	}

	// Per team and kind of wait (wait and support differ in hold_s), not per hold point (3 Nov, note 515): the planner redraws
	// a team's route every 30 s and its hold point moved with it, so the wait never started - 73 waits, 0 holds in 8 battles.
	protected static string LiveHoldKey(AICF_ZcrPlan plan)
	{
		return plan.m_sKey + "@" + plan.m_iHoldS;
	}

	// A go for a team ends its wait: the next wait starts its own timer.
	protected static void LiveClearHold(string teamKey)
	{
		array<string> gone = {};
		foreach (string k, float at : s_mLiveHoldAt)
		{
			if (k.IndexOf(teamKey + "@") == 0)
				gone.Insert(k);
		}
		foreach (string g : gone)
		{
			s_mLiveHoldAt.Remove(g);
		}
	}

	// The team is at its live plan's hold point (the wait starts the first time, per hold point).
	static void LiveAtHold(AICF_ZcrPlan plan, int slot)
	{
		string key = LiveHoldKey(plan);
		if (s_mLiveHoldAt.Contains(key))
			return;
		float now = GetGame().GetWorld().GetWorldTime();
		s_mLiveHoldAt.Set(key, now);
		Print(string.Format("[ZCR][HOLD] t=%1 side=%2 slot=%3 event=hold holding=1 waited_s=0 live=1 hold_s=%4", Math.Round(now / 1000), plan.m_sKey, slot, plan.m_iHoldS), LogLevel.NORMAL);
	}

	// Go for a live plan: its go flag, or its hold_s ran out at its hold point; no live entry any more - go.
	static bool LiveGo(AICF_ZcrPlan plan)
	{
		if (!plan || plan.m_bGo)
			return true;
		float at;
		if (!s_mLiveHoldAt.Find(LiveHoldKey(plan), at))
			return false;
		return GetGame().GetWorld().GetWorldTime() - at >= plan.m_iHoldS * 1000;
	}

	// The same by the hold key (AICF_InfantryApproachRoute.CanResumeTileWait while holding): the latest entry of that team.
	protected static bool LiveGoKey(string key)
	{
		LoadLive();
		AICF_ZcrPlan plan;
		if (!s_mLive.Find(key.Substring(5, key.Length() - 5), plan) || !plan)
			return true;
		return LiveGo(plan);
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
		if (key.IndexOf("live|") == 0)
			return LiveGoKey(key);
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
		// A live plan (lab/liveplan.py, -zcrLivePlan) first - not latched; else the static plan as before.
		AICF_ZcrPlan live = Live(faction, slot, objective);
		if (live)
			return live;
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
