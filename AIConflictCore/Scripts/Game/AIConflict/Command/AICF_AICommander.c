// Faction-scoped facade for autonomous strategic decisions. It never owns
// spawning, tickets, economy, transport, lifecycle, or waypoint entities.
class AICF_AICommander
{
	protected FactionKey m_sFactionKey;
	protected ref AICF_CommandAuthorityPolicy m_AuthorityPolicy;
	protected AICF_FactionState m_FactionState;
	protected SCR_CampaignFaction m_Faction;
	protected AICF_OrderPlanner m_OrderPlanner;
	protected AICF_ObjectiveGraph m_ObjectiveGraph;
	protected AICF_TargetSelector m_TargetSelector;
	protected ref AICF_AdaptiveCommanderPolicy m_AdaptivePolicy;

	void AICF_AICommander(
		AICF_FactionState factionState,
		SCR_CampaignFaction faction,
		AICF_CommandAuthorityPolicy authorityPolicy,
		AICF_OrderPlanner orderPlanner,
		AICF_ObjectiveGraph objectiveGraph,
		AICF_TargetSelector targetSelector)
	{
		m_FactionState = factionState;
		m_Faction = faction;
		m_AuthorityPolicy = authorityPolicy;
		m_OrderPlanner = orderPlanner;
		m_ObjectiveGraph = objectiveGraph;
		m_TargetSelector = targetSelector;
		m_AdaptivePolicy = new AICF_AdaptiveCommanderPolicy(factionState, faction, objectiveGraph, targetSelector);
		if (m_Faction)
			m_sFactionKey = m_Faction.GetFactionKey();
	}

	FactionKey GetFactionKey()
	{
		return m_sFactionKey;
	}

	AICF_FactionState GetFactionState()
	{
		return m_FactionState;
	}

	SCR_CampaignFaction GetFaction()
	{
		return m_Faction;
	}

	bool IsEnabled()
	{
		return m_AuthorityPolicy && m_AuthorityPolicy.IsValid() &&
			m_FactionState && m_Faction &&
			m_AuthorityPolicy.IsAICommanderEnabled(m_sFactionKey);
	}

	int SelectConstructionType(array<bool> covered, int startType)
	{
		if (!Replication.IsServer() || !IsEnabled() || !covered || covered.Count() != AICF_EConstructionType.COUNT)
			return -1;
		for (int offset; offset < AICF_EConstructionType.COUNT; offset++)
		{
			int type = (startType + offset) % AICF_EConstructionType.COUNT;
			if (!covered[type])
				return type;
		}
		return -1;
	}

	// ZCR (21 Oct): building's navmesh queries go through an empty group of their own at the main base, which
	// nobody fights. Borrowed from the side's first living squad, the context changed whenever that squad was
	// wiped out, and every building order in progress was cancelled and searched again
	// (NAVMESH_CONTEXT_CHANGED: 1-2 fortifications a watched match, and a first depot in a headless test).
	// -aicfBorrowedPathfinding: borrowed, as before. Logs CONSTRUCTION_PATH_GROUP.
	protected SCR_AIGroup m_PathGroup;
	protected int m_iPathGroupTries;

	// Read-only infantry navmesh context; не создаёт group/worker или assignment.
	AIPathfindingComponent GetConstructionPathfinding()
	{
		if (!IsEnabled())
			return null;
		if (!m_PathGroup && m_iPathGroupTries < 3 && m_Faction && m_Faction.GetMainBase() && !System.IsCLIParam("aicfBorrowedPathfinding"))
		{
			m_iPathGroupTries++;
			Resource res = Resource.Load("{000CD338713F2B5A}Prefabs/AI/Groups/Group_Base.et");
			if (res && res.IsValid())
			{
				EntitySpawnParams params = new EntitySpawnParams();
				params.TransformMode = ETransformMode.WORLD;
				params.Transform[3] = m_Faction.GetMainBase().GetOwner().GetOrigin();
				m_PathGroup = SCR_AIGroup.Cast(GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params));
			}
			AIPathfindingComponent made;
			if (m_PathGroup)
				made = AIPathfindingComponent.Cast(m_PathGroup.FindComponent(AIPathfindingComponent));
			AICF_Stage1Diagnostics.Info("CONSTRUCTION_PATH_GROUP", string.Format("faction=%1 spawned=%2 navmesh=%3 try=%4", m_sFactionKey,
				m_PathGroup != null, made && made.GetNavmeshComponent(), m_iPathGroupTries));
		}
		if (m_PathGroup)
		{
			AIPathfindingComponent own = AIPathfindingComponent.Cast(m_PathGroup.FindComponent(AIPathfindingComponent));
			if (own && own.GetNavmeshComponent())
				return own;
		}
		for (int i; i < AICF_Stage1Config.GROUP_SLOTS_PER_FACTION; i++)
		{
			AICF_GroupSlot slot = m_FactionState.GetSlot(i);
			if (slot && slot.GetGroup() && slot.GetGroup().GetFaction() == m_Faction)
				return AIPathfindingComponent.Cast(slot.GetGroup().FindComponent(AIPathfindingComponent));
		}
		return null;
	}

	bool OwnsSlot(AICF_GroupSlot slot)
	{
		return IsEnabled() && slot && slot.GetSlotId() >= 0 &&
			m_FactionState.GetSlot(slot.GetSlotId()) == slot;
	}

	bool Tick(AICF_MatchController matchController, string reason)
	{
		if (!Replication.IsServer() || !IsEnabled() || !matchController)
			return false;

		return matchController.RunAICommanderTick(this, reason);
	}

	bool AssignOrder(
		AICF_GroupSlot slot,
		string reason,
		SCR_CampaignMilitaryBaseComponent excludedTarget = null,
		bool waypointSuspendedByVehicle = false)
	{
		if (!Replication.IsServer() || !OwnsSlot(slot) || !m_OrderPlanner || !m_ObjectiveGraph ||
			!m_TargetSelector)
			return false;

		m_OrderPlanner.PrepareAICommanderSelection(slot, m_Faction, reason);
		AICF_AdaptiveDecision decision = m_AdaptivePolicy.Select(slot, excludedTarget);
		return m_OrderPlanner.AssignAICommanderOrder(
			slot,
			m_Faction,
			m_ObjectiveGraph,
			m_TargetSelector,
			reason,
			excludedTarget,
			waypointSuspendedByVehicle,
			decision);
	}

	bool ReconcileStrategicOrder(
		AICF_GroupSlot slot,
		string reason,
		int minimumDwellMs,
		int stableCandidateMs,
		bool waypointSuspendedByVehicle = false)
	{
		if (!Replication.IsServer() || !OwnsSlot(slot) || !m_OrderPlanner || !m_ObjectiveGraph ||
			!m_TargetSelector)
			return false;

		m_OrderPlanner.PrepareAICommanderSelection(slot, m_Faction, reason);
		AICF_AdaptiveDecision decision = m_AdaptivePolicy.Select(slot);
		return m_OrderPlanner.ReconcileAICommanderOrder(
			slot,
			m_Faction,
			m_ObjectiveGraph,
			m_TargetSelector,
			reason,
			minimumDwellMs,
			stableCandidateMs,
			waypointSuspendedByVehicle,
			decision);
	}

	bool AssignLossResponseOrder(
		AICF_GroupSlot slot,
		SCR_CampaignMilitaryBaseComponent lostBase,
		int minimumDwellMs,
		int stableCandidateMs,
		bool waypointSuspendedByVehicle = false)
	{
		if (!Replication.IsServer() || !OwnsSlot(slot) || !m_OrderPlanner || !m_ObjectiveGraph ||
			!m_TargetSelector || !lostBase)
			return false;

		m_AdaptivePolicy.RecordLoss(lostBase);
		m_OrderPlanner.PrepareAICommanderSelection(slot, m_Faction, "BASE_OWNER_CHANGED_LOSS");
		AICF_AdaptiveDecision decision = m_AdaptivePolicy.Select(slot);
		return m_OrderPlanner.AssignAICommanderLossResponseOrder(
			slot,
			m_Faction,
			m_ObjectiveGraph,
			m_TargetSelector,
			lostBase,
			minimumDwellMs,
			stableCandidateMs,
			waypointSuspendedByVehicle,
			decision);
	}
}
