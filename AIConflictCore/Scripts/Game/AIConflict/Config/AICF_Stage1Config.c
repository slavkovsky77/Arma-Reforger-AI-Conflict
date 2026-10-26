// Server-owned Stage 1 balance settings with conservative bounds for local Arland play.
class AICF_Stage1Config
{
	static const string AI_COMMANDER_MODE_BOTH = "BOTH";
	static const string AI_COMMANDER_MODE_US = "US";
	static const string AI_COMMANDER_MODE_USSR = "USSR";
	// Fireteams (ZCR, 19 Oct - Martin: "groups should be smaller, 2-5 people max, so it's realistic";
	// Arma 3's teams were 4): 20 teams of 5 per side instead of 10 squads of 10 - the same 100 soldiers.
	static const int GROUP_SLOTS_PER_FACTION = 20;
	static const int DEFAULT_GROUP_SIZE = 5;
	static const int DEFAULT_FULL_SIZE_GROUPS_PER_FACTION = GROUP_SLOTS_PER_FACTION;
	static const int MIN_GROUP_SIZE = 1;
	// ZCR (26 Oct - Martin: "fewer teams, 50 soldiers a side; not all teams the same size - assault teams can be bigger
	// than crew, engineer, drone or transport teams"): the hard maximum is 8; every team still defaults to 5
	// (FULL_GROUP_SIZE), only -aicfAttackTeamSize / -aicfDefendTeamSize make assault or defend teams bigger or smaller.
	static const int MAX_GROUP_SIZE = 8;
	static const int FULL_GROUP_SIZE = 5;
	// Compatibility alias for diagnostics and older static contracts. Runtime
	// roster gates use each slot's commander-selected desired size.
	static const int MANAGED_GROUP_SIZE = DEFAULT_GROUP_SIZE;
	static const int ATTACK_SLOTS_PER_FACTION = 12;
	static const int DEFEND_SLOTS_PER_FACTION = 8;
	static const int RESERVE_SLOTS_PER_FACTION = 0;
	static const int LEGACY_ATTACK_SLOTS_PER_FACTION = 10;
	static const int LEGACY_DEFEND_SLOTS_PER_FACTION = 6;
	static const int LEGACY_RESERVE_SLOTS_PER_FACTION = 4;
	static const int ROLE_MINIMUM_DWELL_INTERVALS = 2;
	// Teams that start small and wait for the rest (ZCR, 19 Oct - Martin: "many soldiers just chilling
	// in the base doing nothing", "doing push-ups"): a side's barracks recruits one soldier at a time
	// for one team at a time, so teams that started as 1 man waited at the main base 10-20 minutes
	// (two armies on Arland: the first team out at 13.5 min, the other side's at 21 min, with over
	// 1000 supplies in stock). Infantry teams now start with 3 of their 5 and set out once they have 3
	// (-aicfInfantryDeploySize, -aicfMusterSize: 1-5).
	static const int DEFAULT_INFANTRY_DEPLOY_SIZE = 3;
	static const int DEFAULT_MUSTER_SIZE = 3;

	static int InfantryDeploySize()
	{
		return CliGroupSize("aicfInfantryDeploySize", DEFAULT_INFANTRY_DEPLOY_SIZE);
	}

	static int MusterSize()
	{
		return CliGroupSize("aicfMusterSize", DEFAULT_MUSTER_SIZE);
	}

	// Infantry teams ride a vehicle to marches this long (ZCR; AI Conflict never let infantry ride).
	static float InfantryRideMeters()
	{
		string value;
		if (System.GetCLIParam("aicfInfantryRideMeters", value) && !value.IsEmpty())
			return Math.Clamp(value.ToFloat(), 300, 5000);
		return 300;
	}

	protected static int CliGroupSize(string name, int fallback)
	{
		string value;
		if (System.GetCLIParam(name, value) && !value.IsEmpty())
			return Math.ClampInt(value.ToInt(), MIN_GROUP_SIZE, MAX_GROUP_SIZE);
		return fallback;
	}

	static const int DEFAULT_INITIAL_TICKETS = 12;
	static const int DEFAULT_REPLACEMENT_TICKET_COST = 1;
	static const int DEFAULT_REINFORCEMENT_DELAY_MS = 30000;
	static const int DEFAULT_COMMANDER_INTERVAL_MS = 15000;
	static const int DEFAULT_MAX_MANAGED_AGENTS = 220;
	static const int DEFAULT_WAR_TEMPO_PERCENT = 100;

	static const int MAX_TICKET_VALUE = 1000000;
	static const int MAX_DELAY_MS = 3600000;
	static const int MIN_COMMANDER_INTERVAL_MS = 1000;
	static const int MAX_COMMANDER_INTERVAL_MS = 600000;
	// Лимит допускает полный состав двадцати slots после набора в казармах:
	// 10 slots * 10 бойцов * 2 стороны = 200. Стартовая пехота — один боец.
	static const int MIN_MANAGED_AGENTS = 200;
	static const int MAX_MANAGED_AGENTS = 256;

	// ZCR (25 Oct): fewer teams a side (-aicfSquadsPerSide N, 2-20; default 20, as before). The FPS sub-lab found the
	// watched game's frame rate set by the speed and the number of soldiers (teams of 1 instead of 2: +6.5 fps at 3x),
	// and every side spawns all its teams at the start whatever the agent caps. GROUP_SLOTS_PER_FACTION stays the id
	// space (builders' and trucks' slot ids start after it).
	protected static int s_iSquadsPerSide = -1;

	static int SquadsPerSide()
	{
		if (s_iSquadsPerSide < 0)
		{
			s_iSquadsPerSide = GROUP_SLOTS_PER_FACTION;
			string value;
			if (System.GetCLIParam("aicfSquadsPerSide", value) && value.ToInt() > 0)
				s_iSquadsPerSide = Math.ClampInt(value.ToInt(), 2, GROUP_SLOTS_PER_FACTION);
		}
		return s_iSquadsPerSide;
	}

	static int GetDefaultGroupSizeForSlot(int slotId)
	{
		if (slotId >= 0 && slotId < DEFAULT_FULL_SIZE_GROUPS_PER_FACTION)
			return FULL_GROUP_SIZE;
		return DEFAULT_GROUP_SIZE;
	}

	// A team's size by its role (ZCR, 26 Oct): -aicfAttackTeamSize / -aicfDefendTeamSize (1-8); 5 otherwise, as before.
	static int GroupSizeForRole(AICF_EGroupRole role, int slotId)
	{
		if (role == AICF_EGroupRole.ATTACK)
			return CliGroupSize("aicfAttackTeamSize", GetDefaultGroupSizeForSlot(slotId));
		if (role == AICF_EGroupRole.DEFEND)
			return CliGroupSize("aicfDefendTeamSize", GetDefaultGroupSizeForSlot(slotId));
		return GetDefaultGroupSizeForSlot(slotId);
	}

	protected int m_iInitialTickets;
	protected int m_iReplacementTicketCost;
	protected int m_iReinforcementDelayMs;
	protected int m_iCommanderIntervalMs;
	protected int m_iMaxManagedAgents;
	protected int m_iMaxAgentsPerFaction;   // 0: no per-side cap (ZCR lab, 24 Oct: -aicfMaxAgentsPerFaction 50)
	protected int m_iWarTempoPercent;
	protected FactionKey m_sExpectedPlayerFaction;
	protected string m_sAICommanderMode;
	protected string m_sInvalidAICommanderMode;
	protected bool m_bRequirePlayerForResult;
	protected bool m_bActiveForcesRolesEnabled;
	protected bool m_bAICommanderModeValid;

	void AICF_Stage1Config()
	{
		ResetToDefaults();
		ApplyCLIOverrides();
	}

	void ResetToDefaults()
	{
		m_iInitialTickets = DEFAULT_INITIAL_TICKETS;
		m_iReplacementTicketCost = DEFAULT_REPLACEMENT_TICKET_COST;
		m_iReinforcementDelayMs = DEFAULT_REINFORCEMENT_DELAY_MS;
		m_iCommanderIntervalMs = DEFAULT_COMMANDER_INTERVAL_MS;
		m_iMaxManagedAgents = DEFAULT_MAX_MANAGED_AGENTS;
		m_iWarTempoPercent = DEFAULT_WAR_TEMPO_PERCENT;
		m_sExpectedPlayerFaction = string.Empty;
		m_sAICommanderMode = AI_COMMANDER_MODE_BOTH;
		m_sInvalidAICommanderMode = string.Empty;
		m_bRequirePlayerForResult = true;
		m_bActiveForcesRolesEnabled = true;
		m_bAICommanderModeValid = true;
	}

	int GetInitialTickets()
	{
		return m_iInitialTickets;
	}

	void SetInitialTickets(int value)
	{
		m_iInitialTickets = ClampInt(value, 0, MAX_TICKET_VALUE);
	}

	int GetReplacementTicketCost()
	{
		return m_iReplacementTicketCost;
	}

	void SetReplacementTicketCost(int value)
	{
		m_iReplacementTicketCost = ClampInt(value, 1, MAX_TICKET_VALUE);
	}

	int GetReinforcementDelayMs()
	{
		return m_iReinforcementDelayMs;
	}

	void SetReinforcementDelayMs(int value)
	{
		m_iReinforcementDelayMs = ClampInt(value, 0, MAX_DELAY_MS);
	}

	int GetCommanderIntervalMs()
	{
		return m_iCommanderIntervalMs;
	}

	void SetCommanderIntervalMs(int value)
	{
		m_iCommanderIntervalMs = ClampInt(value, MIN_COMMANDER_INTERVAL_MS, MAX_COMMANDER_INTERVAL_MS);
	}

	// Soldiers per side at most (0: none): squads and recruits wait while their side is at it. Martin, 24 Oct: "maybe 150
	// soldiers is too much - we should cap it; start with 50 vs 50" (FPS fell from ~37 at 125 AI soldiers to ~17 at 240).
	int GetMaxAgentsPerFaction()
	{
		return m_iMaxAgentsPerFaction;
	}

	int GetMaxManagedAgents()
	{
		return m_iMaxManagedAgents;
	}

	void SetMaxManagedAgents(int value)
	{
		m_iMaxManagedAgents = ClampInt(value, MIN_MANAGED_AGENTS, MAX_MANAGED_AGENTS);
	}

	int GetWarTempoPercent()
	{
		return m_iWarTempoPercent;
	}

	void SetWarTempoPercent(int value)
	{
		m_iWarTempoPercent = ClampInt(value, 25, 400);
		m_iReinforcementDelayMs = ClampInt(
			DEFAULT_REINFORCEMENT_DELAY_MS * 100 / m_iWarTempoPercent,
			0,
			MAX_DELAY_MS);
		m_iCommanderIntervalMs = ClampInt(
			DEFAULT_COMMANDER_INTERVAL_MS * 100 / m_iWarTempoPercent,
			MIN_COMMANDER_INTERVAL_MS,
			MAX_COMMANDER_INTERVAL_MS);
	}

	FactionKey GetExpectedPlayerFaction()
	{
		return m_sExpectedPlayerFaction;
	}

	void SetExpectedPlayerFaction(FactionKey factionKey)
	{
		m_sExpectedPlayerFaction = string.Empty;
		if (factionKey == "US" || factionKey == "USSR")
			m_sExpectedPlayerFaction = factionKey;
	}

	bool GetRequirePlayerForResult()
	{
		return m_bRequirePlayerForResult;
	}

	void SetRequirePlayerForResult(bool required)
	{
		m_bRequirePlayerForResult = required;
	}

	string GetAICommanderMode()
	{
		return m_sAICommanderMode;
	}

	void SetAICommanderMode(string mode)
	{
		m_bAICommanderModeValid = mode == AI_COMMANDER_MODE_BOTH ||
			mode == AI_COMMANDER_MODE_US ||
			mode == AI_COMMANDER_MODE_USSR;
		if (!m_bAICommanderModeValid)
		{
			m_sInvalidAICommanderMode = mode;
			return;
		}

		m_sAICommanderMode = mode;
		m_sInvalidAICommanderMode = string.Empty;
	}

	bool IsAICommanderModeValid()
	{
		return m_bAICommanderModeValid;
	}

	string GetInvalidAICommanderMode()
	{
		return m_sInvalidAICommanderMode;
	}

	bool GetActiveForcesRolesEnabled()
	{
		return m_bActiveForcesRolesEnabled;
	}

	void SetActiveForcesRolesEnabled(bool enabled)
	{
		m_bActiveForcesRolesEnabled = enabled;
	}

	int GetRoleMinimumDwellMs()
	{
		return m_iCommanderIntervalMs * ROLE_MINIMUM_DWELL_INTERVALS;
	}

	protected void ApplyCLIOverrides()
	{
		string value;
		// Apply the coarse preset first. Explicit timing overrides below always win.
		if (System.GetCLIParam("aicfWarTempoPercent", value))
			SetWarTempoPercent(value.ToInt());
		if (System.GetCLIParam("aicfInitialTickets", value))
			SetInitialTickets(value.ToInt());
		if (System.GetCLIParam("aicfReplacementTicketCost", value))
			SetReplacementTicketCost(value.ToInt());
		if (System.GetCLIParam("aicfReinforcementDelayMs", value))
			SetReinforcementDelayMs(value.ToInt());
		if (System.GetCLIParam("aicfCommanderIntervalMs", value))
			SetCommanderIntervalMs(value.ToInt());
		if (System.GetCLIParam("aicfMaxManagedAgents", value))
			SetMaxManagedAgents(value.ToInt());
		if (System.GetCLIParam("aicfMaxAgentsPerFaction", value))
			m_iMaxAgentsPerFaction = ClampInt(value.ToInt(), 0, MAX_MANAGED_AGENTS);
		if (System.GetCLIParam("aicfExpectedPlayerFaction", value))
			SetExpectedPlayerFaction(value);
		if (System.GetCLIParam("aicfAICommanderMode", value))
			SetAICommanderMode(value);
		if (System.GetCLIParam("aicfRequirePlayerForResult", value))
			SetRequirePlayerForResult(value.ToInt() > 0);
		if (System.GetCLIParam("aicfActiveForcesRolesEnabled", value))
			SetActiveForcesRolesEnabled(value.ToInt() > 0);
	}

	protected int ClampInt(int value, int minimum, int maximum)
	{
		if (value < minimum)
			return minimum;
		if (value > maximum)
			return maximum;

		return value;
	}
}
