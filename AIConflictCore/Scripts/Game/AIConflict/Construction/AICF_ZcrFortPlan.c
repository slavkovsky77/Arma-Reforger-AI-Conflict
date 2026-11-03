// ZCR (2 Nov 2026, Martin: "fortifications should be planned via the LLM general - I think it will be probably much smarter in
// choosing the place and direction of fortifications"): fortification plans made before the battle by lab/fortplan.py. The code
// there proposes spots the builders can use (clear ground inside the base's building radius, off the roads, not under trees or
// roofs, flat, away from the base's core); the model picks the spots and the facing of each from a map image of the base with
// the threat direction, the enemy's approach lanes, the open ground and the cover - in $profile:zcr/fortplan.txt (-zcrFortPlan;
// the lab seeds the profile):
//   world <name>
//   base <base_x> <base_z> <threat_x> <threat_z> <bearing_from> <bearing_to> <source> <id>
//   spot <x> <z> <facing>        the plan's spots in order; facing in compass degrees (0 north, 90 east), the front of the
//                                composition (its +Z: the MG, the barbed wire and the sandbag walls of Conflict's slot compositions)
// A base's plan is the one whose threat sector holds the bearing to the nearest enemy-held base (the base is at the front by
// AtFront's rule). The site search (AICF_ConstructionSiteSearch.BeginCandidate) tries the plan's open spots first, each at its
// facing, +-TURN_DEG and a few metres aside (Seed), with every usual check; when none passes, today's search. Logged: each
// planned try (event=try) and every fortification placed, from the plan or not (dir: its front's compass bearing; err: degrees
// off the nearest enemy base):
//   [ZCR][FORTPLAN] event=placed base= kind= x= z= dir= source=llm|algo|rules threat=x,z err= spot= variant=
class AICF_ZcrFortSpot
{
	vector m_vAt;
	float m_fFacing;
	int m_iOrders;     // orders that tried it and placed nothing there
	bool m_bUsed;
}

class AICF_ZcrFortBasePlan
{
	vector m_vBase;
	vector m_vThreat;
	float m_fFrom;
	float m_fTo;
	string m_sSource;
	string m_sId;
	ref array<ref AICF_ZcrFortSpot> m_aSpots = {};
}

class AICF_ZcrFortPlan
{
	static const float BASE_M = 120;         // a plan belongs to the base within this of its base point
	static const float SECTOR_SLACK = 25;    // a plan's threat sector, widened this much either side
	static const int MAX_ORDERS = 2;         // a spot tried by this many orders without a fortification placed there is dropped
	static const float TURN_DEG = 20;        // each spot is also tried at its facing +- this

	protected static bool s_bLoaded;
	protected static int s_iYawSign;
	protected static ref array<ref AICF_ZcrFortBasePlan> s_aPlans = {};
	protected static ref map<string, int> s_mOrderPlan = new map<string, int>();             // order token -> plan index, -1 none
	protected static ref map<string, ref array<int>> s_mOrderSpots = new map<string, ref array<int>>();   // token -> its spots
	protected static ref map<string, int> s_mTries = new map<string, int>();                 // token -> plan tries so far

	static bool On()
	{
		return System.IsCLIParam("zcrFortPlan");
	}

	protected static void Load()
	{
		if (s_bLoaded)
			return;
		s_bLoaded = true;
		FileHandle f = FileIO.OpenFile("$profile:zcr/fortplan.txt", FileMode.READ);
		if (!f)
		{
			Print("[ZCR][FORTPLAN] no $profile:zcr/fortplan.txt - fortifications placed as before", LogLevel.WARNING);
			return;
		}
		string world = GetGame().GetWorldFile();
		world.ToLower();
		bool active = true;
		int spots;
		string line;
		AICF_ZcrFortBasePlan plan;
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
				continue;
			}
			if (!active)
				continue;
			if (v[0] == "base" && v.Count() >= 9)
			{
				plan = new AICF_ZcrFortBasePlan();
				plan.m_vBase = Vector(v[1].ToFloat(), 0, v[2].ToFloat());
				plan.m_vThreat = Vector(v[3].ToFloat(), 0, v[4].ToFloat());
				plan.m_fFrom = v[5].ToFloat();
				plan.m_fTo = v[6].ToFloat();
				plan.m_sSource = v[7];
				plan.m_sId = v[8];
				s_aPlans.Insert(plan);
			}
			else if (v[0] == "spot" && v.Count() >= 4 && plan)
			{
				AICF_ZcrFortSpot spot = new AICF_ZcrFortSpot();
				spot.m_vAt = Vector(v[1].ToFloat(), 0, v[2].ToFloat());
				spot.m_fFacing = v[3].ToFloat();
				plan.m_aSpots.Insert(spot);
				spots++;
			}
		}
		f.Close();
		Print(string.Format("[ZCR][FORTPLAN] event=loaded plans=%1 spots=%2 world=%3", s_aPlans.Count(), spots, world), LogLevel.NORMAL);
	}

	// Compass bearing from one point to another (0 north = +Z, 90 east = +X).
	static float Bearing(vector from, vector to)
	{
		float b = Math.Atan2(to[0] - from[0], to[2] - from[2]) * Math.RAD2DEG;
		if (b < 0)
			b += 360;
		return b;
	}

	static float AngleBetween(float a, float b)
	{
		float d = a - b;
		while (d > 180)
			d -= 360;
		while (d < -180)
			d += 360;
		return Math.AbsFloat(d);
	}

	// The yaw (Math3D.AnglesToMatrix) whose forward (+Z, transform[2]) points along a compass bearing - its sign measured once.
	static float YawFor(float facing)
	{
		if (s_iYawSign == 0)
		{
			vector m[4];
			Math3D.AnglesToMatrix(Vector(90, 0, 0), m);
			s_iYawSign = 1;
			if (m[2][0] < 0)
				s_iYawSign = -1;
		}
		return facing * s_iYawSign;
	}

	// The nearest base held by the other main side (FIA's don't count), else zero.
	static vector Threat(SCR_CampaignMilitaryBaseComponent own, Faction faction)
	{
		SCR_MilitaryBaseSystem system = SCR_MilitaryBaseSystem.GetInstance();
		if (!system || !own || !own.GetOwner() || !faction)
			return vector.Zero;
		vector at = own.GetOwner().GetOrigin();
		array<SCR_MilitaryBaseComponent> bases = {};
		system.GetBases(bases);
		vector best = vector.Zero;
		float bestM = float.MAX;
		foreach (SCR_MilitaryBaseComponent b : bases)
		{
			SCR_CampaignMilitaryBaseComponent other = SCR_CampaignMilitaryBaseComponent.Cast(b);
			if (!other || !other.GetOwner() || !other.GetFaction() || other.GetFaction() == faction || other.GetFaction().GetFactionKey() == "FIA")
				continue;
			float d = vector.DistanceXZ(other.GetOwner().GetOrigin(), at);
			if (d < bestM)
			{
				bestM = d;
				best = other.GetOwner().GetOrigin();
			}
		}
		return best;
	}

	// The order's plan (latched per order): this base's plan whose threat sector holds the bearing to the nearest enemy base.
	protected static AICF_ZcrFortBasePlan PlanOf(AICF_ConstructionOrder order)
	{
		int index;
		if (s_mOrderPlan.Find(order.m_sToken, index))
		{
			if (index < 0)
				return null;
			return s_aPlans[index];
		}
		index = -1;
		vector at = order.m_Base.GetOwner().GetOrigin();
		vector threat = Threat(order.m_Base, order.m_Faction);
		float bearing = -1;
		if (threat != vector.Zero)
		{
			bearing = Bearing(at, threat);
			float best = float.MAX;
			foreach (int i, AICF_ZcrFortBasePlan p : s_aPlans)
			{
				if (vector.DistanceXZ(p.m_vBase, at) > BASE_M)
					continue;
				// The sector runs clockwise from m_fFrom to m_fTo; the bearing's distance outside it (0 inside).
				float width = p.m_fTo - p.m_fFrom;
				if (width < 0)
					width += 360;
				float into = bearing - p.m_fFrom;
				if (into < 0)
					into += 360;
				float outside = 0;
				if (into > width)
					outside = Math.Min(into - width, 360 - into);
				float centre = p.m_fFrom + width / 2;
				float score = outside * 1000 + AngleBetween(bearing, centre);
				if (outside <= SECTOR_SLACK && score < best)
				{
					best = score;
					index = i;
				}
			}
		}
		s_mOrderPlan.Set(order.m_sToken, index);
		string id = "none";
		if (index >= 0)
			id = s_aPlans[index].m_sId;
		Print(string.Format("[ZCR][FORTPLAN] event=plan token=%1 base=%2 at=%3,%4 threat=%5,%6 bearing=%7 plan=%8", order.m_sToken, BaseName(order.m_Base),
			Math.Round(at[0]), Math.Round(at[2]), Math.Round(threat[0]), Math.Round(threat[2]), Math.Round(bearing), id), LogLevel.NORMAL);
		if (index < 0)
			return null;
		return s_aPlans[index];
	}

	// The site search's next candidate for a fortification order from its base's plan: false when there is no plan or every open
	// spot was tried (then today's search). Spots in the plan's order, each in VARIANTS ways: at its facing, +TURN_DEG, -TURN_DEG,
	// then at its facing SHIFT_M to the left, to the right and back. The first Farm run (2 Nov) placed none of the Farm's planned
	// spots: AI Conflict's road clearance (ROAD_OR_ACCESS_CORRIDOR) and single trees (PHYSICAL_OBSTRUCTION) the map dump
	// doesn't have - a few metres aside clears most of them.
	static const int VARIANTS = 6;
	static const float SHIFT_M = 6;

	static bool Seed(AICF_ConstructionOrder order, out vector spot, out float yaw)
	{
		if (order.m_eType != AICF_EConstructionType.FORTIFICATION || !On() || !order.m_Base || !order.m_Base.GetOwner())
			return false;
		Load();
		AICF_ZcrFortBasePlan plan = PlanOf(order);
		if (!plan)
			return false;
		array<int> open = s_mOrderSpots.Get(order.m_sToken);
		if (!open)
		{
			open = {};
			foreach (int i, AICF_ZcrFortSpot s : plan.m_aSpots)
			{
				if (!s.m_bUsed && s.m_iOrders < MAX_ORDERS)
					open.Insert(i);
			}
			s_mOrderSpots.Set(order.m_sToken, open);
		}
		int tries = s_mTries.Get(order.m_sToken);
		if (tries >= open.Count() * VARIANTS)
			return false;
		s_mTries.Set(order.m_sToken, tries + 1);
		int index = open[tries / VARIANTS];
		int variant = tries % VARIANTS;
		AICF_ZcrFortSpot chosen = plan.m_aSpots[index];
		if (variant == 0)
			chosen.m_iOrders++;
		float facing = chosen.m_fFacing;
		if (variant == 1)
			facing += TURN_DEG;
		else if (variant == 2)
			facing -= TURN_DEG;
		spot = chosen.m_vAt;
		vector front = Vector(Math.Sin(facing * Math.DEG2RAD), 0, Math.Cos(facing * Math.DEG2RAD));
		vector right = Vector(front[2], 0, -front[0]);
		if (variant == 3)
			spot = spot - right * SHIFT_M;
		else if (variant == 4)
			spot = spot + right * SHIFT_M;
		else if (variant == 5)
			spot = spot - front * SHIFT_M;
		yaw = YawFor(facing);
		// The candidate's index tells the placement which spot it was (a restored pending candidate keeps it); today's search
		// often lands on the same proven sites, so the position alone doesn't.
		order.m_iCandidateIndex = -1000 - 10 * index - variant;
		Print(string.Format("[ZCR][FORTPLAN] event=try token=%1 plan=%2 spot=%3 variant=%4 facing=%5 x=%6 z=%7", order.m_sToken, plan.m_sId, index,
			variant, Math.Round(facing), Math.Round(spot[0]), Math.Round(spot[2])), LogLevel.NORMAL);
		return true;
	}

	// ZCR (3 Nov, note 518): today's search (no plan, or every planned spot failed - Farm's fenced paddocks fail the game's own
	// checks even at spots it built on before) turns each candidate centre through 8 fixed headings, and the first 32 centres all
	// face north (CandidateTransformIndex: every centre's first heading before any second one) - the building test's fallback
	// fortifications faced a median 102 deg off the enemy. With -zcrFortFace the 8 headings are turned to the nearest enemy-held
	// base from the candidate: facing it first, then +-30, +-60, +-90 deg and the back, the centres and every check as before.
	static bool FallbackYaw(AICF_ConstructionOrder order, vector at, int attempt, out float yaw)
	{
		if (!order || order.m_eType != AICF_EConstructionType.FORTIFICATION || !System.IsCLIParam("zcrFortFace") || !order.m_Base || attempt < 0)
			return false;
		vector threat = Threat(order.m_Base, order.m_Faction);
		if (threat == vector.Zero)
			return false;
		int k = attempt % 8;
		float turn = 180;
		if (k < 7)
		{
			turn = ((k + 1) / 2) * 30;
			if (k % 2 == 0)
				turn = -turn;
		}
		yaw = YawFor(Bearing(at, threat) + turn);
		return true;
	}

	static string BaseName(SCR_CampaignMilitaryBaseComponent base)
	{
		if (!base)
			return "?";
		string name = WidgetManager.Translate(base.GetBaseName());
		name.Replace(" ", "_");
		return name;
	}

	// A fortification placed (any source): which plan spot it is, if any, and how it faces the threat.
	static void Placed(AICF_ConstructionOrder order)
	{
		if (!order || order.m_eType != AICF_EConstructionType.FORTIFICATION || !order.m_Base || !order.m_Base.GetOwner())
			return;
		vector centre = order.m_aTransform[3];
		if (order.m_Metadata)
		{
			vector c = (order.m_Metadata.m_vMin + order.m_Metadata.m_vMax) * 0.5;
			centre = centre + order.m_aTransform[0] * c[0] + order.m_aTransform[2] * c[2];
		}
		float dir = Bearing(vector.Zero, order.m_aTransform[2]);
		string source = "rules";
		int spotIndex = -1;
		int variant = -1;
		int index;
		if (order.m_iCandidateIndex <= -1000 && s_mOrderPlan.Find(order.m_sToken, index) && index >= 0)
		{
			spotIndex = (-1000 - order.m_iCandidateIndex) / 10;
			variant = (-1000 - order.m_iCandidateIndex) % 10;
			if (spotIndex < s_aPlans[index].m_aSpots.Count())
			{
				s_aPlans[index].m_aSpots[spotIndex].m_bUsed = true;
				source = s_aPlans[index].m_sSource;
			}
		}
		vector threat = Threat(order.m_Base, order.m_Faction);
		float err = -1;
		if (threat != vector.Zero)
			err = AngleBetween(dir, Bearing(centre, threat));
		string kind = AICF_Fortifications.Kind(order.m_sPrefab);
		kind.Replace(" ", "_");
		string head = string.Format("[ZCR][FORTPLAN] event=placed token=%1 side=%2 base=%3 kind=%4 x=%5 z=%6", order.m_sToken, order.m_sFaction,
			BaseName(order.m_Base), kind, Math.Round(centre[0] * 10) / 10, Math.Round(centre[2] * 10) / 10);
		Print(head + string.Format(" dir=%1 yaw=%2 source=%3 threat=%4,%5 err=%6 spot=%7 variant=%8", Math.Round(dir), Math.Round(order.m_fYaw),
			source, Math.Round(threat[0]), Math.Round(threat[2]), Math.Round(err), spotIndex, variant), LogLevel.NORMAL);
	}
}
