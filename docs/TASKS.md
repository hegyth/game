# VELRATH - Combat System 1.0 Backlog

Status: implementation backlog derived from `GAME_VISION.md`, `ARCHITECTURE.md`, and `combat/COMBAT_SPEC.md`. This file orders work; it does not change approved mechanics, values, or timing.

## References and Status

- **VISION:** [`GAME_VISION.md`](GAME_VISION.md)
- **ARCH:** [`ARCHITECTURE.md`](ARCHITECTURE.md)
- **CS:** [`combat/COMBAT_SPEC.md`](combat/COMBAT_SPEC.md)
- Task status is one of `TODO`, `IN PROGRESS`, `BLOCKED`, or `DONE`.
- Priority is `P0` (critical foundation), `P1` (core combat), or `P2` (polish/expansion).
- A task is `DONE` only after its Acceptance Criteria and Verification Method pass.

### Current implementation baseline

- One `VELRATH` runtime module exists and builds as Game/Editor.
- `AVelrathCharacter` owns a replicated ASC in `Mixed` mode and initializes Actor Info in `PossessedBy` and `OnRep_PlayerState`.
- The current base `UAttributeSet` is a placeholder; no approved attributes exist yet.
- `AVelrathGameMode` selects `AVelrathCharacter` as the default pawn.
- The current automation test verifies module loading only.
- Dedicated Server cannot be built with the current Launcher UE 5.7.4 distribution.

## Implementation Rules

1. Implement the smallest working behavior for the current task only.
2. Add deterministic automated coverage as soon as the behavior has a stable C++ contract.
3. Add multi-instance verification after local deterministic behavior passes.
4. Add final visual/animation polish after authority and replication are correct.
5. Do not rewrite working code unless a task's Acceptance Criteria require it.
6. Do not edit binary Unreal assets directly. Tasks requiring Input, Blueprint, montage, cue, or Data Asset authoring must identify the required Editor-side step.
7. Use `Scripts/Build.ps1 -Target Editor` after C++ changes and `Scripts/Test.ps1` with the narrowest relevant filter.
8. Never treat a single-instance run as multiplayer verification.
9. Do not run the Server target with the Launcher engine.

## Critical Path to the First Local 1v1 Duel

The first local duel milestone is intentionally minimal: two PIE/listen-server players can move, use Light Attack, receive authoritative damage, and observe replicated HP. Heavy Attack, defense, Echo, scoring, and final presentation are not required for this milestone.

```text
FND-001 (done)
  -> FND-002 -> FND-003 -> FND-004 -> FND-005 -> FND-006
  -> GAS-002 -> GAS-003 -> GAS-004 -> GAS-005 -> GAS-006
  -> GAS-007 -> GAS-008 -> GAS-009 -> GAS-010
  -> CMB-001 -> CMB-002 -> CMB-003 -> CMB-004
  -> CMB-006 -> CMB-007 -> CMB-008 -> CMB-010 -> CMB-011
  -> NET-001 -> NET-002 -> NET-003 -> NET-005
```

Decision gates on this path: `TD-01`, `TD-02`, `TD-04`, and `TD-22`. `TD-05` is required before same-tick deterministic combat is considered complete, but not before the first single-Light-Attack duel demonstration. `FND-005` requires approved Editor-authored Enhanced Input assets because Codex must not directly modify binary assets.

## Stage 1 - Foundation

| Task ID | Priority | Title | Goal and Scope | Dependencies | Acceptance Criteria | Verification Method | Status | References |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| FND-001 | P0 | Inventory existing foundation | Confirm module, targets, Character, ASC, GameMode, scripts, and smoke test without refactoring. | None | Existing types and current gaps are recorded in this backlog; no generated folders are treated as source. | Relevant-file inspection and current smoke-test history. | DONE | ARCH 1, 3, 4; CS 1 |
| FND-002 | P0 | Apply baseline movement parameters | Configure capsule, walk/sprint speed, acceleration, and deceleration from CS. Do not add movement mechanics. | FND-001 | Runtime values match `MOV-01` and `MOV-02`; Editor target builds. | Focused C++ automation/configuration test plus Editor build. | DONE | CS 4; CS MOV-01/02 |
| FND-003 | P0 | Third-person camera foundation | Add minimal spring-arm/camera ownership and elevated third-person presentation with configurable framing; no final tuning values are invented. | FND-001 | Local player has a stable third-person camera; camera code contains no combat authority. | Editor build and one-player PIE visual check. | DONE | VISION 1, 9; ARCH 3 |
| DEV-001 | P0 | Development map and visible Character | Add the persistent `L_Dev` test map, PlayerStart, minimal lighting/geometry, and a C++-derived `BP_VelrathCharacter` using available standard Unreal mannequin assets. | FND-003 | Startup/default map resolves to `L_Dev`; GameMode spawns and possesses `BP_VelrathCharacter`; mesh and camera are visible and functional in one-player PIE; map and assets load without critical errors. | Editor build, asset commandlet validation, automation suite, and rendered one-player PIE screenshot. | DONE | VISION 9; ARCH 3, 8, 12 |
| FND-004 | P0 | Enhanced Input C++ bridge | Define input-facing C++ contract and bind movement/look/action inputs without implementing abilities. GAS input-tag routing is added only after GAS-002. | FND-001 | Input callbacks are narrow, local-only intent; no direct damage/state mutation; build passes. | C++ automation where possible, Editor build, local input log check. | DONE | VISION 1; ARCH 3, 9, 11 |
| FND-005 | P0 | Enhanced Input assets and mapping | Create/assign Input Actions and Mapping Context for WASD, mouse, Light, Heavy, Block, Parry, Dodge, and Echo through Unreal Editor. Codex only wires references/schema. | FND-004 | Every approved input resolves to exactly one binding; binary assets compile in Editor; no unapproved action exists. | Human/Editor asset authoring, Blueprint/asset validation, PIE input check. | BLOCKED | VISION 1, 6; ARCH 9; CS 5 |
| FND-006 | P0 | Locomotion input behavior | Route WASD/mouse intent through Enhanced Input to Character Movement; implement backward multiplier only after its composition rule is isolated. | FND-002, FND-003, FND-005 | Character moves and looks locally; server movement replicates through Character Movement; no custom movement RPC. | Listen-server PIE with two instances and movement correction observation. | IN PROGRESS | CS 4; CS MOV-03; ARCH 7 |
| FND-007 | P1 | Foundation movement tests | Add deterministic checks for configured movement/capsule values and a repeatable multi-instance movement checklist. | FND-006 | MOV-01/02 pass; MOV-03/04 are either executable and pass or explicitly remain blocked by their decision/task. | `Scripts/Test.ps1` focused filter plus two-instance PIE checklist. | IN PROGRESS | CS 18 Movement; ARCH 12 |

## Stage 2 - GAS

| Task ID | Priority | Title | Goal and Scope | Dependencies | Acceptance Criteria | Verification Method | Status | References |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| GAS-001 | P0 | Character-owned ASC baseline | Preserve replicated Character-owned ASC, `Mixed` mode, and server/owning-client Actor Info initialization. | FND-001 | Existing code builds and smoke test loads module; no ability grant is coupled to Actor Info initialization. | Editor build and source inspection. | DONE | ARCH AD-002; ARCH 4; CS 1 |
| GAS-002 | P0 | Canonical native gameplay tags | Define central native tags for input, combat states, control, life state, ability identity, events, and Echo lifecycle used by current specs only. | FND-001 | Every tag has one definition and documented meaning; no string-literal tag usage in feature code; build passes. | Native tag automation test and Editor build. | DONE | ARCH 4, 9; CS 5 |
| GAS-003 | P0 | UVelrathAttributeSet Health | Replace placeholder base set with project AttributeSet containing Health and MaxHealth with accessors/invariants; do not implement damage yet. | GAS-001 | Health/MaxHealth exist, clamp correctly, default maximum can represent 1000, and Character owns the set. | Attribute automation tests and Editor build. | DONE | ARCH 4; CS 8, 9; CS STA/DMG criteria |
| GAS-004 | P0 | Add Stamina attributes | Add Stamina and MaxStamina to the project AttributeSet without regeneration/upkeep logic. | GAS-003 | Stamina/MaxStamina exist, clamp correctly, and can represent maximum 100. | Attribute automation tests and Editor build. | DONE | CS 9; CS STA-01 |
| GAS-005 | P0 | Attribute replication | Replicate approved attributes with `GetLifetimeReplicatedProps`, RepNotify, and GAS notification macros. | GAS-003, GAS-004 | Server, owner, and remote client converge on Health/MaxHealth/Stamina/MaxStamina in multi-instance PIE. | Focused automation plus two-instance PIE value comparison. | DONE | ARCH 4, 7, 12; CS 15; NET-01 |
| GAS-006 | P0 | Default attribute initialization | Create the single approved data/effect path that initializes 1000 HP and 100 Stamina on each new Character. | GAS-005, TD-22 | Defaults apply once on server after Actor Info init, replicate, and do not survive Character replacement except by new initialization. | Spawn/respawn automation test and two-instance PIE. | BLOCKED | ARCH 4, 9; CS 9, 13 |
| GAS-007 | P0 | Startup ability-set schema | Define a minimal configurable ability-set schema for shared foundational abilities; no archetype-specific behavior. | GAS-002, TD-22 | Schema references ability/effect classes, preserves granted handles, and contains no duplicate balance source. | Data validation automation and Editor build. | BLOCKED | ARCH 4, 9; CS 13 |
| GAS-008 | P0 | Server-only startup grants | Grant the shared startup ability set exactly once per Character on the server, separately from Actor Info initialization. | GAS-006, GAS-007 | Duplicate possession/init callbacks cannot duplicate abilities; handles are retained; client cannot grant. | Server automation test and two-instance PIE ability-list comparison. | TODO | ARCH 4 Startup grants; CS STATE-04 |
| GAS-009 | P0 | Possession and respawn ASC initialization | Verify server `PossessedBy`, owning-client init, pawn replacement, and cleanup without moving ASC to PlayerState. | GAS-008 | New Character gets fresh ASC state and one grant sequence; old state is retired; remote view has no stale effects. | Multi-instance PIE possession/respawn test. | TODO | ARCH AD-002/003; CS 5, 15; STATE-04 |
| GAS-010 | P0 | GAS foundation regression suite | Consolidate tags, attributes, initialization, replication, and duplicate-grant tests under a stable filter. | GAS-002 through GAS-009 | Focused suite executes at least one test, reports zero failures/notRun, and covers owner/server/remote expectations where relevant. | `Scripts/Test.ps1 -TestFilter VELRATH.GAS`. | TODO | ARCH 12; CS TEST-01/02/03 |

## Stage 3 - Combat

| Task ID | Priority | Title | Goal and Scope | Dependencies | Acceptance Criteria | Verification Method | Status | References |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| CMB-001 | P0 | 60 Hz combat clock | Implement the approved logical simulation scheduler without changing Unreal physics protocol. | GAS-010, TD-01, TD-02 | Integer authoritative ticks advance with approved catch-up/drift behavior and expose stable test hooks. | Deterministic clock tests under variable frame delta plus Editor build. | BLOCKED | CS 3; TD-01/02 |
| CMB-002 | P0 | Combat state tags and transitions | Implement known Locomotion/action/control/life tags, activation requirements, cleanup, and approved overrides. Leave unresolved transitions unsupported. | GAS-002, CMB-001 | Known state transitions and Dead/control overrides pass STATE-01/02/03/05; no parallel state framework. | State-transition automation tests. | TODO | CS 5; ARCH 5 |
| CMB-003 | P0 | Ability tick-window support | Provide the minimum shared mechanism for abilities to evaluate startup/active/recovery/cancel ticks from the combat clock. | CMB-001, CMB-002 | Half-open windows are exact, deterministic, and reusable by Light/Heavy without a broad ability hierarchy. | Boundary tests for every start/end tick. | TODO | CS 3, 6; ATK-01/02 |
| CMB-004 | P0 | Light Attack ability shell | Implement Light activation, state, 36-tick phases, zero Stamina cost, movement state, and cancel gates without hit resolution. | FND-005, GAS-008, CMB-003 | Light phases and tick 27/29 cancel boundaries pass ATK-01/03/05; no damage occurs yet. | Focused ability timing tests and local PIE. | TODO | CS 6 Light; CS ATK-01/03/05 |
| CMB-005 | P1 | Heavy Attack ability shell | Implement Heavy activation, state, 60-tick phases, zero Stamina cost, movement state, and cancel gates without hit resolution. | CMB-003, CMB-004 | Heavy phases and tick 50/54 cancel boundaries pass ATK-02/04/05. | Focused ability timing tests and local PIE. | TODO | CS 6 Heavy; CS ATK-02/04/05 |
| CMB-006 | P0 | Hurtbox and HitVolume contract | Implement the approved target hurtbox and attack sweep representation for Light/Heavy only. | FND-002, TD-04 | Collision roles are distinct; authoritative query shape is testable; no client authority. | Geometry/unit tests and server debug draw in PIE. | BLOCKED | CS 7; TD-04 |
| CMB-007 | P0 | AttackInstanceId and hit deduplication | Assign authoritative attack IDs and prevent more than one resolution per target per attack. | CMB-004, CMB-006 | HIT-03 passes across repeated sweeps and multiple hurtboxes. | Deterministic server automation test. | TODO | CS 7; HIT-03 |
| CMB-008 | P0 | Server Hit Detection | Execute Light/Heavy range, angle, and server sweep checks only during each attack Active interval. | CMB-005, CMB-006, CMB-007 | HIT-01/02/04 pass; client-only result cannot mutate state. | Server functional tests at boundary distances/angles. | TODO | CS 6, 7; HIT-01/02/04 |
| CMB-009 | P1 | Same-tick batch and deterministic order | Resolve mutual candidates from one snapshot in a stable order. | CMB-008, TD-05 | HIT-05/06 pass repeatedly with identical results. | Seeded same-tick automation tests and logs. | BLOCKED | CS 7; HIT-05/06; TD-05 |
| CMB-010 | P0 | Damage execution calculation | Implement server `RoundHalfUp` formula and approved modifiers via GAS effect execution/context. | GAS-003, CMB-008 | DMG-01/02/03/04/05 arithmetic passes without applying death yet. | Pure calculation tests plus effect-spec test. | TODO | CS 8; ARCH 5 |
| CMB-011 | P0 | Apply damage, Health, and death | Apply authoritative damage effect, clamp Health, enter Dead, and cancel living abilities. | CMB-002, CMB-010 | Exact example damage updates Health; Dead transition passes STATE-03; client final-damage input is ignored. | Server automation and two-instance PIE. | TODO | CS 8, 10; DMG/STATE criteria |
| CMB-012 | P1 | Block activation and sector | Implement 3-tick startup, sustained Block state, 120-degree server sector, and 0.30 GuardModifier, excluding exhaustion behavior. | CMB-002, CMB-010 | DEF-01/02 and blocked damage examples pass when Stamina is sufficient. | Directional boundary and damage automation tests. | TODO | CS 10 Block; DMG-02; DEF-01/02 |
| CMB-013 | P1 | Block Stamina and GuardBreak | Implement upkeep, block-hit cost, insufficient-resource outcome, and 36-tick GuardBreak. | CMB-012, CMB-016, TD-06, TD-07 | STA-03/06 and DEF-07 pass with approved rounding/outcome. | Tick-rate resource tests and server functional test. | BLOCKED | CS 9, 10; TD-06/07 |
| CMB-014 | P1 | Parry | Implement cost, timing, cooldown, server success, zero damage, refund, and attacker Stagger. | CMB-010, CMB-016, TD-08, TD-10 | DEF-03/04 and STA-04 pass with approved direction/rounding. | Tick-boundary, damage, refund, and control tests. | BLOCKED | CS 10 Parry; TD-08/10 |
| CMB-015 | P1 | Dodge | Implement cost, cooldown, invulnerability ticks, and predicted movement within 400 cm. | FND-006, CMB-002, CMB-016, TD-13 | DEF-05, MOV-04, STA-05 pass on server and owning client correction. | Ability timing tests and two-instance PIE. | BLOCKED | CS 10 Dodge; TD-13 |
| CMB-016 | P1 | Stamina regeneration | Implement 45-tick delay and precise 18/sec regeneration; expose reusable server-authoritative spending hooks. | GAS-004, CMB-001 | STA-01/02 pass; fractional accumulation is preserved; server final value replicates. | Deterministic tick/resource tests and PIE replication. | TODO | CS 9; STA-01/02 |
| CMB-017 | P1 | Crowd control and diminishing returns | Implement approved control effects, rolling window, DR, and 42-tick cap after decisions resolve. | CMB-002, CMB-001, TD-08, TD-09, TD-18 | Approved control types have deterministic integer durations; cap and DR pass boundary tests. | Rolling-window automation tests and server functional test. | BLOCKED | CS 11; TD-08/09/18 |
| CMB-018 | P1 | Combat acceptance suite | Consolidate attack, hit, damage, defense, resource, state, and control criteria implemented in Stage 3. | CMB-004 through CMB-017 | Every executable CS Stage 3 criterion passes; unresolved criteria remain visibly blocked, never skipped as success. | `Scripts/Test.ps1 -TestFilter VELRATH.Combat`. | TODO | CS 18; ARCH 12 |

## Stage 4 - Resonance

| Task ID | Priority | Title | Goal and Scope | Dependencies | Acceptance Criteria | Verification Method | Status | References |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| RES-001 | P1 | Replicated Echo actor shell | Add minimal replicated actor identity, transform, owner reference, and lifecycle state; no effects. Team integration remains a narrow query consumed later. | GAS-010 | Server-only spawn; remote clients observe actor identity/state; actor has no ASC. | Two-instance spawn/replication test. | TODO | ARCH 6; CS 12 Replication |
| RES-002 | P1 | Echo lifecycle | Implement 18-tick Cast handoff, 24-tick Forming, 180-tick Active, expiration/destruction, and 720-tick cooldown anchors. | CMB-001, RES-001, TD-17 | ECHO-02/03 pass without per-frame timer replication. | Lifecycle boundary automation and PIE observation. | BLOCKED | CS 12; ECHO-02/03; TD-17 |
| RES-003 | P1 | Echo placement ability | Implement GAS activation, target data, predicted preview contract, and server placement validation. | FND-005, RES-001, TD-12, TD-14 | Server enforces 400 cm and approved placement rules; invalid client request spawns nothing. | Server placement matrix and two-instance PIE. | BLOCKED | CS 12 Placement; TD-12/14 |
| RES-004 | P1 | Echo ownership and active limits | Track one Echo per player and maximum six in 3v3; clean references on destruction. | RES-002, RES-003 | ECHO-01 passes under simultaneous requests and destruction. | Server concurrency automation test. | TODO | CS 12; ECHO-01 |
| RES-005 | P1 | Rift Field | Apply server GAS 25% non-stacking enemy Slow within 220 cm; allies unaffected; zero damage. Consume a narrow team-relation query that Stage 6 connects to PlayerState teams. | RES-002, GAS-005 | ECHO-05 passes with test doubles for ally/enemy, overlap, leave/re-enter, and duplicate field. | Server functional tests with replicated attributes/tags; production team integration is rechecked by PVP-002. | TODO | CS 12 Rift Field; ECHO-05 |
| RES-006 | P1 | Disrupt interaction | Implement 30-tick interaction within 150 cm and interruption by authoritative damage/control. | RES-002, RES-005, TD-15 | ECHO-06 passes for completion, distance failure, damage interruption, and control interruption. | Tick-boundary server functional tests. | BLOCKED | CS 12 Disrupt; TD-15 |
| RES-007 | P1 | Owner death and Echo cooldown | Remove Echo 12 ticks after owner death and preserve/reset cooldown according to the approved respawn decision. | CMB-011, RES-002, TD-16 | ECHO-04 passes; no orphan Echo/reference; cooldown behavior matches decision after new Character. | Death/respawn multi-instance test. | BLOCKED | CS 12; ECHO-04; TD-16 |
| RES-008 | P2 | Echo visual synchronization | Drive Formation/Active/Disrupt/Destroy presentation through replicated state and Gameplay Cues. | RES-002, RES-006, TD-17 | Remote presentation follows authoritative lifecycle; cosmetics cannot alter gameplay. | Multi-instance visual checklist plus cue event logs. | BLOCKED | ARCH 6; CS 12 Replication; TD-17 |
| RES-009 | P1 | Resonance regression suite | Cover placement, lifecycle, limits, Rift Field, Disrupt, owner death, and replication. | RES-004 through RES-008 | All executable ECHO criteria pass in automation and multi-instance checks. | `Scripts/Test.ps1 -TestFilter VELRATH.Resonance` plus PIE. | TODO | CS ECHO-01 through ECHO-07 |

## Stage 5 - Multiplayer

| Task ID | Priority | Title | Goal and Scope | Dependencies | Acceptance Criteria | Verification Method | Status | References |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| NET-TEST-001 | P0 | Automated Multi-Instance GAS Replication Verification | Run a listen-server PIE session with two independent clients and verify Character possession, owner/remote ASC state, Health/MaxHealth delivery, client non-authority, repeated ActorInfo initialization, and pawn replacement. | GAS-003 | Server, owning client, and remote client are distinct replicated worlds; every required scenario reports PASS without simulated delivery or manual OnRep calls. | `Scripts/Test.ps1 -TestFilter VELRATH.Network.GAS` plus captured Automation report/logs. | DONE | ARCH 4, 7, 12; CS NET-01; CS TEST-01/02/03/04 |
| NET-001 | P0 | Network 1v1 Harness | Define a repeatable two-player PIE/listen-server setup and evidence checklist; no Dedicated Server. | FND-006, GAS-010, CMB-011 | Two players possess separate Characters and can move/observe each other; net mode and instance count are recorded. | Multi-instance PIE runbook and captured logs. | TODO | ARCH 7, 12; CS 15 |
| NET-002 | P0 | GAS replication verification | Verify Actor Info, startup abilities, attributes, effects, and tags across server/owner/remote. | NET-001, GAS-010 | NET-01 passes for GAS foundation; no duplicate grants or stale old-pawn state. | Automated/functional multi-instance test. | TODO | ARCH 4, 7; CS NET-01 |
| NET-003 | P0 | Combat replication verification | Verify Light/Heavy state, hits, damage, death, and corrections across 1v1 instances. | NET-002, CMB-018 | Server/owner/remote converge; cosmetic prediction cannot change result. | Two-instance combat matrix. | TODO | CS 15; NET-01/02 |
| NET-004 | P1 | GAS prediction windows | Enable and verify prediction only for approved responsive abilities and tick windows. | NET-003, TD-01, TD-02 | Rejected activations/cancels correct without stale tags/effects; NET-03 passes. | Artificial latency PIE tests and prediction logs. | BLOCKED | ARCH 7; CS 15; NET-03; TD-01/02 |
| NET-005 | P0 | Server-side hit validation security | Prove fabricated/out-of-range/duplicate client hit intent cannot mutate combat state. | NET-001, CMB-008, CMB-010 | HIT-04 and NET-02 pass for range, angle, duplicate, and final-damage tampering. | Adversarial functional automation. | TODO | CS 7, 15; HIT-04; NET-02 |
| NET-006 | P1 | Latency test profiles | Define and run recorded latency, jitter, and loss profiles against 1v1 combat and Echo when available. | NET-004, RES-009 | NET-04 has named profiles, reproducible results, and observed correction issues. | PIE network emulation with result table. | TODO | CS 15; NET-04 |
| NET-007 | P2 | Lag-compensation decision spike | Measure present-time validation and decide whether narrow rewind design is justified; do not implement custom protocol automatically. | NET-006, TD-20 | Written evidence accepts no-rewind or scopes a separate approved design task. | Trace/log analysis under target profiles. | BLOCKED | ARCH 7 Lag compensation; CS 15; TD-20 |
| NET-008 | P1 | Multiplayer regression suite | Automate stable possession, ASC, combat, and Echo cases supported by completed stages. Respawn coverage joins this suite when PVP-004 is completed. | NET-002 through NET-007, RES-009 | No completed multiplayer criterion regresses; zero-test runs fail. | Focused automation plus documented multi-instance suite. | TODO | ARCH 12; CS NET/TEST criteria |

## Stage 6 - Competitive PvP

| Task ID | Priority | Title | Goal and Scope | Dependencies | Acceptance Criteria | Verification Method | Status | References |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| PVP-001 | P1 | PlayerState match data | Add replicated TeamId, Kills, Deaths, and Score with server-only mutation; keep ASC on Character. | GAS-010 | Values persist across pawn replacement and replicate to all required clients. | PlayerState automation and two-instance PIE. | TODO | ARCH 3 PlayerState; AD-003 |
| PVP-002 | P1 | Team assignment and queries | Implement server team assignment and canonical ally/enemy queries used by combat/Echo; connect the contract already consumed by Rift Field. | PVP-001, RES-005 | Every player has one valid team; clients cannot change it; ally/enemy checks are deterministic; production Rift Field filtering passes. | Server tests and multi-instance PIE. | TODO | VISION 8, 10; ARCH 3 |
| PVP-003 | P1 | Replicated GameState shell | Add approved match-phase/clock/team-score containers without defining Convergence rules. | PVP-001 | Server mutates; clients observe; no client dependency on GameMode. | GameState replication test. | TODO | ARCH 3 GameState |
| PVP-004 | P1 | Death and respawn flow | Replace dead Character, preserve PlayerState, initialize new ASC once, and represent Respawn per decision. | CMB-011, GAS-009, PVP-001, TD-16, TD-21 | STATE-04 passes; death count persists; no stale ASC/Echo; spawn remains server-selected. | Multi-instance death/respawn test. | BLOCKED | ARCH 3/4; CS 5; TD-16/21 |
| PVP-005 | P1 | Archetype data schema | Define configurable Vanguard/Reaver/Weaver startup data sharing approved HP, Stamina, movement, and common abilities. | GAS-007, PVP-002, TD-22 | All archetypes use 1000 HP/100 Stamina and common foundation; no unique ability behavior invented. | Data validation automation. | BLOCKED | CS 13; ARCH 9; TD-22 |
| PVP-006 | P2 | Vanguard ability set | Implement only approved Vanguard abilities as configurable GAS content. | PVP-005, TD-19 | Approved spec criteria pass; no inferred mechanics. | Ability-specific tests and multi-instance PIE. | BLOCKED | VISION 11; CS 13; TD-19 |
| PVP-007 | P2 | Reaver ability set | Implement only approved Reaver abilities as configurable GAS content. | PVP-005, TD-19 | Approved spec criteria pass; no inferred mechanics. | Ability-specific tests and multi-instance PIE. | BLOCKED | VISION 11; CS 13; TD-19 |
| PVP-008 | P2 | Weaver ability set | Implement only approved Weaver abilities as configurable GAS content. | PVP-005, TD-19 | Approved spec criteria pass; no inferred mechanics. | Ability-specific tests and multi-instance PIE. | BLOCKED | VISION 11; CS 13; TD-19 |
| PVP-009 | P1 | Convergence rules implementation | Implement objective, phases, victory, tie resolution, and timing only after product approval. | PVP-003 | Approved Convergence state machine has deterministic server transitions and replicated client view. | Match-flow automation and multi-instance PIE. | BLOCKED | VISION 8, 13; ARCH 3, 15 |
| PVP-010 | P1 | Match scoring | Connect approved kills/objective outcomes to server PlayerState/GameState score. | PVP-002, PVP-009 | Score changes exactly once per approved event and cannot be client-mutated. | Server scoring tests and three-instance observation. | BLOCKED | ARCH 3; VISION 8 |
| PVP-011 | P1 | 3v3 match verification | Run six-player listen-server match flow with teams, respawn, score, combat, and Echo limits. | PVP-004, PVP-006, PVP-007, PVP-008, PVP-010, NET-008 | Six players form two teams; max six Echoes; match resolves by approved rules; all clients converge. | Six-instance PIE/listen-server acceptance run. | TODO | VISION 1, 8; CS ECHO-01; ARCH 7 |

## Stage 7 - Quality

| Task ID | Priority | Title | Goal and Scope | Dependencies | Acceptance Criteria | Verification Method | Status | References |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| QLT-001 | P1 | Combat debugging and telemetry | Add scoped server decision logs/trace/debug draw from CS without shipping exposure. | CMB-018, RES-009 | Attack IDs, ticks, rejection, damage, resources, control, and Echo lifecycle are reproducible from diagnostics. | Log schema tests and Shipping-disable inspection. | TODO | CS 17 |
| QLT-002 | P2 | Combat HUD | Present replicated/predicted HP, Stamina, cooldown, state, and approved match data without authority. | GAS-010, PVP-003 | HUD reads GAS/GameState; never mutates combat; owner values reconcile after correction. | PIE UI checklist and Blueprint compile. | TODO | ARCH 3 UI, 9; CS 9 |
| QLT-003 | P1 | Animation montage integration | Author/connect Light, Heavy, Block, Parry, Dodge, control, and Echo presentation to authoritative tick windows. | CMB-018, RES-009, TD-01, TD-13 | Montages/notifies match approved phases and cannot authorize gameplay independently. | Editor montage review, tick-boundary logs, PIE. | BLOCKED | CS 14; TD-01/13 |
| QLT-004 | P2 | Gameplay Cues and readability | Add approved non-authoritative cues for impacts, defense, control, and Echo lifecycle. | QLT-003, RES-008 | Owner/remote see correct cue once; cues do not alter state; readability checklist passes. | Multi-instance visual/audio checklist. | TODO | ARCH 4/6; VISION 5/9 |
| QLT-005 | P1 | Full automated acceptance suite | Map every executable CS acceptance ID to an automated or explicitly manual test with no silent gaps. | CMB-018, RES-009, NET-008, PVP-011 | Each criterion is PASS/FAIL; no matching-test and invalid-report paths fail. | `Scripts/Test.ps1` suite matrix and coverage report. | TODO | CS 18; ARCH 12 |
| QLT-006 | P2 | Performance profiling | Profile CPU, GPU, network, GAS, Character Movement, Echo count, and six-player scenario on approved target hardware. | PVP-011, QLT-004 | Stable 60 FPS target is measured with named hardware/content; bottlenecks are recorded. | Unreal Insights, stat captures, network profiler. | BLOCKED | VISION 1/9/13; ARCH 12 |
| QLT-007 | P1 | Multiplayer regression gate | Run required 1v1 and 3v3 regression under baseline and adverse network profiles before release candidates. | QLT-005, QLT-006 | No completed NET/ECHO/STATE criterion regresses; corrections and disconnects leave no stale state. | Automated suite plus documented multi-instance runs. | TODO | CS 15/18; ARCH 12 |
| QLT-008 | P1 | Combat System 1.0 vertical slice acceptance | Verify approved movement, GAS, combat, Resonance, archetypes, Convergence, UI, animation, performance, and multiplayer as one release gate. | QLT-001 through QLT-007 | All non-blocked CS criteria pass; every remaining blocked item is explicitly excluded by approval, not hidden. | Signed acceptance matrix with build/test/profile evidence. | TODO | VISION 11; ARCH 12; CS 18 |

## Technical Decisions Required

Recommendations below are proposals only. They do not become approved merely by appearing in this backlog.

| Decision ID | Open question | Blocking? / Tasks | Decision required before | Recommended option (not approved) | Status |
| --- | --- | --- | --- | --- | --- |
| TD-01 | How is the 60 Hz combat simulation scheduled and synchronized with world time, movement, physics, and montages? | Yes: CMB-001/003, NET-004, QLT-003. | CMB-001 | Small server-authoritative logical combat clock driven by a world-tick accumulator with bounded catch-up; do not force all Unreal physics into a custom fixed loop. | OPEN |
| TD-02 | What is the authoritative tick epoch and how do predicted client ticks map to it? | Yes: CMB-001/003, NET-004. | CMB-001 | Monotonic server combat tick plus sampled/replicated anchors; correlate actions with GAS prediction keys and never trust a client final tick. | OPEN |
| TD-03 | How do backward, attack, and Block movement multipliers compose? | Yes: FND-006/007 and movement acceptance. | FND-006 | Multiply one directional modifier and one action modifier against the current approved base speed; confirm sprint interaction separately. | OPEN |
| TD-04 | What hurtbox topology and server sweep shape/sampling are used? | Yes: CMB-006/008 and first 1v1 duel. | CMB-006 | Start with the simplest server-queryable Character hurtbox and one reusable sweep representation; add skeletal segmentation only if tests prove necessary. | OPEN |
| TD-05 | Which stable identifiers order same-tick hits? | Yes: CMB-009. | CMB-009 | Use server-assigned stable actor identity plus AttackInstanceId and target identity; never rely on container iteration order. | OPEN |
| TD-06 | What happens when a blocked hit costs more Stamina than remains? | Yes: CMB-013. | CMB-013 | Design decision required; either current hit remains blocked then GuardBreaks, or it resolves unblocked. No option is assumed. | OPEN |
| TD-07 | How is fractional block-hit Stamina cost rounded? | Yes: CMB-013. | CMB-013 | Use `RoundHalfUp` consistently with damage unless playtesting/design requires another explicit rule. | OPEN |
| TD-08 | How is 4.5-tick third Stagger rounded to integer ticks? | Yes: CMB-014/017. | CMB-014 | Use `RoundHalfUp` to 5 ticks for consistency, subject to approval. | OPEN |
| TD-09 | Which control types count toward the 42/120 control-loss budget? | Yes: CMB-017. | CMB-017 | Count hard loss-of-control states; exclude Slow unless explicitly approved. Exact Flinch/Knockback treatment needs design approval. | OPEN |
| TD-10 | Does Parry require a directional sector? | Yes: CMB-014. | CMB-014 | Design decision required; do not inherit Block's 120-degree sector automatically. | OPEN |
| TD-11 | Is Block-to-Parry allowed, and with what cancel window? | Partially: CMB-002/012/014. | CMB-014 | Keep unsupported until an explicit counterplay decision is approved. | OPEN |
| TD-12 | What interrupts AbilityCast and EchoCast? | Yes: CMB-002, RES-003. | RES-003 | Define interruption through existing damage/control tags and GAS cancellation, not a parallel cast system. Exact allowed tags need design approval. | OPEN |
| TD-13 | What Dodge displacement/root-motion curve is used? | Yes: CMB-015, QLT-003. | CMB-015 | Use standard predicted Character Movement/root-motion support with an authored curve capped at 400 cm. Curve shape requires approval. | OPEN |
| TD-14 | Which Echo surfaces, slope, vertical offset, LOS, and navigation rules are valid? | Yes: RES-003. | RES-003 | Implement only CS minimum distance/collision validity first; approve every additional placement restriction explicitly. | OPEN |
| TD-15 | Who may Disrupt an Echo and what follows completion? | Yes: RES-006. | RES-006 | Opponent-only interaction and server destruction is the likely minimum, but must be design-approved. | OPEN |
| TD-16 | Does Echo cooldown survive owner death/respawn, and where is it stored? | Yes: RES-007, PVP-004. | RES-007 | First decide persistence. If it persists, store an authoritative player-lifetime expiry outside the Character ASC without moving the ASC itself. | OPEN |
| TD-17 | How are Echo lifecycle tick anchors mapped to client presentation? | Yes: RES-002/008. | RES-002 | Replicate lifecycle state plus authoritative start/end tick anchors; clients derive visuals and accept correction, with no per-frame timer replication. | OPEN |
| TD-18 | What are durations/strengths for Flinch, generic Stagger, non-Rift Slow, and Knockback? | Yes: CMB-017. | CMB-017 | Pending Design Approval; implement only values already present in CS. | OPEN |
| TD-19 | What are Vanguard/Reaver/Weaver unique abilities and values? | Yes: PVP-006/007/008. | PVP-006 | Pending Design Approval; keep shared foundation only until approved. | OPEN |
| TD-20 | What latency threshold/attack types justify rewind? | Yes only for NET-007; not for initial combat. | NET-007 | Ship initial tests with present-time server validation; consider narrow rewind only from measured failures. | OPEN |
| TD-21 | How is Respawn represented across old/new Character lifetime? | Yes: PVP-004. | PVP-004 | Let GameMode/PlayerState own lifecycle orchestration; new Character begins fresh Locomotion/ASC state. Do not require one tag to survive actor destruction. | OPEN |
| TD-22 | What runtime data/effect layout is the single source for approved values? | Yes: GAS-006/007, PVP-005. | GAS-006 | C++ schema plus human-authored Primary Data Assets/Gameplay Effects, with validation against CS and no duplicated constants. | OPEN |

### Additional product/design blockers

- **PD-01 Convergence rules:** blocks PVP-009/010/011; objective, victory, tie resolution, and scoring remain open in VISION.
- **PD-02 Camera framing:** does not block the camera foundation, but blocks final camera tuning/polish.
- **PD-03 Target hardware:** blocks final performance acceptance in QLT-006.
- **PD-04 Binary asset authoring:** FND-005 and presentation tasks require Unreal Editor/human asset changes because Codex must not directly edit `.uasset` files.

## First Five Tasks to Implement

These are the first safe, independently verifiable implementation tasks after this backlog is approved:

1. **FND-002 - Apply baseline movement parameters.** No open Combat Spec decision blocks walk/sprint/acceleration/deceleration/capsule configuration.
2. **FND-003 - Third-person camera foundation.** Final framing remains open, but a configurable camera component foundation is not blocked.
3. **GAS-002 - Canonical native gameplay tags.** No design blocker; define only tags already named by CS/ARCH.
4. **GAS-003 - UVelrathAttributeSet Health.** No formula decision is required to introduce Health/MaxHealth and invariants.
5. **GAS-004 - Add Stamina attributes.** No regeneration/spending decision is required to introduce Stamina/MaxStamina and clamps.

Technical questions blocking these five: **none**. `TD-03` blocks final overlapping movement-multiplier behavior in FND-006, not the FND-002 baseline. `TD-22` blocks default initialization/data layout in GAS-006, not the AttributeSet declarations.

## Dependency Audit

- Main task dependencies point from later work to earlier IDs/stages; no task depends on itself.
- No cyclic dependency is present in the declared graph.
- Stage 1 and early Stage 2 can progress in parallel where no dependency is listed.
- Every `BLOCKED` task names a Technical Decision, product decision, or binary-asset prerequisite.
- Stage 3 does not depend on Resonance, PvP mode rules, UI, or presentation.
- Stage 4 reuses GAS/combat authority and does not create a parallel effect system.
- Stage 5 verifies completed behavior and does not become a prerequisite for the local deterministic implementation it tests.
- Stage 6 composes completed systems; unresolved Convergence/archetype design is not silently invented.
- Stage 7 is downstream validation/polish and cannot redefine combat values.
