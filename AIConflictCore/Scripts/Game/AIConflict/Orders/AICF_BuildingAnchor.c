// ZCR: where a base's defenders stand - in and around its buildings, not on the open ground at its spawn
// point (Martin, 20 Oct 02:35: "at Moss Hill three guys got killed by drones and they just spend time
// outdoors like it's the time of their life - and they have a house there"). Once per base: points every
// 8 m within 120 m of the spawn point that are inside a building (a roof within 6 m over the head, walls
// within 8 m on at least 5 of 8 sides); the anchor is the one with the most others within 20 m (3 at least),
// nearest the spawn point on a tie. No building there: the spawn point as before. The defend waypoint then
// stands at the anchor with a 25 m radius, and the game's own defend behaviour picks positions there.
class AICF_BuildingAnchor
{
	protected static ref map<EntityID, vector> s_mAnchors = new map<EntityID, vector>();
	protected static ref TraceParam s_Trace;
	static const float DEFEND_RADIUS = 25;

	static bool Find(SCR_CampaignMilitaryBaseComponent base, vector spawn, out vector anchor)
	{
		anchor = spawn;
		if (!base || !base.GetOwner() || System.IsCLIParam("aicfDefendInOpen"))
			return false;
		EntityID id = base.GetOwner().GetID();
		vector cached;
		if (s_mAnchors.Find(id, cached))
		{
			if (cached == vector.Zero)
				return false;
			anchor = cached;
			return true;
		}
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return false;
		array<vector> inside = {};
		for (float x = -120; x <= 120; x += 8)
		{
			for (float z = -120; z <= 120; z += 8)
			{
				if (x * x + z * z > 120 * 120)
					continue;
				vector p = spawn + Vector(x, 0, z);
				p[1] = world.GetSurfaceY(p[0], p[2]);
				if (Inside(world, p))
					inside.Insert(p);
			}
		}
		int best = -1;
		int bestCount = 2;
		float bestDistance = 1e9;
		foreach (int i, vector p : inside)
		{
			int n;
			foreach (vector q : inside)
			{
				if (vector.DistanceXZ(p, q) < 20)
					n++;
			}
			float d = vector.DistanceXZ(p, spawn);
			if (n > bestCount || (n == bestCount && best >= 0 && d < bestDistance))
			{
				best = i;
				bestCount = n;
				bestDistance = d;
			}
		}
		if (best < 0)
		{
			s_mAnchors.Set(id, vector.Zero);
			AICF_Stage2Diagnostics.Info("DEFEND_ANCHOR", string.Format("base=%1 anchor=NONE inside_points=%2", AICF_Stage1Diagnostics.BaseKey(base), inside.Count()));
			return false;
		}
		anchor = inside[best];
		s_mAnchors.Set(id, anchor);
		AICF_Stage2Diagnostics.Info("DEFEND_ANCHOR", string.Format("base=%1 anchor=%2 %3 from_spawn=%4 inside_points=%5 cluster=%6",
			AICF_Stage1Diagnostics.BaseKey(base), Math.Round(anchor[0]), Math.Round(anchor[2]), Math.Round(bestDistance), inside.Count(), bestCount));
		return true;
	}

	protected static bool Inside(BaseWorld world, vector p)
	{
		if (!s_Trace)
			s_Trace = new TraceParam();
		s_Trace.Flags = TraceFlags.ENTS | TraceFlags.WORLD;
		s_Trace.TargetLayers = EPhysicsLayerDefs.ViewGeometry | EPhysicsLayerDefs.FireGeometry;
		s_Trace.Exclude = null;
		s_Trace.Start = p + "0 1.5 0";
		s_Trace.End = p + "0 7.5 0";
		if (world.TraceMove(s_Trace, null) >= 1.0)
			return false;
		int walls;
		for (int k; k < 8; k++)
		{
			float r = k * 45 * Math.DEG2RAD;
			s_Trace.Start = p + "0 1 0";
			s_Trace.End = p + Vector(Math.Cos(r) * 8, 1, Math.Sin(r) * 8);
			if (world.TraceMove(s_Trace, null) < 1.0)
				walls++;
			if (walls >= 5)
				return true;
			if (walls + (7 - k) < 5)
				return false;
		}
		return false;
	}
}
