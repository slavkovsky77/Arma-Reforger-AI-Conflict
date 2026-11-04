# AI Conflict - Zeus Commander edition 0.1.0 (alpha)

4 November 2026. Branch `release-alpha` of slavkovsky77/Arma-Reforger-AI-Conflict (from `zcr`). Not an AI Conflict
release by Ishafel: a modified edition for Zeus Commander Reforger, on top of AI Conflict as of 17 Oct 2026 (8a790b7,
after 0.1.29). Licence Apache-2.0, as the original; what changed, file by file: NOTICE.md.

Shipped addons (Workshop, version 0.1.0):
- AI Conflict Core - Zeus Commander edition, GUID CF39E4A7CCBBF845 (was 9178E5822AFE48EA)
- AI Conflict Arland - Zeus Commander edition, GUID ACAA98B674F022BB (was B52C5F6AEDBF423E)
New GUIDs because the original is on the Workshop under the old ones; never load both.

New in this edition's release branch:
- AICF_Launch: launch parameters from the command line or, when not given there, from the scenario's mission header
  (m_sAICFLaunchParams) - a released scenario sets its features without command-line parameters. Every read of a
  launch parameter goes through it (reforger-commander tools/release/launch_params.py). Empty header field = as before.
- The licence and NOTICE inside the packed addons (AICF_LICENSE_NOTICE.c, comments only).

Everything else since the fork point (140 commits, 19 Oct - 4 Nov): see NOTICE.md and the commit history of `zcr`.

Checked: Workbench script validation in all configurations; packed with Workbench -packAddon; loaded headless with the
alpha scenario of Zeus Commander Reforger and only its dependencies, 0 script errors of ours.
