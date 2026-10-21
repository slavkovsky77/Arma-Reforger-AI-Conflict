// ZCR (21 Oct): a longer default radio range for every Conflict base on maps whose bases are far apart.
// SpearHead's Yunakivka scenario (10 km) links its bases with its own component instead of radio, so with
// Conflict's default range no base was within reach of a main base (2-4 km away): AI Conflict and our
// strategist saw nothing to attack and the soldiers stayed home. -aicfRadioRange N (metres; Yunakivka 3000).
modded class SCR_CampaignMilitaryBaseComponent
{
	void AICF_SetRadioRangeDefault(float range)
	{
		if (range <= m_fRadioRangeDefault)
			return;
		m_fRadioRangeDefault = range;
		RecalculateRadioRange();
	}
}

class AICF_RadioRange
{
	static void Extend(string mapKey)
	{
		float range;
		string value;
		if (System.GetCLIParam("aicfRadioRange", value) && !value.IsEmpty())
			range = value.ToFloat();
		else if (mapKey == "Yunakivka")
			range = 3000;
		if (range <= 0 || !Replication.IsServer())
			return;
		SCR_MilitaryBaseSystem system = SCR_MilitaryBaseSystem.GetInstance();
		if (!system)
			return;
		array<SCR_MilitaryBaseComponent> bases = {};
		system.GetBases(bases);
		int n;
		foreach (SCR_MilitaryBaseComponent b : bases)
		{
			SCR_CampaignMilitaryBaseComponent base = SCR_CampaignMilitaryBaseComponent.Cast(b);
			if (!base)
				continue;
			base.AICF_SetRadioRangeDefault(range);
			n++;
		}
		AICF_Stage1Diagnostics.Info("RADIO_RANGE_EXTENDED", string.Format("map=%1 bases=%2 range_m=%3", mapKey, n, range));
	}
}
