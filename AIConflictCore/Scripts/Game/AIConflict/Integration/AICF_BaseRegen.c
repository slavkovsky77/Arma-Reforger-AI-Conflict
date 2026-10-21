// ZCR (21 Oct, Martin: "supply trucks barely deliver" / "logistics is very important"): front bases filled up
// for free - 1250 -> 3000 in 4 minutes with no truck - so AI Conflict's trucks (which go below 60%) had
// nothing to do: 0.4 deliveries a watched match. A probe of the base's supply containers found it: the
// container the base's stock is summed from regains 100 every few seconds (resource gain from the base's
// encapsulator config), about 570 a minute. Non-HQ bases now get no free supplies - the gain is switched off
// every 5 s (the encapsulator can rebuild the container) - so they live on supply trucks; HQs and AI
// Conflict's supply depots keep theirs. Conflict's own regular income past a threshold is off too.
// Logs BASE_REGEN_OFF (bases and containers switched off, once a minute when it changes something).
// -aicfBaseRegen keeps the free refill.
class AICF_BaseRegen
{
	protected static int s_iSwitched;
	protected static int s_iNextLog;

	static void Start()
	{
		if (!Replication.IsServer() || System.IsCLIParam("aicfBaseRegen"))
			return;
		GetGame().GetCallqueue().CallLater(Tick, 5000, true);
	}

	protected static void Tick()
	{
		SCR_MilitaryBaseSystem system = SCR_MilitaryBaseSystem.GetInstance();
		if (!system)
			return;
		array<SCR_MilitaryBaseComponent> bases = {};
		system.GetBases(bases);
		int switched;
		foreach (SCR_MilitaryBaseComponent b : bases)
		{
			SCR_CampaignMilitaryBaseComponent base = SCR_CampaignMilitaryBaseComponent.Cast(b);
			if (!base || base.IsHQ() || base.GetType() == SCR_ECampaignBaseType.SOURCE_BASE || AICF_SupplyDepots.Is(base))
				continue;
			SCR_ResourceComponent resources = base.GetResourceComponent();
			if (!resources)
				continue;
			SCR_ResourceContainer own = resources.GetContainer(EResourceType.SUPPLIES);
			if (own && own.IsResourceGainEnabled())
			{
				own.EnableGain(false);
				switched++;
			}
			SCR_ResourceConsumer consumer = resources.GetConsumer(EResourceGeneratorID.DEFAULT, EResourceType.SUPPLIES);
			if (!consumer || !consumer.GetContainerQueue())
				continue;
			SCR_ResourceContainerQueueBase queue = consumer.GetContainerQueue();
			for (int i; i < queue.GetContainerCount(); i++)
			{
				SCR_ResourceContainer container = queue.GetContainerAt(i);
				if (container && container.IsResourceGainEnabled())
				{
					container.EnableGain(false);
					switched++;
				}
			}
		}
		s_iSwitched += switched;
		int now = System.GetTickCount();
		if (switched > 0 && now >= s_iNextLog)
		{
			s_iNextLog = now + 60000;
			AICF_Stage1Diagnostics.Info("BASE_REGEN_OFF", string.Format("containers_switched=%1 total=%2", switched, s_iSwitched));
		}
	}
}

modded class SCR_GameModeCampaign
{
	// Conflict's own regular income for non-HQ bases fills only up to this threshold: none.
	override int GetSuppliesReplenishThreshold()
	{
		if (System.IsCLIParam("aicfBaseRegen"))
			return super.GetSuppliesReplenishThreshold();
		return 0;
	}
}
