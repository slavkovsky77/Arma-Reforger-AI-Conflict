// Launch parameters, from the command line or from the scenario (Zeus Commander Reforger edition, 4 Nov 2026).
//
// Why: this edition and Zeus Commander turn their features on with launch parameters (-aicfSquadsPerSide 12,
// -zcrAttackPlan...). A released scenario can't ask players to type those, so its mission header carries them:
// m_sAICFLaunchParams, written as on a command line ("-name" or "-name value"). Every read of a parameter goes through
// AICF_Launch.Has / AICF_Launch.Get instead of System.IsCLIParam / System.GetCLIParam:
//   - a parameter on the command line always wins (a server admin can still change any of them);
//   - otherwise the scenario's own value counts;
//   - a scenario with an empty m_sAICFLaunchParams (every scenario of AI Conflict itself, and the lab's) behaves exactly
//     as before: only the command line counts.
// The first time the scenario's parameters are known they are printed once:
//   [AICF][LAUNCH] scenario=<header> params=<n> list=<...>
// and any of them that was asked for before the mission header existed (so the code saw "off") is named:
//   [AICF][LAUNCH] asked before the scenario was known: <names>

modded class SCR_MissionHeaderCampaign
{
	[Attribute("", UIWidgets.EditBox, "Launch parameters this scenario turns on, as on a command line (-name or -name value). The command line wins.")]
	string m_sAICFLaunchParams;
}

class AICF_Launch
{
	protected static ref map<string, string> s_mScenario;
	protected static bool s_bKnown;
	protected static ref set<string> s_sAskedEarly;

	// Is the parameter set (the command line, else the scenario)?
	static bool Has(string name)
	{
		if (System.IsCLIParam(name))
			return true;
		if (!Known(name))
			return false;
		return s_mScenario.Contains(name);
	}

	// The parameter's value (the command line, else the scenario); false when it isn't set.
	static bool Get(string name, out string value)
	{
		if (System.GetCLIParam(name, value))
			return true;
		if (!Known(name))
			return false;
		return s_mScenario.Find(name, value);
	}

	// The scenario's parameters, read once the mission header exists (not latched before: a read during the game's start
	// would otherwise fix "no parameters" for the whole match).
	protected static bool Known(string name)
	{
		if (s_bKnown)
			return true;
		// Some reads come while the scripts load (a global object's constructor), before the game itself exists.
		ArmaReforgerScripted game = GetGame();
		SCR_MissionHeaderCampaign header;
		if (game)
			header = SCR_MissionHeaderCampaign.Cast(game.GetMissionHeader());
		if (!header)
		{
			if (!s_sAskedEarly)
				s_sAskedEarly = new set<string>();
			s_sAskedEarly.Insert(name);
			return false;
		}
		s_bKnown = true;
		s_mScenario = Parse(header.m_sAICFLaunchParams);
		if (s_mScenario.IsEmpty())
			return true;
		string list;
		foreach (string key, string val : s_mScenario)
		{
			if (!list.IsEmpty())
				list += " ";
			list += key;
			if (!val.IsEmpty())
				list += "=" + val;
		}
		Print(string.Format("[AICF][LAUNCH] scenario=%1 params=%2 list=%3", header.m_sName, s_mScenario.Count(), list), LogLevel.NORMAL);
		if (s_sAskedEarly)
		{
			string early;
			foreach (string asked : s_sAskedEarly)
			{
				if (!s_mScenario.Contains(asked))
					continue;
				if (!early.IsEmpty())
					early += " ";
				early += asked;
			}
			if (!early.IsEmpty())
				Print("[AICF][LAUNCH] asked before the scenario was known: " + early, LogLevel.WARNING);
		}
		return true;
	}

	// "-a -b 12 -c=x -d y" -> a:"", b:"12", c:"x", d:"y". A word after a parameter is its value unless it starts with "-"
	// followed by a letter (so a negative number is still a value).
	static map<string, string> Parse(string text)
	{
		map<string, string> result = new map<string, string>();
		array<string> words = {};
		string clean = text;
		clean.Replace("\t", " ");
		clean.Replace("\n", " ");
		clean.Replace("\r", " ");
		clean.Split(" ", words, true);
		string key;
		foreach (string w : words)
		{
			if (IsName(w))
			{
				key = w.Substring(1, w.Length() - 1);
				int eq = key.IndexOf("=");
				if (eq > 0)
				{
					result.Set(key.Substring(0, eq), key.Substring(eq + 1, key.Length() - eq - 1));
					key = string.Empty;
					continue;
				}
				result.Set(key, string.Empty);
				continue;
			}
			if (!key.IsEmpty())
			{
				result.Set(key, w);
				key = string.Empty;
			}
		}
		return result;
	}

	protected static bool IsName(string w)
	{
		if (w.Length() < 2 || w.Get(0) != "-")
			return false;
		string c = w.Get(1);
		return c.ToAscii() >= 65 && c.ToAscii() <= 122;   // a letter (A-Z, a-z and the few signs between)
	}
}
