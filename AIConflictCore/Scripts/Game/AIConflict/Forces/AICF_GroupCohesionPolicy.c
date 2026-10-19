// Keeps managed fireteams in a compact stock formation without teleporting
// soldiers or overriding their combat reactions.
class AICF_GroupCohesionPolicy
{
	// A wedge, not single file (ZCR, 19 Oct - Martin: a whole squad walking in a line down a road
	// "would never happen").
	static const string FORMATION_NAME = "Wedge";

	bool Apply(SCR_AIGroup group)
	{
		if (!group)
			return false;

		bool applied;
		AIFormationComponent formation = AIFormationComponent.Cast(
			group.FindComponent(AIFormationComponent));
		if (formation && formation.SetFormation(FORMATION_NAME))
			applied = true;

		AIGroupMovementComponent movement = AIGroupMovementComponent.Cast(
			group.FindComponent(AIGroupMovementComponent));
		if (movement)
		{
			// Zero displacement keeps the default handler attached to the group.
			movement.SetFormationDisplacement(0);
			movement.SetFormationDefinition(AIGroupMovementComponent.DEFAULT_HANDLER_ID, FORMATION_NAME);
			applied = true;
		}

		// Teams jog between points (ZCR, 19 Oct - Martin: "they just walk to the points through no
		// cover"): the group's own movement speed, so the open stretches are crossed at a run, not
		// at a stroll. Fights keep the game's own combat movement.
		SCR_AIGroupSettingsComponent settings = SCR_AIGroupSettingsComponent.Cast(group.FindComponent(SCR_AIGroupSettingsComponent));
		if (settings)
			settings.AddSetting(SCR_AIGroupCharactersMovementSpeedSetting.Create(SCR_EAISettingOrigin.COMMANDING, EMovementType.RUN), false, true);

		return applied;
	}

	// Boarding may create extra movement handlers for vehicle subgroups. Call
	// this only after every protected member is verified outside the vehicle;
	// ordinary formation refreshes must not disrupt active movement handlers.
	bool NormalizeAfterVehicle(SCR_AIGroup group)
	{
		if (!group)
			return false;
		AIGroupMovementComponent movement = AIGroupMovementComponent.Cast(
			group.FindComponent(AIGroupMovementComponent));
		if (movement)
			movement.ClearGroupMoveHandlers();
		return Apply(group);
	}

	// A failed stock movement activity may leave per-agent movement handlers
	// behind. Persistent-stuck containment owns a fresh field-hold waypoint, so
	// it is safe to return every survivor to the default group handler first.
	bool NormalizeAfterMovementFailure(SCR_AIGroup group)
	{
		if (!group)
			return false;
		AIGroupMovementComponent movement = AIGroupMovementComponent.Cast(
			group.FindComponent(AIGroupMovementComponent));
		if (movement)
			movement.ClearGroupMoveHandlers();
		return Apply(group);
	}
}
