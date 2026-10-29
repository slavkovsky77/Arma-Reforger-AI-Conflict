// ZCR, 29 Oct night (Martin, review note 449: "attacks should be planned in some smarter way, we have to read the maps -
// driving route first, then coordination of teams where they actually attack, some timing, a set of waypoints under
// attack, where they have cover"): an attack plan read from the map by lab/attackplan.py, one per HQ, in
// $profile:zcr/attackplan.txt (-zcrAttackPlan; the lab seeds the profile with it):
//   plan <hq_x> <hq_z> <objective_x> <objective_z> <assembly_x> <assembly_z> <id>
//   lane <count> <hold_x> <hold_z>        then <count> lines "x z", from the assembly point to the objective
// A side's plan is the one whose HQ is within 300 m of its main base (Arland's HQs swap between matches). Its attack
// teams' approach legs follow the lanes (AICF_InfantryApproachRoute; team N on lane N mod lanes) instead of a straight
// line with lane offsets; ZCR drives its rides to the assembly point (ZCR_Dismount).
class AICF_ZcrPlanLane
{
	ref array<vector> m_aPoints = {};
	vector m_vHold;
}

class AICF_ZcrPlan
{
	vector m_vHq;
	vector m_vObjective;
	vector m_vAssembly;
	string m_sId;
	ref array<ref AICF_ZcrPlanLane> m_aLanes = {};
}

class AICF_ZcrAttackPlan
{
	protected static bool s_bLoaded;
	protected static ref array<ref AICF_ZcrPlan> s_aPlans = {};
	protected static ref map<string, int> s_mFor = new map<string, int>();   // faction -> plan index, -1 none

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
		while (f.ReadLine(line) >= 0)
		{
			line.TrimInPlace();
			if (line.IsEmpty())
				continue;
			array<string> v = {};
			line.Split(" ", v, true);
			if (v[0] == "plan" && v.Count() >= 8)
			{
				plan = new AICF_ZcrPlan();
				plan.m_vHq = Vector(v[1].ToFloat(), 0, v[2].ToFloat());
				plan.m_vObjective = Vector(v[3].ToFloat(), 0, v[4].ToFloat());
				plan.m_vAssembly = Vector(v[5].ToFloat(), 0, v[6].ToFloat());
				plan.m_sId = v[7];
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
		Print(string.Format("[ZCR][ATTACKPLAN] loaded plans=%1", s_aPlans.Count()), LogLevel.NORMAL);
	}

	// The side's plan whatever its teams go for now (ZCR's hold line), else null.
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

	// The side's plan when its attack goes for the plan's objective (within 150 m), else null.
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
		if (vector.DistanceXZ(plan.m_vObjective, objective) > 150)
			return null;
		return plan;
	}
}
