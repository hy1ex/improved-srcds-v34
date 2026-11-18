# Landed backports

Commits oldest → newest; each body carries the full explanation (why
upstream changed it, compatibility rating, build verification).

| Commit | Change |
|---|---|
| `6ded251` | SetParentAttachment -1-attachment guard; warn on destroying constraint-bound physics objects (nillerusr) |
| `9674cee` | util.cpp console-command hardening: format string, missing debugoverlay, NULL local player |
| `d97f4cd` | CBaseAnimating hardened against stale/invalid model state |
| `e2b035b` | CNavMesh::GetGroundHeight → upstream iterative ground probe |
| `dd29423` | Debris stuck inside non-static physics objects (nillerusr penetration solver) |
| `ea02fbc` | IK_GROUND foot trace re-anchored to estimated floor height (CalculateIKLocks) |
| `9eb1faf` | IK step-height estimator reset on teleport (InitStepHeightAdjust) |
| `f3975e0` | Entity flame removed when it vanishes into a client-side ragdoll (BecomeRagdollOnClient) |
| `a448fed` | NULL-guard UTIL_PlayerByIndex( 1 ) in the dangerous-autosave timer paths |
| `2b122f2` | Validate numcmds against totalcmds before processing client usercmds (Asserts vanish under -DNDEBUG) |
| `b36c793` | NULL-guard g_pGameRules in the CServerGameClients::ClientConnect path |
| `2c563cc` | Clear the pending dangerous-autosave timer on level change (LevelInit) |
| `c5bd690` | Honor cl_interp when the server computes a player's interpolation time (lag-comp rewound less than the client interpolated) |
| `2ab23a1` | NULL-guard pPlayer in CServerGameClients::ClientSetupVisibility |
| `6d23314` | Entity I/O debug buffers passed to DevMsg as data, not format strings |
| `6c30f85` | Detect truncated output in V_snprintf/V_vsnprintf; fix size_t underflow in V_strncat |
| `99af5f1` | Guard the null-termination write in CUtlBuffer::AddNullTermination against a negative index |
| `5843414` | Clamp the hitbox cross-axis separation test with a projection epsilon (ClipRayToHitbox) |
| `e0f5723` | Tolerate a NULL value string in ConVar::InternalSetValue and ConVar::ChangeStringValue |
| `b888440` | README + this report brought back in sync with the tree (docs-only; FIXES.md frozen as archive) |
| `0f86f08` | `~CBaseEntity` clears the ground entity during teardown (3/3-ref consensus, finding 4) |
| `aa75b38` | nav_generate skips already-covered seeds instead of creating duplicate nodes (safe deviation from upstream's abort) |
| `615515d` | XZip PUTBYTE/PUTSHORT flush-bounds guard (3/3 refs; file is Windows-only dead source) |
| `0438a3a` | Physics-reasonableness guards in baseentity_shared: VPhysicsUpdate bounds checks (fixes the `IsFinite(angles.x)`-twice/never-z bug), impulse clamps, throttled spew |
| `6870376` | FireBullets clamps a startsolid trace back to its start position (Tracker 70354/63250 — out-of-range networked tracer endpos) |
| `9870ae4` | IntersectRayWithOBB transforms its result back to world space and un-doubles the fraction (3/3 refs; endpos was OBB-local, fraction was half the true value) |
| `cb7a5e0` | IntersectRayWithBox snaps a startsolid trace's startpos to the solid exit point (3/3 refs; pFractionLeftSolid param skipped — no callers) |
| `550029b` | ConVar::Create parses its default int with atoi, not a float cast (3/3 refs — >2^24 defaults lost precision) |
| `16970b2` | CUtlCharConversion table lookups cast the index to unsigned char (3/3 refs — signed-char negative subscripts) |
| `af15460` | CUtlBuffer::Get rejects non-positive sizes and asserts the read index (3/3 refs) |
| `ab8d754` | CUtlBuffer::SeekGet validates the landing position via OnGetOverflow (3/3 refs) |
| `78b6a2e` | NULL-guard the five shared physics object factories incl. PhysCreateWorld_Shared's vcollide checks (upstream consensus) |
| `1ff8704` | Bound the worldspawn fluid index before indexing the solids array (sdk2013; second site is dead USE_PHX_FILES code) |
| `454ca83` | STRING(classname) in the global-entity mismatch Warning (3/3 refs — strong string_t passed raw to %s) |
| `33ab533` | NULL-guard SaveInit's engine allocations (3/3 refs; upstream's error-path leak corrected) |
| `9ea3ba1` | CSave::WriteFunction writes a BADFUNCTIONPOINTER placeholder instead of dropping the field (3/3 refs — a dropped field desynced the save stream) |
| `19681c4` | Bound ParseFile's output token with a caller-supplied max length (3/3 refs; OUT_Z_* annotations dropped — absent from this tree) |
| `a5a21b6` | animation.cpp: AE_TYPE_SERVER event filter (3/3), negative bodygroup guards, FindBodygroupByName IsValid(), ExecuteNTimes log throttling |
| `1d60224` | sv_shootpositionfix reworked onto the ClientMod-Api/firebulletsfix algorithm: origin swapped back around PostThink, stock RunCommand order restored (hit-registration bug hunt) |
| `fd72abe` | CUtlVector::FastRemove skips the self-memcpy when removing the last element (3/3 refs) |
| `23d4f04` | CUtlRBTree::CopyFrom copies whole nodes instead of sizeof(T): truncated node array (sdk2013 fix) |
| `1af5d81` | Lag compensation teleport guard measures horizontal distance only (Length2DSqr + sv_lagcompensation_teleport_dist cvar, 3/3 refs + Valve 2011 fix): vertical game-code motion no longer aborts a backtrack or skips a restore |
| `1c8b4de` | Lag compensation records and restores the victim's pose parameters (`m_flPoseParameters[MAXSTUDIOPOSEPARAM]`) around a backtrack, mirroring sdk2013/CS:GO: rewound bones are rebuilt in the pose the shooter actually saw (CS:S players are LEGANIM_9WAY and `FinishMove` rewrites `body_pitch` every command), not in the current pose — a real contributor to model/hitbox misalignment |
| `b16c437` | `VPhysicsUpdatePusher` blocked-pusher rewind math corrected (moveback = movetime*fraction, gate start+0.99f, blocker NULL-check) — 3/3 refs |
| `2d068fd` | Local-setter physics-reasonableness guards: SetLocalOrigin/SetLocalVelocity/SetLocalAngularVelocity + new `IsEntityQAngleVelReasonable` helper |
| `b282b73` | `GetVelocity` NULL physics-object guard (4/4) + `AcceptInput` `DevMsg("%s", szBuffer)` format-string fix (3/3) |
| `8234594` | `SetMoveType` calls `UpdateWaterState()` for MOVETYPE_FLY/FLYWATER (3/3) |
| `c1fc932` | `TakeHealth` returns the healed delta and uses `GetMaxHealth()` (3/4) |
| `f20460c` | `VPhysicsShadowUpdate` ROTPUSH stuck-check + unified recovery + dead-player skip (3/3, `WasFrozen()` half ABI-deferred) |
| `7b0c54b` | `TraceAttack` NULL-attacker guard (3/3) |
| `3fc99ae` | `GetNetworkIDString` NULL-guards the engine network-ID string (3/4, csgo uses `"UNKNOWN"`) |
| `2e4e3a5` | Lag compensation refuses a second `Start` while a session is active (sdk2013 runtime guard + Finish/FrameUpdate self-clears; Finish made idempotent so a stray second Finish cannot replay the restore) |
| `411b183` | SetLightingOriginRelative DevWarnings wrap their `string_t` args in `STRING()` (3/3 refs; same defect class as `454ca83`) |
| `071ebd7` | CBaseAnimating constructor initializes networked `m_vecForce`/`m_nForceBone` unconditionally (was `_DEBUG`-only; 3/3 refs) |
| `0856942` | GetBoneCache rejects future-stamped cache entries and uses mask containment instead of exact equality (3/3 refs) |
| `26e2f2a` | HandleStartGestureSceneEvent bounds its faceposer tag copies (Q_strncpy) and STRING()s GetModelName() in the looping-gesture warning (3/3 refs) |
| `1329ddd` | AllocateLayer caps m_AnimOverlay at MAX_OVERLAYS (the SendPropUtlVector max) and returns -1 when full; paired Debug Assert dropped (3/3 refs) |
| `5f3fc10` | CAnimationLayer::Init zero-fills m_flBlendIn/m_flBlendOut/m_flLastEventCheck; RestartGesture resets the event check (2/3 refs) |
| `97b4fdd` | CPhysicsProp::ClearFlagsThink NULL-guards the physics object before clearing FVPHYSICS_WAS_THROWN (4/4 refs) |
| `652c93e` | VPhysicsCollision passes the thrown prop as pObject to g_interactionHitByPlayerThrownPhysObj (4/4 refs) |
| `0b80389` | HandleAnyCollisionInteractions does not impale NPCs friendly (D_LI) to the prop's physics attacker (4/4 refs) |
| `646b31f` | GetRootPhysicsObjectForBreak requires a bone follower before indexing follower 0 (4/4 refs) |
| `819168a` | ShouldSolvePenetration returns 0 for the COLLSTATE_TRYDISABLE branch so the solver stops fighting the sleep path (4/4 refs) |
| `eafc450` | CPhysMagnet::ConstraintBroken NULL-guards the magnetized entity handle and its physics object (4/4 refs) |
| `eff5879` | CPhysImpact::InputImpact re-traces from start + dir*0.1 when the first trace starts solid (3/3 refs) |
| `0fbbeef` | CBreakable::Die only emits chunk models when a gib set is configured and the chunk lookup succeeded (3/3 refs) |
| `00e96ae` | CBreakable::OnTakeDamage drops the unused inflictor-origin computation that could dereference a NULL inflictor (2/2 refs) |
| `1c783c2` | PhysicsStep feeds the vphysics shadow position before the think-tick gate instead of only on think frames (3/3 refs) |
| `203a9aa` | CBoneFollower::Init only marks the follower physically controlled for NPC owners, whose followers vphysics has to move (4/4 refs) |
| `f3c73b5` | LevelInit assigns eLoadType before BeginRestoreEntities/LoadGameState so restore paths see the right load type (2/2 refs) |
| `f55c2a1` | LevelShutdown holds MDLCACHE_CRITICAL_SECTION across entity teardown and model release (3/3 refs) |
| `d8abc9f` | CLogicBranchList::DoTest guards the branch handle so a removed logic_branch no longer faults (4/4 refs) |
| `1ab83b7` | Non-solid-to-player doors also get FL_UNBLOCKABLE_BY_PLAYER so players in the doorway stop blocking the push (3/3 refs) |
| `1136d95` | trigger_look parks its timeout think with TICK_NEVER_THINK; trigger_hurt skips disconnected players (4/4, 2/2 refs) |
| `8ef3c55` | momentary_rot_button clamps its move interval to whole ticks and arms the move think only when idle (4/4 refs) |
| `cbc7509` | Ragdoll prop respects RagdollGetBoneMatrix failure, solves separation after the read and guards a null object (3/3 refs) |
| `a8dc7bd` | filter_multi rejects non-filter entities instead of calling a virtual through the wrong vtable (3/3 refs) |
| `e819563` | world StaticDecal tests the trace entity for NULL before truncating it to the decal entity index (3/3 refs) |
| `3c8e6cf` | Physics NPC/entity solvers guard a cleared EHANDLE and a missing physics object instead of dereferencing them (4/4 refs) |
| `af84694` | soundent timestamps its pool diagnostics and warns when the pool is undersized for the client count (3/3 refs) |
| `56bc2ad` | ambient_generic initialises its modulation params once in Spawn instead of on every re-precache (3/3 refs) |
| `f2e5d25` | Team recipient filters include spectators watching the filtered team (2/2 refs) |
