// ZCR: kill zones - where each side's soldiers died lately (Martin, 20 Oct 02:02: "the soviets died where
// the previous team got killed"). A march step keeps 60 m away from the side's own losses of the last
// 3 game minutes (AICF_InfantryApproachRoute.TowardCover). Fed by the commander bot's loss log.
class AICF_DangerZones
{
	protected static ref array<vector> s_aPos = {};
	protected static ref array<string> s_aSide = {};
	protected static ref array<float> s_aAt = {};
	static const float KEEP_MS = 180000;       // 3 game minutes
	static const float RADIUS = 60;

	protected static float Now()
	{
		if (!GetGame().GetWorld())
			return 0;
		return GetGame().GetWorld().GetWorldTime();
	}

	static void Add(vector pos, string side)
	{
		Prune();
		s_aPos.Insert(pos);
		s_aSide.Insert(side);
		s_aAt.Insert(Now());
	}

	// A loss of 'side' within 'radius' of 'p' in the last 3 game minutes.
	static bool Near(vector p, string side, float radius = RADIUS)
	{
		float now = Now();
		for (int i = s_aPos.Count() - 1; i >= 0; i--)
		{
			if (now - s_aAt[i] > KEEP_MS)
				break;
			if (s_aSide[i] == side && vector.DistanceXZ(s_aPos[i], p) < radius)
				return true;
		}
		return false;
	}

	protected static void Prune()
	{
		float now = Now();
		while (!s_aAt.IsEmpty() && now - s_aAt[0] > KEEP_MS)
		{
			s_aPos.RemoveOrdered(0);
			s_aSide.RemoveOrdered(0);
			s_aAt.RemoveOrdered(0);
		}
	}
}
