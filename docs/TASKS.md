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
- `UVelrathAttributeSet` owns replicated Health, MaxHealth, Stamina, and MaxStamina; GAS-006 initializes them once per authoritative Character through the validated shared startup asset and Instant Gameplay Effect.
- `AVelrathGameMode` selects `AVelrathCharacter` as the default pawn.
- Current automation covers native tags, attribute invariants/replication, client non-authority, same-pawn repossession, and pawn replacement; startup data/effects and ability grants remain unimplemented.
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

Decision gates on this path: `TD-01`, `TD-02`, and `TD-22` are approved; `TD-04` remains open. `TD-05` is required before same-tick deterministic combat is considered complete, but not before the first single-Light-Attack duel demonstration. `FND-005` requires approved Editor-authored Enhanced Input assets because Codex must not directly modify binary assets.

## Stage 1 - Foundation

| Task ID | Priority | Title | Goal and Scope | Dependencies | Acceptance Criteria | Verification Method | Status | References |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| FND-001 | P0 | Inventory existing foundation | Confirm module, targets, Character, ASC, GameMode, scripts, and smoke test without refactoring. | None | Existing types and current gaps are recorded in this backlog; no generated folders are treated as source. | Relevant-file inspection and current smoke-test history. | DONE | ARCH 1, 3, 4; CS 1 |
| FND-002 | P0 | Apply baseline movement parameters | Configure capsule, walk/sprint speed, acceleration, and deceleration from CS. Do not add movement mechanics. | FND-001 | Runtime values match `MOV-01` and `MOV-02`; Editor target builds. | Focused C++ automation/configuration test plus Editor build. | DONE | CS 4; CS MOV-01/02 |
| FND-003 | P0 | Third-person camera foundation | Add minimal spring-arm/camera ownership and elevated third-person presentation with configurable framing; no final tuning values are invented. | FND-001 | Local player has a stable third-person camera; camera code contains no combat authority. | Editor build and one-player PIE visual check. | DONE | VISION 1, 9; ARCH 3 |
| DEV-001 | P0 | Development map and visible Character | Add the persistent `L_Dev` test map, PlayerStart, minimal lighting/geometry, and a C++-derived `BP_VelrathCharacter` using available standard Unreal mannequin assets. | FND-003 | Startup/default map resolves to `L_Dev`; GameMode spawns and possesses `BP_VelrathCharacter`; mesh and camera are visible and functional in one-player PIE; map and assets load without critical errors. | Editor build, asset commandlet validation, automation suite, and rendered one-player PIE screenshot. | DONE | VISION 9; ARCH 3, 8, 12 |
| FND-004 | P0 | Enhanced Input C++ bridge | Define input-facing C++ contract and bind movement/look/action inputs without implementing abilities. GAS input-tag routing is added only after GAS-002. | FND-001 | Input callbacks are narrow, local-only intent; no direct damage/state mutation; build passes. | C++ automation where possible, Editor build, local input log check. | DONE | VISION 1; ARCH 3, 9, 11 |
| FND-005 | P0 | Enhanced Input assets and mapping | Create/assign Input Actions and Mapping Context for WASD, mouse, Light, Heavy, Block, Parry, Dodge, and Echo through Unreal Editor. Codex only wires references/schema. | FND-004 | Every approved input resolves to exactly one binding; binary assets compile in Editor; no unapproved action exists. | Human/Editor asset authoring, Blueprint/asset validation, PIE input check. | BLOCKED | VISION 1, 6; ARCH 9; CS 5 |
| FND-006 | P0 | Locomotion input behavior | Route WASD/mouse intent through Enhanced Input to Character Movement; implement backward multiplier only after its composition rule is isolated. | FND-002, FND-003, FND-005 | Character moves and looks locally; server movement replicates through Character Movement; no custom movement RPC. | Listen-server PIE with two instances and movement correction observation. | DONE | CS 4; CS MOV-03; ARCH 7 |
| FND-007 | P1 | Foundation movement tests | Add deterministic checks for configured movement/capsule values and a repeatable multi-instance movement checklist. | FND-006 | MOV-01/02 pass; MOV-03/04 are either executable and pass or explicitly remain blocked by their decision/task. | `Scripts/Test.ps1` focused filter plus two-instance PIE checklist. | IN PROGRESS | CS 18 Movement; ARCH 12 |

FND-007 audit (2026-11-21): MOV-01 and MOV-02 are executable and covered by deterministic defaults plus measured listen-server movement. TD-03 is APPROVED, but MOV-03 remains pending implementation of the future predicted/authoritative Attack and Block Combat States in CMB-002/CMB-004/CMB-005/CMB-012; storing the three multipliers and documenting their composition is not acceptance and does not make MOV-03 DONE. MOV-04 remains blocked by CMB-015 and TD-13 because no Dodge movement exists yet. The existing CQTest scenario is the repeatable multi-instance checklist for camera-relative input, measured Walk/Sprint speed, server authority, simulated-proxy replication, correction convergence, repossession, and pawn replacement. FND-007 therefore remains IN PROGRESS.

## Stage 2 - GAS

| Task ID | Priority | Title | Goal and Scope | Dependencies | Acceptance Criteria | Verification Method | Status | References |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| GAS-001 | P0 | Character-owned ASC baseline | Preserve replicated Character-owned ASC, `Mixed` mode, and server/owning-client Actor Info initialization. | FND-001 | Existing code builds and smoke test loads module; no ability grant is coupled to Actor Info initialization. | Editor build and source inspection. | DONE | ARCH AD-002; ARCH 4; CS 1 |
| GAS-002 | P0 | Canonical native gameplay tags | Define central native tags for input, combat states, control, life state, ability identity, events, and Echo lifecycle used by current specs only. | FND-001 | Every tag has one definition and documented meaning; no string-literal tag usage in feature code; build passes. | Native tag automation test and Editor build. | DONE | ARCH 4, 9; CS 5 |
| GAS-003 | P0 | UVelrathAttributeSet Health | Replace placeholder base set with project AttributeSet containing Health and MaxHealth with accessors/invariants; do not implement damage yet. | GAS-001 | Health/MaxHealth exist, clamp correctly, default maximum can represent 1000, and Character owns the set. | Attribute automation tests and Editor build. | DONE | ARCH 4; CS 8, 9; CS STA/DMG criteria |
| GAS-004 | P0 | Add Stamina attributes | Add Stamina and MaxStamina to the project AttributeSet without regeneration/upkeep logic. | GAS-003 | Stamina/MaxStamina exist, clamp correctly, and can represent maximum 100. | Attribute automation tests and Editor build. | DONE | CS 9; CS STA-01 |
| GAS-005 | P0 | Attribute replication | Replicate approved attributes with `GetLifetimeReplicatedProps`, RepNotify, and GAS notification macros. | GAS-003, GAS-004 | Server, owner, and remote client converge on Health/MaxHealth/Stamina/MaxStamina in multi-instance PIE. | Focused automation plus two-instance PIE value comparison. | DONE | ARCH 4, 7, 12; CS 15; NET-01 |
| GAS-006 | P0 | Default attribute initialization | Create the approved shared startup-data reference and one Instant Gameplay Effect whose four fixed `Override` modifiers are the sole runtime source for initial Health/MaxHealth/Stamina/MaxStamina. | GAS-005, TD-22 | Effect applies once on server after Actor Info init, maxima before currents, replicates to owner/remote, survives same-Character repossession without reset, and initializes each replacement Character once; invalid content fails closed without fallback constants. | Data/effect validation, spawn/repossession/respawn automation, Editor build, and multi-instance PIE. | DONE | ARCH 4, 9, AD-013; CS ATTR-01..06 |
| GAS-007 | P0 | Startup ability-set schema | Extend the shared startup-data schema with configurable foundational ability references and retained grant handles; no archetype-specific behavior or duplicated attribute magnitudes. | GAS-002, GAS-006, TD-22 | Schema references the one initialization effect and shared ability classes, preserves granted handles, and contains no second balance source. | Data validation automation and Editor build. | DONE | ARCH 4, 9, AD-013; CS 13; CS ATTR-01 |
| GAS-008 | P0 | Server-only startup grants | Grant the shared startup ability set exactly once per Character on the server, separately from Actor Info initialization. | GAS-006, GAS-007 | Duplicate possession/init callbacks cannot duplicate abilities; handles are retained; client cannot grant. | Server automation test and two-instance PIE ability-list comparison. | TODO | ARCH 4 Startup grants; CS STATE-04 |
| GAS-009 | P0 | Possession and respawn ASC initialization | Verify server `PossessedBy`, owning-client init, pawn replacement, and cleanup without moving ASC to PlayerState. | GAS-008 | New Character gets fresh ASC state and one grant sequence; old state is retired; remote view has no stale effects. | Multi-instance PIE possession/respawn test. | TODO | ARCH AD-002/003; CS 5, 15; STATE-04 |
| GAS-010 | P0 | GAS foundation regression suite | Consolidate tags, attributes, TD-22 content validation/initialization, replication, possession/respawn, and duplicate-grant tests under a stable filter. | GAS-002 through GAS-009 | `VELRATH.GAS` executes at least one test with zero failures/notRun and covers invalid content, server-only application, same-pawn repossession, replacement pawn, duplicate callbacks, and server/owner/remote convergence where relevant. | `Scripts/Test.ps1 -TestFilter VELRATH.GAS`. | TODO | ARCH 12; CS ATTR-01..06; CS TEST-01/02/03 |

GAS-007 completion evidence (2026-11-21): the existing shared startup Primary Data Asset schema now accepts validated, unique concrete Gameplay Ability classes while retaining the sole TD-22 initialization effect reference and no attribute magnitudes. Server startup orchestration grants the configured set only after Actor Info initialization, retains handles scoped to the issuing Ability Set and ASC, prevents duplicate requests/repossessions, and removes only its owned grants. The production asset intentionally remains empty until real foundational abilities are approved; transient no-op abilities provide coverage without shipping placeholder combat behavior. Editor build, data/content validation, local lifecycle automation, listen-server CQTest with two independent clients, and the full 13-test `VELRATH` suite pass. GAS-008, GAS-009, and GAS-010 remain separate roadmap tasks.

## Stage 3 - Combat

| Task ID | Priority | Title | Goal and Scope | Dependencies | Acceptance Criteria | Verification Method | Status | References |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| CMB-001 | P0 | 60 Hz combat clock | Implement the approved server-world logical clock, bounded accumulator, epoch, and sparse GameState anchor without changing Unreal physics or Character Movement protocols. | GAS-010, TD-01, TD-02 | TIME-01 through TIME-06 pass; tick indices remain contiguous; overrun state, approved epoch/anchor mapping, and stable deterministic test hooks are exposed; no per-tick replication exists. | Deterministic clock/estimate tests under variable/low frame delta, long runs, hitches, and delayed/stale anchors plus Editor build. | BLOCKED | TD-01/02 approved; still blocked by GAS-010; CS 3 and TIME criteria |
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
| PVP-003 | P1 | Replicated match state | Extend the GameState shell introduced by CMB-001 with approved match-phase/match-clock/team-score containers without defining Convergence rules or changing the combat clock contract. | PVP-001, CMB-001 | Server mutates match state; clients observe; no client dependency on GameMode; TD-02 clock anchors remain independent. | GameState replication test. | TODO | ARCH 3 GameState |
| PVP-004 | P1 | Death and respawn flow | Replace dead Character, preserve PlayerState, initialize new ASC once, and represent Respawn per decision. | CMB-011, GAS-009, PVP-001, TD-16, TD-21 | STATE-04 passes; death count persists; no stale ASC/Echo; spawn remains server-selected. | Multi-instance death/respawn test. | BLOCKED | ARCH 3/4; CS 5; TD-16/21 |
| PVP-005 | P1 | Archetype data schema | Define configurable Vanguard/Reaver/Weaver startup data sharing the approved initial-attribute effect, movement, and common abilities. | GAS-007, PVP-002, TD-22 | All archetypes reference the shared `1000/1000` Health and `100/100` Stamina effect and common foundation; no magnitude duplication or unique ability behavior is invented. | Data validation automation. | TODO | CS 13; ARCH 9; TD-22 |
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

## Technical Decisions

Recommendations for decisions with `OPEN` status are proposals only. A row becomes authoritative only when its status is explicitly changed to `APPROVED` and the approved rule is reflected in the owning specification.

| Decision ID | Question | Blocking? / Tasks | Decision required before | Decision / recommended option | Status |
| --- | --- | --- | --- | --- | --- |
| TD-01 | How is the 60 Hz combat simulation scheduled and synchronized with world time, movement, physics, and montages? | TD-01 blocker resolved for CMB-001/003, NET-004, and QLT-003; TD-02 is also now approved. | CMB-001 scheduling design (satisfied) | Hybrid model: one server-world logical clock accumulates gameplay `DeltaSeconds`, emits fixed `1/60` steps with an eight-step per-World-Tick catch-up limit, retains backlog without skipping, and leaves GAS, Character Movement, physics, and montages in their established roles. | APPROVED |
| TD-02 | What is the authoritative tick epoch and how do predicted client ticks map to it? | TD-02 blocker resolved for CMB-001/003 and NET-004; their implementation dependencies remain. | CMB-001 network-time design (satisfied) | World-lifetime monotonic server tick plus `10 Hz` GameState clock anchors, authoritative per-action state anchors, a capped client presentation estimate, GAS prediction-key correlation for the owner, and no trusted client tick. | APPROVED |
| TD-03 | How do backward, Attack, and Block movement multipliers compose, and may Sprint apply during Attack or Block? | TD-03 blocker resolved for FND-007, CMB-002/004/005/012, and MOV-03; implementation still awaits Combat States. | MOV-03 implementation (satisfied) | `FinalSpeed = ServerClamp(BaseSpeed × DirectionalMultiplier × CombatMultiplier)`. Normal uses Walk `520` or Sprint `660` when held and allowed. Attack/Block prohibit Sprint and force Walk `520`. Direction is forward/sideways `1.0` or backward `0.85`; combat is Normal `1.0`, Attack `0.45`, or Block `0.60`. Held Sprint resumes on exit when allowed. | APPROVED |
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
| TD-17 | Which Echo lifecycle states and transition anchors must its replicated snapshot expose through TD-02? | Yes: RES-002/008. | RES-002 | Use the TD-02 clock mapping with an Echo-owned lifecycle instance/revision, current state, state-change tick, and terminal tick; exact Echo transition schema remains to be approved. | OPEN |
| TD-18 | What are durations/strengths for Flinch, generic Stagger, non-Rift Slow, and Knockback? | Yes: CMB-017. | CMB-017 | Pending Design Approval; implement only values already present in CS. | OPEN |
| TD-19 | What are Vanguard/Reaver/Weaver unique abilities and values? | Yes: PVP-006/007/008. | PVP-006 | Pending Design Approval; keep shared foundation only until approved. | OPEN |
| TD-20 | What latency threshold/attack types justify rewind? | Yes only for NET-007; not for initial combat. | NET-007 | Ship initial tests with present-time server validation; consider narrow rewind only from measured failures. | OPEN |
| TD-21 | How is Respawn represented across old/new Character lifetime? | Yes: PVP-004. | PVP-004 | Let GameMode/PlayerState own lifecycle orchestration; new Character begins fresh Locomotion/ASC state. Do not require one tag to survive actor destruction. | OPEN |
| TD-22 | What runtime data/effect layout is the single source for approved values? | TD-22 blocker resolved for GAS-006/007 and PVP-005; implementation dependencies remain. | GAS-006 data/effect implementation (satisfied) | One shared startup Primary Data Asset references one Instant initialization Gameplay Effect; the effect's four fixed `Override` magnitudes are the sole runtime balance source. Server applies it once per new Character after Actor Info init; clients only receive replication. | APPROVED |

### TD-01 approved decision and implementation note

#### Architecture comparison

| Criterion | A. Fixed-step clock used as a broad combat loop | B. GAS/Unreal timers using seconds | C. Hybrid discrete windows plus GAS/Character Movement |
| --- | --- | --- | --- |
| Window accuracy | Exact integer boundaries if every relevant system is moved into the loop. | Seconds can represent authored values, but callbacks execute on World Ticks and may be late/coalesced. | Exact integer combat boundaries without moving unrelated systems into the loop. |
| FPS below 60 | Multiple fixed steps can catch up, but coupling movement/physics greatly increases cost. | Timers fire only when the world services them; phase ordering becomes frame-dependent. | Zero or multiple lightweight combat steps run per World Tick; movement/physics keep standard behavior. |
| Frame hitch | Can catch up exactly but risks a large spiral when the loop also simulates spatial systems. | Late callbacks cannot reconstruct all missed discrete boundaries reliably. | Eight-step bounded catch-up preserves order and retains remaining backlog without a single unbounded frame. |
| Server load | Highest if all actors, traces, movement, or physics are fixed-stepped. | Low, but correctness becomes distributed across independent timers/tasks. | Low for 3v3: one clock plus only active combat consumers; trace count remains feature-owned. |
| GAS interaction | Risks duplicating GAS lifecycle and cancellation. | Natural for durations/cooldowns, weak as the sole exact window scheduler. | GAS remains lifecycle authority and reads integer window state from the focused clock. |
| Network prediction | Requires a custom shared simulation/rollback model to gain its full benefit. | Uses GAS prediction but has no canonical discrete server timeline. | Uses GAS prediction keys and Character Movement correction; sparse server tick anchors are handled by TD-02. |
| Server validation | Strong only after all command ordering/spatial simulation is integrated. | Server remains authoritative, but exact boundary evidence is harder to reproduce. | Server stamps activation and resolves all consequential windows/results against one tick sequence. |
| Testability | High for the loop, lower for Unreal-system integration. | Easy timer smoke tests, weak boundary/hitch reproducibility. | Pure clock/window tests plus focused GAS, movement, and multi-instance integration tests. |
| Implementation/maintenance | Highest; creates a second simulation architecture. | Lowest initially, but timer drift/ordering special cases spread across abilities. | Moderate and bounded; one small scheduler and integer helpers. |
| Competitive 3v3 fit | More machinery than the MVP requires. | Insufficiently explicit for narrow Parry/Dodge and same-tick diagnostics. | Best balance of exact windows, server authority, existing Unreal systems, and MVP scope. |

Option C is approved. It uses the fixed-step accumulator from option A only for discrete combat time. It does not make Unreal physics, Character Movement, GAS, animation, rendering, or replication fixed at `60 Hz`.

#### Technical contract and answers

1. The gameplay server owns the authoritative clock. Each gameplay `UWorld` has one focused clock instance; a listen-server client never overrides it.
2. A world-scoped game-thread driver runs once per Unreal World Tick with `TickInterval = 0` semantics and feeds an accumulator. An Actor tick interval or `FTimerManager` timer is not the `60 Hz` guarantee.
3. The clock stores a monotonic `uint64 CurrentTick`, initially `0` at its TD-02 epoch. For every complete `1/60` second in the accumulator it subtracts one step, increments `CurrentTick`, then processes that tick. Extraction keeps a normalized sub-step remainder and is tested against long-run floating-point drift. Action-local time is `CurrentTick - StartTick`.
4. After a hitch, the driver processes available fixed steps in order and retains unprocessed accumulated time. It never replaces several ticks with one large step or jumps directly to a later phase.
5. Yes. The initial maximum is eight simulation steps per Unreal World Tick (`133.33 ms` of logical time). This bounds one-frame work while allowing common low-FPS/hitch recovery.
6. A server that cannot drain backlog is overloaded. The clock preserves sequence correctness, exposes backlog/overrun telemetry, and combat may fall behind real time; it does not shorten windows. Match abort/degradation policy is not invented by TD-01 and requires later operational/product approval.
7. For an action accepted at server tick `S`, authored local `[a, b)` maps to absolute `[S + a, S + b)`. The predicate is `a <= CurrentTick - S && CurrentTick - S < b`.
8. The server assigns an `AttackInstanceId` and keeps a per-attack set of already resolved target identities. Repeated sweeps/catch-up steps cannot resolve the same `(AttackInstanceId, Target)` twice. Other one-shot transitions use instance ID plus boundary tick and execute only on monotonic crossing.
9. When prediction is enabled, the owning client starts the GAS ability/montage/cues under its GAS prediction key immediately. Server rejection cancels/reconciles presentation; predicted cosmetics never confirm a gameplay result. Exact predicted-tick alignment uses the approved TD-02 anchor contract.
10. GAS server activation validates the request and snapshots the current authoritative tick as `StartTick`; a client-reported final tick is not trusted. TD-01 does not add a custom movement/input RPC or rollback command queue.
11. Do not replicate every combat tick. Replicate normal GAS state/effects/tags and feature state plus stable action/lifecycle instance IDs and authoritative start/end tick anchors when clients need presentation. The approved TD-02 contract defines the sparse clock anchor, epoch, cadence, ordering/late-join behavior, and client estimate.
12. The combat clock neither drives nor re-simulates `UCharacterMovementComponent`. Movement continues through saved moves, prediction, server correction, and supported root motion. Combat tags/windows gate movement rules; the server evaluates spatial combat against the authoritative transform available at the step.
13. Combat-clock and server-window correctness do not require ability prediction in the first implementation. Add owner prediction only through the later GAS prediction task using TD-02; begin with authoritative results and separately local, non-authoritative presentation where safe.
14. Server rewind, historical hurtboxes, client timestamp validation, and latency thresholds belong to NET-007/TD-20. TD-01 includes no rollback or lag-compensation history.
15. Test the pure clock/window contract with supplied delta sequences and explicit tick advancement, not wall-clock sleeps or render FPS. Integration tests may vary World Tick cadence but assert integer ticks, boundaries, ordering, backlog, and exact-once events.

Render FPS is the rate at which a client presents images. Unreal World Tick is the variable-rate update of one game world, normally once per game frame. A Combat Simulation Step is one discrete `1/60` logical update emitted zero or more times from a World Tick. Network Replication Frequency is how often the server considers/sends replicated actor state; it is bandwidth scheduling, not the combat clock and not a guarantee of one packet per combat tick.

#### Minimal component structure

- `UVelrathCombatClockSubsystem` (planned, world-scoped): owns `CurrentTick`, double accumulator, fixed step, eight-step budget, backlog/overrun diagnostics, and deterministic test advance. It owns no ability, hit, damage, or movement rules.
- `FVelrathCombatTickWindow` (planned value type): stores integer start/end offsets and implements the half-open containment predicate without timers.
- Per-ability/Echo state (planned in its owning feature): stores only its authoritative `StartTick`, instance identity, and already-resolved event/target state needed by that feature.
- TD-02 replication bridge (planned separately): `AVelrathGameState` publishes sparse authoritative clock anchors, while feature owners publish action/lifecycle snapshots; no per-frame tick replication.

#### Small implementation tasks

| Step | Component responsibility | Likely files/classes | Required tests | Dependencies | Done when |
| --- | --- | --- | --- | --- | --- |
| 1. Clock arithmetic core | Own monotonic tick, double accumulator, normalized remainder, eight-step budget, backlog, and test-only advance; no `UWorld`, GAS, movement, or replication behavior. | `FVelrathCombatClock` in `Public/Combat/VelrathCombatClock.h` and `Private/Combat/VelrathCombatClock.cpp`; `Private/Tests/VelrathCombatClockTest.cpp`. | Equivalent delta partitions, zero delta, eight-step limit, retained backlog, contiguous indices, long-run drift, and deterministic reset/initialization. | Approved TD-01/02 and completed CMB-001 task prerequisites. | TIME-01/02/03/05/06 pass and the Editor build passes. |
| 2. World driver and diagnostics | Create exactly one authoritative gameplay-world clock driver, feed gameplay `DeltaSeconds`, enforce world/net-mode guards, and expose backlog/overrun logs or trace without adding gameplay consumers. | `UVelrathCombatClockSubsystem` in `Public/Combat/VelrathCombatClockSubsystem.h` and `Private/Combat/VelrathCombatClockSubsystem.cpp`; focused subsystem automation test. | Server/standalone world creation, non-authoritative client rejection, pause/zero delta, world teardown/reset, and overrun telemetry. | Step 1 and approved TD-02 epoch/initialization contract. | One authority instance advances per server world, no client can mutate it, TIME-05 passes, and teardown leaves no stale clock. |
| 3. Tick-window helper | Represent integer action offsets and half-open containment/crossing without timers or montage authority. | `FVelrathCombatTickWindow` in a focused `Public/Combat/VelrathCombatTickTypes.h` (add a `.cpp` only if behavior requires it); `Private/Tests/VelrathCombatTickWindowTest.cpp`. | Start/end boundary, zero-length invalid data, monotonic crossing, and every current Startup/Active/Recovery/Parry/Dodge/Echo interval from CS. | Step 1 only. | TIME-04 and the existing ATK/DEF/ECHO boundary tables pass using explicit ticks. |
| 4. TD-02 anchor and estimate | Add the approved GameState clock anchor, capped client estimate, latest-state ordering, late-join reconstruction, and owner prediction-key correlation; do not replicate every step. | Planned `AVelrathGameState`, focused anchor/estimate value types under `Public/Combat/`, feature-owned action snapshots, and network tests under `Private/Tests/`. | Delayed, duplicate, and out-of-order anchor delivery; one lost cadence update; extrapolation-horizon hold; server hitch/backlog correction; late join; client-tick tampering; no per-combat-tick replication. | Approved TD-02 and Steps 1-2. | GameState exposes the newest epoch/sequence/tick/time pair, actions expose authoritative instance/revision/start/state/end anchors, presentation corrects without changing gameplay, and the server never trusts a client final tick. |
| 5. First GAS consumer | Connect only Light Attack activation to authoritative `StartTick` and shared window queries; keep cost, tags, cancellation, and montage lifecycle in GAS. | Planned focused Light Attack Gameplay Ability under `Public/Abilities/` and `Private/Abilities/`, plus `Private/Tests/VelrathLightAttackTimingTest.cpp`; exact class name follows the existing ability naming convention established by its prerequisite task. | Tick 0 activation, `[12,19)` Active, total `[0,36)`, tick 27/29 cancels, rejection cleanup, and no timer/montage-authorized hit. | GAS-008, CMB-002, CMB-003, and Steps 1-4. | ATK-01/03/05 pass locally and the Editor build passes; no hit/damage behavior is added early. |
| 6. Network and presentation verification | Verify owner prediction/reconciliation and server/remote observation without changing authority; author/adjust binary montage content only through Unreal Editor. | Focused `Private/Tests/VelrathCombatTimingNetworkTest.cpp`, existing ability classes, and Editor-authored montage assets listed explicitly in the task report; no direct `.uasset` edits by Codex. | Listen server plus client, variable client/server FPS, latency/jitter/loss profiles, rejected activation, correction cleanup, and tick-boundary diagnostics. | NET-004, QLT-003, relevant completed abilities, and Steps 1-5. | Owner/server/remote converge, rejected prediction leaves no stale tags/effects, montage mismatch cannot change results, and the recorded multi-instance matrix passes. |

TD-01 approval selects the scheduler contract only. No Combat Clock, ability, state machine, damage, prediction, rollback, or lag compensation is implemented by this decision task. TD-02 is now approved separately; `CMB-001` remains `BLOCKED` until its preceding GAS dependency is complete.

### TD-02 approved decision and implementation note

#### ADR

**Decision.** Use option C: standard GameState server-time synchronization plus sparse global clock anchors and authoritative per-action state anchors. The client may extrapolate only for presentation and allowed GAS prediction. The server never consumes a client-estimated combat tick as authority.

**Clock lifetime.** One `UVelrathCombatClockSubsystem` belongs to one authoritative gameplay `UWorld`. It initializes exactly once at world begin play with a new nonzero `FGuid` epoch and `CurrentTick = 0` before the first emitted step. `CurrentTick` is a monotonic `uint64`; it is never derived from world time. Respawn keeps the same clock. World teardown/travel destroys it, and the next world uses a new epoch.

**Replicated owner.** The planned `AVelrathGameState` is the smallest appropriate global owner because it exists on the server and every client, already supplies synchronized server world time, and provides current state to late joiners. `PlayerController` cannot serve remote observers, Character lifetime is too short, and GameMode is server-only.

#### Architecture comparison

| Criterion | A. Replicate absolute `CurrentTick` | B. Replicate only action timestamp/tick anchors | C. GameState server time + global/action anchors |
| --- | --- | --- | --- |
| Combat-phase display | Exact only at receipt; sparse updates stutter without extrapolation, while 60 updates/sec are unnecessary. | Exact relative boundaries after an action snapshot, but no shared correction domain after hitch or for unrelated lifecycle state. | Smooth nominal extrapolation with authoritative phase/start/end correction and an explicit hitch-safe mapping. |
| Network cost | Highest if sent every tick; if made sparse it becomes a partial form of option C. | Lowest; sent only when action/lifecycle state changes. | Low and bounded: one small `10 Hz` match-wide anchor plus state changes, never 60 RPCs/sec. |
| Latency and jitter | Presentation trails the last packet or needs an unspecified local extrapolator. | Start delivery latency can shift presentation unless paired with synchronized server time. | Uses Unreal's synchronized server-time estimate; owner may predict under GAS and then rebase. |
| Packet loss | A lost update delays or jumps the display; periodic resend is still required. | Reliable latest state recovers active actions, but a missing start-only event would be fatal. | Replicated latest-state properties plus periodic anchors recover without replaying an event stream. |
| Mid-match join | Works if the current tick is a replicated property. | Works only if every active feature exposes a current snapshot rather than transient RPCs. | GameState supplies current clock mapping and each relevant actor supplies its current action/lifecycle snapshot. |
| GAS compatibility | Does not by itself correlate a predicted activation with its accepted server instance. | Natural for ability instances and prediction keys, but incomplete for a shared clock correction. | Keeps GAS prediction/rejection and adds only accepted-instance/start-tick correlation. |
| Implementation complexity | Simple sender, but smooth/correct client behavior and bandwidth policy remain unresolved. | Simple per feature, but duplicated time-estimation logic and weak global diagnostics. | Moderate, focused implementation: one global anchor/estimator and small feature-owned snapshots. |
| Testability | Easy tick delivery tests; hard to prove useful behavior without defining interpolation. | Good feature tests; limited global hitch/late-join coverage. | Pure estimator tests plus deterministic delayed/lost/out-of-order and multi-instance scenarios. |

Option C is the minimum complete architecture for 3v3. Option B remains part of C for action state. Option A is rejected as per-tick replication; the `CurrentTick` sample inside the sparse global anchor is not a continuously replicated clock.

#### Network contract and minimal replicated data

| State | Owner / delivery | Required fields | Rule |
| --- | --- | --- | --- |
| Global clock anchor | `AVelrathGameState`, replicated latest-state property | `ClockEpoch: FGuid`, `AnchorSequence: uint32`, `ServerTick: uint64`, `SampledServerWorldTimeSeconds: double` | Refresh at `10 Hz`; request an immediate replication update on epoch creation and overrun begin/end. A new GameState/epoch resets client caches; within an epoch, ignore a non-newer wrap-aware sequence. |
| Action snapshot | Character or feature Actor that owns the state, replicated latest-state property | `ActionTag`, `ActionInstanceId: uint32`, `Revision: uint32`, `StartTick: uint64`, `PhaseOrState`, `StateChangeTick: uint64`, optional `EndTick` and end/cancel reason | Actor network identity + epoch + instance identifies one activation. A newer instance supersedes an older one; a newer revision supersedes older state for the same instance. |
| Owner prediction correlation | Existing GAS activation path; add a narrow owner-only acknowledgement only if GAS activation info cannot expose the mapping | Original GAS `FPredictionKey`, accepted `ActionInstanceId`, authoritative `StartTick` | Never sent as a client-selected authoritative tick. Remote clients do not need a prediction key. |
| Cosmetic event identity | Feature state/cue payload when a one-shot presentation event is required | Actor identity, epoch, action instance, event kind, authoritative event tick | Client deduplication only; server gameplay exact-once state remains feature-owned. |

`EndTick` uses an explicit optional/invalid representation while active; tick `0` is valid and therefore cannot be the sentinel. `ActionInstanceId` is monotonically allocated by the server per owning Actor and may restart on a new pawn because replicated Actor identity and clock epoch remain part of the key. This does not pre-approve TD-05's cross-actor same-tick ordering rule.

The normal anchor period is `0.1 s`, matching the UE 5.7 `AGameStateBase` default server-world-time update frequency. Action start, phase transition, cancel/interruption, completion, and replacement mark their latest-state property dirty/request timely actor replication; they do not wait for the next global anchor and do not create a combat-tick RPC stream.

#### Client estimate and correction

For the newest valid anchor, the raw presentation estimate is:

```text
Elapsed = max(0, GameState.GetServerWorldTimeSeconds() - Anchor.SampledServerWorldTimeSeconds)
EstimatedTick = Anchor.ServerTick + floor(Elapsed * 60)
EstimatedTick = min(EstimatedTick, Anchor.ServerTick + 30)
```

The 30-tick cap limits unconfirmed forward prediction to `500 ms`. If a newer anchor has not arrived by then, presentation holds at the cap or uses the last authoritative phase. There can be no finite absolute accuracy guarantee during an arbitrarily long network outage; the contract bounds speculation instead of hiding that fact.

Every newer global anchor rebases the estimate. A server hitch can produce an anchor whose tick is behind the value predicted from elapsed world time, and catch-up can later advance faster than real time. Inside the same authoritative phase, montage rate/position may converge smoothly. A different authoritative phase, a cancellation/end, a GAS rejection, a replacement instance, or a new epoch applies immediately. A correction may move presentation backward, but it never rewinds server gameplay or re-emits a deduplicated one-shot event.

#### Server / owner / remote flow

```text
Server UWorld clock       AVelrathGameState        Owning client                 Remote client
       |                         |                       |                             |
       |-- sample tick + time -->|-- replicated anchor->|-- presentation estimate -->|
       |                         |------------------------------ replicated anchor -->|
input/GAS prediction                                     |
       |<---- standard GAS activation + prediction key --|
       |-- validate; assign InstanceId and StartTick ----> owner reconciliation
       |-- replicated authoritative action snapshot ---->| correct predicted phase
       |-------------------------------------------------------------- snapshot ----->| derive remote phase
       |-- phase/cancel/end revision -------------------->| correct/deduplicate
       |-------------------------------------------------------------- revision ----->| correct/deduplicate
       |        all hits, defense, damage, resources, death, and Echo results remain server-only
```

#### Required-question answers

1. **World identity:** the runtime clock is the world subsystem instance associated with one authoritative gameplay `UWorld`; the network identity is its server-created `ClockEpoch` on GameState.
2. **Server `CurrentTick`:** initialize to `0` at authoritative world begin play and increment the monotonic `uint64` once before processing each fixed step. Only the server/standalone clock mutates it.
3. **Server-time relationship:** clients use the newest replicated `(ServerTick, SampledServerWorldTimeSeconds)` pair and GameState's synchronized `GetServerWorldTimeSeconds()`. This is a sampled mapping, not an equality between clocks.
4. **Separate `CurrentTick` replication:** no per-tick property or RPC. The tick sample inside the `10 Hz` anchor and state-change anchors is sufficient for presentation and recovery.
5. **Minimum clock anchor:** epoch, sequence, authoritative tick, and sampled server world time. Backlog remains server diagnostic state; overrun edge changes force a fresh anchor instead of redefining the formula.
6. **Attack-instance distinction:** replicated Actor identity + clock epoch + server `ActionInstanceId`; the per-Actor ID increases for every accepted activation, including repeated uses of the same attack.
7. **Start/phase/ID delivery:** a feature-owned latest-state snapshot carries action tag, instance, revision, `StartTick`, current phase/state, `StateChangeTick`, and optional `EndTick`/reason. State transitions request timely replication.
8. **Phase between updates:** derive the scheduled phase from `EstimatedTick - StartTick` and authored integer windows, then clamp/override it with the latest authoritative phase and terminal state.
9. **Loss/out-of-order:** periodic GameState anchors recover clock mapping; replicated current state recovers actions. Epoch/sequence and instance/revision comparisons ignore stale or duplicate deliveries. Lifecycle correctness never depends on an unreliable multicast.
10. **Late join:** initial GameState replication provides the current anchor; relevant Characters/Echoes provide their current active/terminal snapshot. Completed history is not replayed.
11. **Duplicate events:** the server keys gameplay resolution by action instance plus target/event boundary. Clients key one-shot cosmetics by actor, epoch, instance, event kind, and authoritative event tick.
12. **Cancel/interruption/respawn:** publish a newer terminal revision with authoritative end tick/reason; a newer instance also supersedes old state. Pawn destruction clears its snapshots and GAS prediction; a new pawn has a new network identity while the world clock continues.
13. **Local-clock disagreement:** local wall time is never compared directly. GameState supplies estimated server world time, and every fresh combat anchor rebases accumulated offset/drift.
14. **Visual-error limit:** unconfirmed forward extrapolation is capped at 30 ticks; authoritative phase changes can arrive sooner. During arbitrary outage or severe catch-up, absolute visual error cannot be guaranteed, so the client holds/falls back rather than extending authority claims.
15. **Contradictory update:** authoritative epoch, instance, phase, cancellation, and end state win. Cancel/replace stale predicted presentation, seek or restart the montage/cue state as needed, and never undo or duplicate gameplay outcomes.
16. **GAS prediction keys:** GAS continues to generate, send, accept, and reject prediction keys. The server associates the accepted key with the new instance/`StartTick`; the owner uses that relation only to reconcile its predicted presentation. The key is not a clock and remote clients do not need it.
17. **Character Movement SavedMove:** no combat tick is added as movement authority. SavedMoves continue to carry movement intent/predicted movement state and reconcile through standard Character Movement; combat snapshots may affect presentation/gates but do not replace move timestamps or corrections.
18. **Future lag compensation:** historical hit validation, rewindable transforms/hurtboxes, mapping a client input time into history, latency acceptance windows, and anti-abuse policy belong to NET-007/TD-20. Clock synchronization alone intentionally solves none of them.

#### Boundary cases

- The clock does not advance for paused/zero gameplay delta, while anchors may repeat the same tick with a newer sequence/time sample.
- One or more lost periodic updates are recovered by the next latest-state update; after 30 extrapolated ticks the client stops speculating.
- A same-tick action start/end is ordered by action revision and explicit state, not packet arrival order.
- A hitch may move client presentation ahead; bounded catch-up may later move it behind. Both cases rebase without changing authored windows.
- Anchor sequence and action revision comparisons must be wrap-aware. `uint64` tick exhaustion is outside any practical match lifetime and is not assigned a wrap behavior.
- Relevancy loss/re-entry behaves like late join for that Actor: consume its current snapshot and do not replay missed transient history.
- World travel/new epoch invalidates all cached estimates, action keys, and presentation-event dedup entries from the old epoch.
- A remote client never receives owner-only prediction acknowledgement and does not need it to render authoritative state.

#### Planned CQTest verification (not implemented by TD-02)

| Scenario | Required observation |
| --- | --- |
| Listen server + owner + remote | All three identify the same epoch/action instance and owner/remote converge on authoritative phase without client authority. |
| Create and cancel action | One instance is created; cancellation publishes one newer terminal revision and clears predicted presentation. |
| Start/end anchors | Derived boundaries match authoritative `StartTick`, state-change tick, and `EndTick`, including half-open end behavior. |
| Delayed/lost anchor | Client extrapolates no more than 30 ticks, then holds/falls back; the next anchor rebases it. |
| Duplicate/out-of-order state | Older epoch/sequence/instance/revision is ignored and no cosmetic event fires twice. |
| Server hitch/backlog | Client does not assume `floor(ServerWorldTime * 60)`; overrun anchor corrects ahead/behind presentation while server ticks remain contiguous. |
| Late join / relevancy re-entry | Current GameState and actor snapshots reconstruct active phases without requiring historical RPCs. |
| Respawn | New pawn identity cannot collide with old action state; world epoch/tick remains continuous. |
| Predicted phase correction | Accepted prediction maps to authoritative instance/start; rejection or contradictory state removes stale tags/effects/presentation through GAS. |
| Authoritative result | Owner and remote presentation differences never change server hit, defense, damage, resource, Echo, or death outcomes. |

These are future CQTest scenarios for the implementation tasks. TD-02 adds no test code and makes no claim that any network scenario has already passed.

#### Dependencies and next implementation steps

| Item | Actual dependency state after TD-02 | Consequence |
| --- | --- | --- |
| TD-02 | Design is complete and independent of GAS-010 implementation. | `APPROVED`; it no longer blocks CMB-001/003 or NET-004 on design grounds. |
| GAS-010 | Depends on GAS-002 through GAS-009 and must consolidate tags, attributes, initialization, replication, and duplicate-grant tests under `VELRATH.GAS`; at least one test must run with zero failures/notRun and owner/server/remote coverage where relevant. | Its unfinished suite and prerequisites still block CMB-001 because the declared task graph explicitly requires GAS-010, not because the clock algorithm depends on GAS. |
| CMB-001 | TD-01 and TD-02 gates are satisfied; GAS-010 is not. | Remains `BLOCKED`. Do not implement the clock yet. |

There is no single implementation task that can now make CMB-001 runnable while preserving the declared dependencies. TD-22 is approved and GAS-006 is complete; the earliest unfinished chain is now GAS-007, GAS-008, GAS-009, and GAS-010. Only then does CMB-001 begin.

When that chain is complete, keep implementation slices small:

1. Implement/test pure `FVelrathCombatClock` arithmetic and reset semantics.
2. Add the authoritative world subsystem, epoch initialization, guards, and overrun telemetry.
3. Introduce `AVelrathGameState` and the global anchor latest-state property without action logic.
4. Add the pure client estimator/order/correction helper and delayed/lost/out-of-order tests.
5. Add feature-owned action snapshots and owner prediction correlation with the first GAS timing consumer, not as a parallel ability framework.
6. Run the planned listen-server CQTest matrix with server, owning client, and remote client before claiming multiplayer behavior.

TD-02 approves documentation only. It creates no Combat Clock, Combat State Machine, Gameplay Ability, AttributeSet, Character Movement change, or lag compensation.

### TD-03 approved decision and implementation note

The approved composition is `FinalSpeed = ServerClamp(BaseSpeed × DirectionalMultiplier × CombatMultiplier)`. Normal selects Walk `520 cm/s` or Sprint `660 cm/s` when Sprint intent is held and Sprint is allowed. Attack and Block always select Walk `520 cm/s`, so held Sprint intent does not affect current speed in either state. The directional factor is `0.85` only for backward movement and otherwise `1.0`; the combat factor is Attack `0.45`, Block `0.60`, or Normal `1.0`. Attack and Block are mutually exclusive canonical GAS states, so their factors never stack.

Entering Attack or Block suppresses Sprint speed without clearing the held intent. On exit, Sprint resumes if the intent is still held and the resulting state permits Sprint; otherwise Walk applies.

This composition is compatible with `UCharacterMovementComponent` prediction/correction only when the owning client and server derive the same factors for each saved move. Sprint intent already travels in `FSavedMove_Character::FLAG_Custom_0`; future predicted GAS state must expose the Attack/Block factor consistently during move replay, and directional classification must be deterministic from movement data available on both sides. The server remains final and clamps/rejects invalid combinations through the standard Character Movement correction path. No separate movement RPC or prediction system is required.

TD-03 approval resolves the design rule only. MOV-03 remains pending, not DONE, until the Combat States and their predicted/server-authoritative movement behavior are implemented and verified. FND-007 remains IN PROGRESS; MOV-04 also still awaits Dodge implementation under CMB-015 and TD-13.

### TD-22 approved decision and implementation note

#### Requirement audit

The following are binding requirements from GAS-006 through GAS-010, ARCH, and CS rather than optional design suggestions:

- one Character-owned ASC and exactly one `UVelrathAttributeSet` per Character;
- Health/MaxHealth `1000/1000` and Stamina/MaxStamina `100/100` for every current MVP archetype;
- one approved data/effect initialization path, executed once by the server after Actor Info initialization and observed by clients only through replication;
- same-Character repossession must preserve changed values, while respawn/pawn replacement creates fresh Character-lifetime GAS state and initializes it once;
- Actor Info initialization, attribute initialization, and ability grants are separate operations; startup grants are server-only, duplicate-safe, and retain their handles;
- GAS-010 must consolidate validation, initialization, replication, possession/respawn, and duplicate-grant coverage under `VELRATH.GAS` with real server/owner/remote checks where applicable.

Before this approval, use of a Primary Data Asset, placement of magnitudes in a Data Asset versus a Gameplay Effect, SetByCaller mapping, exact class/asset names, and separate Vanguard/Reaver/Weaver startup assets were proposals. CS requires all three archetypes to share the same starting attributes today; unique values remain unapproved and are not part of GAS-006/007.

#### Architecture comparison

| Criterion | A. Direct C++ assignment | B. One fixed-value Instant Gameplay Effect | C. Values in Data Asset + SetByCaller Instant effect | D. Curve/Data Table-backed Gameplay Effect |
| --- | --- | --- | --- | --- |
| MVP complexity | Lowest, already present. | Low: one effect plus one reference and a guard. | Medium: schema, four transport tags/keys, spec population, and cross-asset validation. | Medium/high: table schema, row/level policy, effect wiring, and validation. |
| GAS fit | Acceptable bootstrap path, but bypasses the normal effect path. | Native GAS effect-spec/application path. | Native GAS path, but adds a custom magnitude transport contract. | Native GAS scalable-float path, useful mainly when level/progression exists. |
| Balance tuning | Requires C++ edit/build and encourages constants. | Four magnitudes are edited together in the Gameplay Effect asset. | Values are edited together in the Data Asset. | Values are editable in a table, with extra indirection. |
| Blueprint/Editor use | Weak; Blueprint can only wrap exposed C++ configuration. | Strong; standard Gameplay Effect authoring and a class reference in startup data. | Strong, but authors must keep Data Asset fields and GE SetByCaller modifiers/tags compatible. | Strong for bulk/level curves, less transparent for four fixed values. |
| Network safety | Safe only with the same authority/once guards required by every option. | Safe with server-only spec creation/application; clients replicate attributes. | Same as B after validation. | Same as B after validation. |
| Repossession/respawn | Guard preserves same pawn; new pawn reruns initialization. | Same lifecycle, with GAS application. | Same lifecycle, with GAS application. | Same lifecycle, with GAS application. |
| Testability | Easy values/lifecycle tests, weak content validation. | Easy effect-contract, postcondition, lifecycle, and replication tests. | Good, but requires Data Asset-to-tag-to-modifier mapping tests. | Good, but also requires table row/level resolution tests. |
| Duplicate-source risk | High: constructor defaults, constants, Blueprint defaults, and tests can drift. | Low if C++/Data Asset magnitudes are removed and the startup asset stores only the effect reference. | Medium: safe only if the GE contains no fallback magnitudes; otherwise two sources exist. | Medium: table, effect coefficient/default, and level selection can each become an accidental source. |

Option B is approved. It is combined with the reference-only startup schema required by GAS-007, but that schema does not own attribute magnitudes and therefore does not turn the design into option C. C adds SetByCaller machinery without a present requirement; D is reserved for a real level/progression or bulk-table requirement.

#### Approved initialization contract

1. **Single source.** `GE_VelrathInitialAttributes` (working asset name) is the only runtime owner of the four starting magnitudes. CS remains the normative acceptance oracle, so tests may assert its approved numbers; test or documentation literals are not runtime fallback sources.
2. **Transfer into the AttributeSet.** The Instant effect contains exactly four `Override` modifiers in validated order: MaxHealth, Health, MaxStamina, Stamina. It yields `1000/1000` and `100/100`; normal AttributeSet hooks clamp invariants.
3. **Once responsibility.** `AVelrathCharacter` owns a server-only transient `bInitialAttributesInitialized` guard independent of Actor Info and ability-grant guards. The guard becomes true only after validated application and postcondition checks succeed.
4. **Creation/application.** A human authors the Gameplay Effect asset. On the server, the Character asks its ASC to create the outgoing spec from the referenced class and applies that spec to self. No new initialization component or manager is introduced.
5. **Actor Info order.** Server `PossessedBy` calls `InitAbilityActorInfo(this, this)` first, then attempts initial attributes, then later startup grants. The owning client initializes Actor Info through its established replication callback but never calls attribute initialization.
6. **Repeated possession.** Repeated `PossessedBy` on the same Character may refresh Actor Info. The true initialization guard prevents effect application, so damaged/spent or modified maxima remain unchanged.
7. **Respawn proof.** The test records the old Character, ASC, and AttributeSet identities, replaces the pawn through the authoritative GameMode path, proves all three identities are new, and observes one fresh initial tuple on server, owner, and remote.
8. **Maximum/current order.** Each maximum is applied before its current value; asset validation rejects a different/missing/duplicate modifier contract. Current values must be finite and within `[0, Maximum]`.
9. **Double-application prevention.** No construction, BeginPlay, client callback, or Actor Info helper applies the effect. Only the server startup method can do so, and the per-instance guard is checked before spec creation. Repossession tests mutate values first, making any reset observable.
10. **No client application.** The method begins with authority and valid-Actor-Info checks and is not called from `OnRep_PlayerState`. Network tests retain the existing unauthorized-client mutation scenario and verify that only server replication reaches other worlds.
11. **Invalid content.** `UVelrathAbilitySet::IsDataValid` (working type name) and focused runtime checks require a non-null Instant effect, the exact four attributes, `Override` operations, validated order/ranges, and finite values. Invalid/missing content logs an actionable error, applies nothing, leaves the guard false, grants nothing dependent on successful startup, and has no C++ fallback.
12. **Archetypes now.** Do not create three startup assets or different values in GAS-006/007. Vanguard, Reaver, and Weaver reference one shared startup asset/effect. PVP-005 may add archetype composition later; different starting values require a new design/balance approval.
13. **No drift.** Remove `DefaultHealth`, `DefaultMaxHealth`, `DefaultStamina`, `DefaultMaxStamina`, constructor balance initialization, direct `SetNumericAttributeBase` startup writes, and Blueprint/Data Asset magnitude fields when GAS-006 migrates. Structural zero initialization is allowed; it is not a playable fallback. The GE is runtime truth and CS/test assertions detect asset drift.
14. **GAS-006/007 tests.** Add pure content validation; valid/invalid/missing effect cases; one AttributeSet; server order and postconditions; duplicate callback/repossess preservation; new-pawn initialization; client non-authority; and server/owner/remote convergence. GAS-007 additionally validates unique ability entries, effect/ability class references, retained handle ownership, server-only idempotent grants/removal, and owner-only ability-spec delivery without implementing combat abilities.
15. **GAS-010 gate.** Move all relevant tests under or include them in the stable `VELRATH.GAS` filter, require at least one executed test with zero failed/notRun, and include the real listen-server server/owner/remote lifecycle scenario. GAS-010 cannot pass from data validation or single-world automation alone.

Blueprint/Data Asset reinstance is an Editor authoring event, not respawn. Initialization is never called from construction or reinstance callbacks. Recompiling content must not rebalance an already initialized live Character; a subsequent authoritative spawn uses the new validated content. GAS-006 must include an Editor-side reinstance check of the configured reference and verify that any still-live initialized Character is not reset. If Unreal replaces a PIE Actor object during reinstance, that editor-only replacement is recorded separately and must not be reported as gameplay respawn verification.

#### Minimal implementation sequence

| Step | Task/scope | Minimum classes/assets/files | Verification | Done when |
| --- | --- | --- | --- | --- |
| 1 | GAS-006 startup reference and validation shell | `UVelrathAbilitySet : UPrimaryDataAsset` with one initial-effect class reference; one shared `DA_VelrathSharedStartup`; data-validation test. No ability grant behavior. | Valid reference passes; missing/wrong duration/attribute/op/order/range fails. | The schema contains no attribute magnitude field and invalid content cannot reach application. |
| 2 | GAS-006 effect application/migration | One Editor-authored `GE_VelrathInitialAttributes`; `AVelrathCharacter` server startup method/guard; remove balance defaults/direct startup writes from `UVelrathAttributeSet` while preserving replication/clamps. | Editor build; local initialization and invariant tests; reinstance check. | One valid server application produces the approved tuple and no fallback source remains. |
| 3 | GAS-006 lifecycle/network acceptance | Extend the existing CQTest rather than create a parallel harness. | Spawn, changed-value repossession, replacement pawn, invalid client mutation, server/owner/remote convergence, exactly one AttributeSet. | ATTR-01 through ATTR-06 pass in listen-server PIE; no Dedicated Server claim is made. |
| 4 | GAS-007 schema and grant lifecycle | Add shared foundational ability entries and a small runtime granted-handle container to the same `UVelrathAbilitySet` contract; grant only on the authoritative Character after Actor Info initialization. Do not add archetype-specific data or real combat abilities. | Data validation, duplicate-entry rejection, handle ownership/removal tests, duplicate request/repossession tests, listen-server owner/remote visibility, Editor build. | Schema references effect/ability classes once, preserves handles for their issuing set/ASC, grants idempotently on the server, and contains no balance magnitudes. |
| 5 | GAS-008/009 then GAS-010 | Implement server-only grants and full possession/respawn cleanup only in their own tasks; consolidate afterward. | Duplicate grants/client rejection, old-pawn retirement, stable `VELRATH.GAS` filter and CQTest. | GAS-008/009 criteria pass, then GAS-010 reports executed tests with zero failed/notRun and required network roles. |

TD-22 approves documentation and task boundaries only. No C++, Blueprint, Data Asset, Gameplay Effect, or GAS-006/007 implementation is created by this decision task.

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

Technical questions blocking these five: **none**. `TD-03` is approved; MOV-03 implementation still awaits Combat States and does not change the baseline/network Character Movement acceptance completed in FND-006. `TD-22` is approved and GAS-006 is complete; GAS-007 remains the next startup-data implementation task and does not change the completed AttributeSet declarations.

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
