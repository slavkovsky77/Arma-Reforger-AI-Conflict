// Отдельная частота строительства; infantry commander и worker сохраняют cadence.
class AICF_ConstructionConfig
{
	int m_iDecisionMs = 60000;
	int m_iCooldownMs = 60000;
	int m_iDeadlineMs = 120000;
	int m_iCandidatesPerTick = 4;
	int m_iMetadataEntriesPerTick = 24;
	int m_iSliceMs = 8;
	int m_iQueriesPerTick = 96;
	// Больше transforms в одном заказе без увеличения нагрузки одного tick.
	int m_iAttempts = 256;
	int m_iReserveGroups = 1;
	float m_fMargin = 1;
	float m_fHeightDelta = 0.8;
	float m_fTerrainStep = 3;

	void AICF_ConstructionConfig()
	{
		string value;
		// ZCR (21 Oct): the planner's windows, decisions and cooldowns run on real time. In the lab's sped-up
		// watched matches (-zcrSpeed 3) they are that much shorter, so they last as long in game time as in a
		// normal match: at 3x a small barracks searched its 2-minute window - 6 game minutes - and found no
		// site, then the forced fallback found one in 13 s (w17), and every depot waited for it.
		// Explicit -aicfConstruction*Ms options still win.
		float speed = 1;
		if (System.GetCLIParam("zcrSpeed", value) && !value.IsEmpty())
			speed = Math.Max(1, value.ToFloat());
		m_iDecisionMs = 60000 / speed;
		m_iCooldownMs = 60000 / speed;
		m_iDeadlineMs = 120000 / speed;
		if (System.GetCLIParam("aicfConstructionDecisionMs", value))
			m_iDecisionMs = Math.ClampInt(value.ToInt(), 60000, 3600000);
		if (System.GetCLIParam("aicfConstructionCooldownMs", value))
			m_iCooldownMs = Math.ClampInt(value.ToInt(), 60000, 3600000);
		if (System.GetCLIParam("aicfConstructionDeadlineMs", value))
			m_iDeadlineMs = Math.ClampInt(value.ToInt(), 1000, 120000);
		if (System.GetCLIParam("aicfConstructionCandidatesPerTick", value))
			m_iCandidatesPerTick = Math.ClampInt(value.ToInt(), 1, 4);
		if (System.GetCLIParam("aicfConstructionMetadataEntriesPerTick", value))
			m_iMetadataEntriesPerTick = Math.ClampInt(value.ToInt(), 1, 32);
		if (System.GetCLIParam("aicfConstructionSliceMs", value))
			m_iSliceMs = Math.ClampInt(value.ToInt(), 1, 16);
		if (System.GetCLIParam("aicfConstructionQueriesPerTick", value))
			m_iQueriesPerTick = Math.ClampInt(value.ToInt(), 16, 128);
		if (System.GetCLIParam("aicfConstructionAttempts", value))
			m_iAttempts = Math.ClampInt(value.ToInt(), 4, 512);
		if (System.GetCLIParam("aicfConstructionReserveGroups", value))
			m_iReserveGroups = Math.ClampInt(value.ToInt(), 0, 100);
	}
}
