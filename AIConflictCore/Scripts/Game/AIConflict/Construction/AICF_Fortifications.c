// ZCR (21 Oct, Martin: "I am not seeing defences being built by anybody, so it's hard to tell"): the
// fortifications AI Conflict's builders finished - entity, side and kind (bunker, MG nest, sandbags, tower) -
// for the watch view's markers. Read-only for everyone else.
class AICF_Fortifications
{
	protected static ref array<IEntity> s_aEntities = {};
	protected static ref array<string> s_aSides = {};
	protected static ref array<string> s_aKinds = {};

	static void Add(IEntity entity, string side, ResourceName prefab)
	{
		if (!entity)
			return;
		s_aEntities.Insert(entity);
		s_aSides.Insert(side);
		s_aKinds.Insert(Kind(prefab));
	}

	static string Kind(ResourceName prefab)
	{
		string p = prefab;
		if (p.Contains("MachineGunNest"))
			return "MG NEST";
		if (p.Contains("SandbagPosition"))
			return "SANDBAGS";
		if (p.Contains("GuardTower"))
			return "TOWER";
		if (p.Contains("Bunker"))
			return "BUNKER";
		return "FORTIFICATION";
	}

	// The standing ones (destroyed or removed ones drop out).
	static void List(notnull array<IEntity> entities, notnull array<string> sides, notnull array<string> kinds)
	{
		for (int i = s_aEntities.Count() - 1; i >= 0; i--)
		{
			if (!s_aEntities[i])
			{
				s_aEntities.Remove(i);
				s_aSides.Remove(i);
				s_aKinds.Remove(i);
			}
		}
		foreach (int i, IEntity e : s_aEntities)
		{
			entities.Insert(e);
			sides.Insert(s_aSides[i]);
			kinds.Insert(s_aKinds[i]);
		}
	}
}
