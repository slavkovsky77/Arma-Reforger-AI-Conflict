// ZCR (25 Oct - Martin: "why don't vehicles have tags with their mission and supplies? I would expect to see this"): which
// logistics worker drives a vehicle, for the watched game's vehicle tags (ZeusCommanderReforger ZCR_VehicleTag). Refreshed
// on every logistics tick from the depot registry's workers.
class AICF_LogisticsLookup
{
	protected static ref map<IEntity, AICF_LogisticsWorker> s_mByVehicle = new map<IEntity, AICF_LogisticsWorker>();

	static void Update(array<ref AICF_LogisticsWorker> workers)
	{
		s_mByVehicle.Clear();
		foreach (AICF_LogisticsWorker w : workers)
		{
			if (w && w.m_Vehicle)
				s_mByVehicle.Set(w.m_Vehicle, w);
		}
	}

	static AICF_LogisticsWorker Get(IEntity vehicle)
	{
		if (!vehicle)
			return null;
		return s_mByVehicle.Get(vehicle);
	}
}
