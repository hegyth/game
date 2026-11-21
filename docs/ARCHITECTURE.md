# VELRATH - Technical Architecture

Status: architecture baseline for the MVP. This document defines responsibilities and dependency boundaries; it does not approve unresolved game rules or balance values. `GAME_VISION.md` remains the product authority.

## 1. Overview

VELRATH is a Windows PC / Steam competitive 3v3 Action RPG built with Unreal Engine 5.7.4, C++, Gameplay Ability System (GAS), Enhanced Input, and standard Unreal networking.

The MVP uses one runtime module, `VELRATH`. Combat is server-authoritative. The owning client may predict supported movement and abilities, but the server decides ability validity, hits, damage, resource changes, Echo placement, and match results.

Current implementation includes `AVelrathCharacter`, its Character-owned Ability System Component (ASC), `UVelrathAttributeSet` with replicated Health/MaxHealth/Stamina/MaxStamina, and `AVelrathGameMode`. The remaining types described here are planned responsibilities, not existing classes.

## 2. Architecture Principles

- Use Unreal Gameplay Framework, GAS, Character Movement, replication, and prediction before introducing custom systems.
- Keep the single `VELRATH` runtime module until a demonstrated build-time or dependency need justifies another module.
- Keep authoritative state on the server and replicate only state needed by clients.
- Keep persistent match data separate from pawn-lifetime combat data.
- Prefer small, focused C++ types and data-driven configuration over broad inheritance hierarchies.
- Do not create a second ability, effect, cooldown, combat-state, or resource framework beside GAS.
- Do not hard-code balance values in gameplay flow. Numerical combat rules belong to the future `docs/combat/COMBAT_SPEC.md` and corresponding data/effect assets.
- Treat Blueprint as composition and presentation over stable C++ contracts, not as a second authority model.
- Add abstractions only after at least one concrete responsibility requires them.

## 3. Gameplay Framework

### AVelrathCharacter

| Area | Responsibility |
| --- | --- |
| Purpose | Replicated player pawn, movement avatar, and owner/avatar of pawn-lifetime GAS combat state. |
| Stores | ASC, `UVelrathAttributeSet`, transient combat references, presentation hooks, and pawn-specific configuration. It does not own persistent score or team statistics. |
| Replicates | Standard Character movement, ASC state, replicated attributes, gameplay tags/effects as configured by GAS, and only the minimum non-GAS presentation state that cannot be derived. |
| Server only | Startup ability grants, authoritative effect application, hit confirmation, damage, resource changes, Echo spawn requests, death handling, and combat-state transitions. |
| Client | Enhanced Input forwarding, local camera/presentation, supported GAS prediction, montage/cue presentation, and read-only observation of replicated state. |

The ASC remains on Character for the MVP. A new Character receives a new ASC and new combat state after respawn. Moving the ASC to PlayerState is prohibited unless a confirmed requirement says effects or attributes must survive pawn replacement.

### AVelrathPlayerController

| Area | Responsibility |
| --- | --- |
| Purpose | Connection-owned input and local player orchestration. |
| Stores | Local input state, input mapping setup, local UI references, and client-only camera preferences. No authoritative combat statistics. |
| Replicates | Only data explicitly required between its server instance and owning client; PlayerController is not visible to every client. |
| Server only | Possession orchestration and validation entry points that cannot be expressed through GAS. It must not accept client-reported damage or hits. |
| Client | Convert Enhanced Input actions to input tags/ability input, manage local UI, and request supported actions through GAS or narrow validated RPCs. |

### AVelrathPlayerState

| Area | Responsibility |
| --- | --- |
| Purpose | Long-lived player identity and match statistics across Character death and respawn. |
| Stores | `TeamId`, kills, deaths, score, and other approved match-level statistics. It does not own the MVP ASC. |
| Replicates | Match statistics and team identity to relevant clients using replicated properties and RepNotify only where presentation needs a callback. |
| Server only | Mutate team assignment, kills, deaths, score, and other authoritative statistics. |
| Client | Read replicated values for UI and presentation. No client-side score mutation. |

### AVelrathGameMode

| Area | Responsibility |
| --- | --- |
| Purpose | Server-only authority for Convergence rules, login, team assignment, spawn selection, match transitions, death processing, and respawn. |
| Stores | Server-only rule configuration and transient orchestration state. Replicated match state belongs in GameState. |
| Replicates | Nothing; GameMode exists only on the server. |
| Server only | All responsibilities. It creates/possesses Characters and updates GameState/PlayerState. |
| Client | No direct access. Client code must not depend on GameMode being present. |

### AVelrathGameState

| Area | Responsibility |
| --- | --- |
| Purpose | Replicated view of the current Convergence match. |
| Stores | Approved match phase, match clock, team scores, and objective state once Convergence rules are defined. |
| Replicates | Client-visible match state, with RepNotify callbacks only for required presentation updates. |
| Server only | Mutate authoritative match state under GameMode control. |
| Client | Read state for UI, audio, and visual presentation. |

### Spawn mechanism

Start with standard `APlayerStart` actors and tags interpreted by GameMode. Add `AVelrathPlayerStart` only when typed team, phase, or Convergence-specific spawn metadata is demonstrably required.

Spawn choice and final transform are server-only decisions. Clients may display spawn presentation after possession but never select the authoritative spawn directly.

## 4. GAS Architecture

### Ability System Component

- One replicated ASC is created with each `AVelrathCharacter`.
- The Character is both Owner Actor and Avatar Actor for the MVP.
- `Mixed` replication mode remains appropriate for a player-owned Character, provided the Character is owned by its Controller after possession.
- On the server, call `InitAbilityActorInfo(this, this)` from `PossessedBy` before granting or activating startup abilities.
- On the owning client, initialize Actor Info when the replicated PlayerState/possession relationship is available. The current `OnRep_PlayerState` path is retained unless multi-instance testing proves an additional possession callback is required.
- Re-initializing Actor Info must not grant abilities. Initialization and grants are separate operations.

### UVelrathAttributeSet

`UVelrathAttributeSet` owns Health, MaxHealth, Stamina, and MaxStamina. Exactly one instance is registered with each Character-owned ASC.

- Define approved combat attributes only; their formulas and values come from `docs/combat/COMBAT_SPEC.md`.
- Replicate attributes that clients need for prediction, UI, or presentation.
- Use `GetLifetimeReplicatedProps`, per-attribute RepNotify functions, and `GAMEPLAYATTRIBUTE_REPNOTIFY`.
- Enforce invariants in standard AttributeSet hooks such as pre-change clamping and post-effect processing, without duplicating damage logic in Character.
- The server is authoritative for final attribute values.

### Initial attributes (TD-22)

- The sole runtime source of the four starting magnitudes is one human-authored Instant Gameplay Effect. Its four `Override` modifiers initialize MaxHealth, Health, MaxStamina, and Stamina; no starting magnitude is duplicated in C++, Blueprint defaults, or the startup Data Asset.
- The shared startup Primary Data Asset references that Gameplay Effect class but does not repeat its magnitudes. GAS-007 extends the same schema with shared foundational ability references and retained grant handles rather than creating a second loadout/configuration path.
- The initialization effect sets each maximum before its corresponding current value. The approved asset therefore produces Health/MaxHealth `1000/1000` and Stamina/MaxStamina `100/100`, while normal AttributeSet clamping remains the final invariant boundary.
- Only the authoritative Character creates an outgoing effect spec and applies it to its own ASC. This happens once, synchronously after server `InitAbilityActorInfo(this, this)` succeeds. Clients never apply the effect; they receive the four attributes through normal AttributeSet replication.
- A transient, server-only per-Character guard belongs to startup orchestration, not Actor Info initialization. Re-running `PossessedBy` for the same Character may reinitialize Actor Info but cannot reapply the effect or reset changed attributes. A newly spawned respawn Character owns a fresh ASC, AttributeSet, and guard, so it receives one new initialization.
- Missing startup data, a null/wrong-duration effect, an unexpected attribute/modifier operation, duplicate/missing modifiers, non-finite/negative magnitudes, or current values above their maxima is a validation/startup error. Initialization fails closed, logs an actionable error, and does not fall back to C++ constants or mark the guard complete.
- Blueprint construction, `OnConstruction`, client replication callbacks, and Data Asset/Blueprint reinstance are not initialization triggers. An already initialized live Character is not silently rebalanced when content is recompiled; a newly spawned Character reads the current validated asset.

### Shared startup abilities (GAS-007)

- The same shared startup Primary Data Asset owns the configurable list of foundational Gameplay Ability classes. Empty lists are valid until real abilities are approved; null, duplicate, abstract, deprecated, or non-Gameplay-Ability entries fail validation.
- Only the authoritative Character calls `GiveAbility`, after `InitAbilityActorInfo(this, this)`. Actor Info initialization itself never grants abilities.
- Each successful grant retains its `FGameplayAbilitySpecHandle` in transient Character-owned state together with the issuing Ability Set and target ASC. Repeated grant requests and same-Character repossession are idempotent.
- Removal uses only those retained handles and requires the original authoritative ASC and issuing Ability Set, so abilities granted by other systems remain untouched.
- A replacement Character owns a fresh ASC and grant state. GAS replication delivers ability specs to the owning client according to normal Mixed-mode visibility; server-only handle bookkeeping is not replicated.
- Ability grants and the TD-22 initialization effect have separate guards. Granting abilities never resets Health or Stamina.

### Abilities, effects, tags, and cues

- **Gameplay Abilities:** active combat actions, including attacks, block, parry, dodge, and Echo creation. Abilities own activation rules, costs, cooldowns, cancellation, and montage flow.
- **Gameplay Effects:** damage, healing if later approved, Stamina cost/recovery, buffs, debuffs, cooldowns, and other attribute/tag changes. Direct arbitrary attribute mutation is not normal gameplay flow.
- **Gameplay Tags:** the canonical vocabulary for ability input, combat state, events, effect classification, immunity, cancellation, and presentation state. Avoid parallel booleans or enums for the same semantic state.
- **Gameplay Cues:** replicated/predicted cosmetic responses such as impact, block, parry, dodge, and Echo visuals/audio. Cues never decide gameplay outcomes.
- **Data Assets:** archetype ability sets, startup loadouts, and approved configurable ability/Echo definitions. They reference abilities and effects; they do not execute authority logic.

Core tags used by C++ contracts should be declared centrally as native gameplay tags. Content-only tags may be sourced from the project Gameplay Tags configuration, but a tag must have one canonical definition and documented meaning.

### Startup grants and respawn

1. GameMode spawns and possesses a new Character on the server.
2. `PossessedBy` initializes server Actor Info.
3. The server applies the one TD-22 initial-attribute effect if that Character has not completed attribute initialization.
4. Separately, the server grants the shared startup ability set once. Attribute initialization and ability grants use distinct server-only per-Character guards; the grant operation retains granted handles when later removal is required.
5. Replication delivers abilities, effects, tags, and attributes to the owning client according to GAS rules.
6. The owning client initializes Actor Info through the possession/PlayerState replication path.
7. On death, the old Character and its combat state are retired. Respawn creates a new Character, ASC, AttributeSet, and one new startup grant sequence. PlayerState statistics persist.

Damage effects and combat results are applied only by the server. Client prediction may improve responsiveness but cannot confirm damage, parry success, death, score, or Echo validity.

## 5. Combat Architecture

| Feature | GAS responsibility | Supporting Unreal/C++ responsibility |
| --- | --- | --- |
| Light Attack | Gameplay Ability, activation tags, cost/cooldown if approved, montage flow, gameplay event windows. | Character animation data and a focused reusable hit-query helper only if multiple abilities require it. |
| Heavy Attack | Separate Gameplay Ability or data-configured attack ability when behavior is genuinely shared; GAS handles state and commitment. | Animation and server trace shape/configuration. |
| Hit Detection | Ability opens/closes an approved hit window and requests authoritative resolution. | Server performs traces/overlaps against authoritative world state. Client traces may be cosmetic and never apply damage. |
| Damage | Server creates/applies a Gameplay Effect Spec. Use an execution calculation only when the approved formula needs one. | Target validation and hit context creation. Never accept a client-reported final damage value. |
| Block | Gameplay Ability and blocking gameplay tag/state; server resolves incoming hit interaction. | Animation/presentation derived from GAS state. |
| Parry | Gameplay Ability with server-evaluated active window and tags/events. | Montage timing signals; final success remains server-authoritative. |
| Dodge | Gameplay Ability with cost, tags, cancellation, and supported prediction. | Character Movement/root motion mechanisms supported by Unreal; no custom movement protocol. |
| Stamina | Replicated AttributeSet attribute, Gameplay Effect costs and recovery, and ability cost checks. | UI observes replicated/predicted GAS values. Avoid per-frame client-authoritative mutation. |
| Combat States | Gameplay tags and GAS activation/cancellation relationships. | A minimal replicated presentation value is allowed only if tags cannot serve the requirement. |
| Animation Montages | Ability-controlled montage tasks and gameplay events/notifies. | AnimInstance presents movement/combat state; notifies mark windows but do not decide authority. |

### Combat timing and scheduling (TD-01)

VELRATH uses a hybrid timing model. A focused world-scoped logical combat clock advances integer combat ticks at a target rate of `60 Hz`; GAS continues to own ability lifecycle, costs, cooldowns, effects, and canonical state tags; `UCharacterMovementComponent` continues to own physical movement and its prediction/correction; animation remains presentation. The combat clock is not a second ability framework and does not place Unreal physics or Character Movement in a custom fixed-step loop.

The authoritative clock exists in the server's gameplay `UWorld`. Its driver runs once per Unreal World Tick with the world's gameplay `DeltaSeconds`, accumulates elapsed time in double precision, and emits zero or more `1/60`-second logical steps. It must not use an Actor/Component `TickInterval` or an Unreal timer as a claim of exact `60 Hz` execution. Each emitted step increments one monotonic `uint64` tick before processing that tick. Step extraction must preserve a normalized sub-step remainder and pass long-run drift tests; it must not depend on exact equality between decimal frame deltas. A maximum of eight combat steps may execute in one World Tick; excess accumulated time remains pending and is never converted into skipped combat ticks or a larger variable step. Sustained inability to drain that backlog is a server performance failure to measure and report, not permission to change authored windows.

An authoritative action records its server `StartTick` when GAS accepts activation. Its local tick is `CurrentTick - StartTick`, and every window is evaluated as a half-open integer interval `[startTick, endTick)`. State transitions scheduled for tick `N` are applied before authoritative resolution for tick `N`. Server hit queries use the authoritative world state available when the combat step executes; catch-up does not invent intermediate Character Movement or physics transforms.

The owning client may use normal GAS prediction and montage prediction for responsiveness, but predicted presentation never advances the authoritative clock or confirms a hit, Parry, Dodge invulnerability, damage, resource change, Echo result, or death. Replication sends state changes and sparse tick anchors rather than one replicated value per combat step. The authoritative epoch, anchor payload/cadence, and client estimate are defined by TD-02 below.

This contract guarantees exact discrete window boundaries and repeatable server ordering for identical authoritative inputs. It does not claim lockstep determinism of Unreal physics, Character Movement, render frames, network delivery, or simulations on different machines. Rollback and server rewind are not part of TD-01.

### Combat clock replication and network time mapping (TD-02)

Each authoritative gameplay `UWorld` creates one clock epoch when its world-scoped combat clock initializes at world begin play. `CurrentTick` starts at `0` before the first emitted combat step and remains a monotonic `uint64` for that world. Respawn does not reset it. World teardown or travel creates a new clock instance and epoch; action state from the previous epoch is not carried across implicitly.

Clients use the standard `AGameStateBase::GetServerWorldTimeSeconds()` estimate as the network time domain, but server world time is not itself the combat clock. The replicated `AVelrathGameState` publishes a compact clock anchor containing the clock epoch, a monotonic anchor sequence, the authoritative `CurrentTick`, and the sampled server world time for the same instant. Under normal operation it refreshes at `10 Hz`, aligned with the default GameState server-time update frequency, and immediately requests replication when the epoch starts or combat-clock overrun begins or ends. It does not send an RPC or property update for every combat step.

From the newest valid anchor `(AnchorTick, AnchorServerTime)`, a client forms a presentation-only estimate using `AnchorTick + floor(max(0, EstimatedServerTime - AnchorServerTime) * 60)`. The estimate is capped to 30 ticks beyond the last authoritative anchor; when that 500 ms horizon is exhausted, presentation holds or falls back to the last authoritative phase until a newer anchor arrives. A newer anchor always rebases the estimate because server hitch/backlog can make `CurrentTick` temporarily lag elapsed server world time. Small corrections may be smoothed inside one authoritative phase; an epoch change, cancellation, terminal state, or contradictory authoritative phase is applied immediately. This estimate never validates hits, damage, Parry, Dodge, resource changes, cooldowns, Echo outcomes, or death.

Replicated combat actions are state snapshots, not event streams. The owning replicated Character or feature Actor supplies its own network identity; the minimal action state adds an action type, server-assigned instance ID, monotonic revision, authoritative `StartTick`, current phase/state and its change tick, and optional authoritative end tick/reason. The owner correlates the accepted instance with the normal GAS activation prediction key; remote clients need only the authoritative instance. A new GameState/epoch resets cached mapping and action presentation; within one epoch, clients discard an older anchor sequence or action instance/revision. Late join receives the current GameState anchor and the current replicated action/feature snapshots, so reconstruction does not depend on an earlier multicast.

Authoritative gameplay events remain exactly-once on the server through feature-owned keys such as `(ActionInstanceId, Target)` or `(ActionInstanceId, EventKind, BoundaryTick)`. Client presentation deduplicates cosmetic events by replicated actor identity, clock epoch, action instance, event kind, and authoritative event tick; correcting or replaying a montage must not apply a gameplay event again. Ability cancellation, interruption, pawn destruction, and respawn publish or imply a newer terminal/replacement state and clear unmatched local prediction through standard GAS reconciliation. Character Movement SavedMoves remain independent: they carry movement intent and receive normal server correction, never combat-clock authority.

Clock synchronization does not provide lag compensation. Historical transforms, client-shot timestamps, server rewind, and acceptance thresholds remain a separate measured decision.

Do not introduce a general Combat Manager or parallel ability component preemptively. A focused component is allowed only for a proven shared non-GAS responsibility, such as reusable weapon trace state, and must not own ability activation, costs, cooldowns, effects, or authoritative combat state.

Balance values, timings, trace dimensions, damage formulas, and Stamina numbers originate in `docs/combat/COMBAT_SPEC.md`. Any value repeated here only identifies an architecture contract and does not become a second balance authority.

## 6. Resonance Architecture

### Echo creation

- Echo creation is a Gameplay Ability activated through the Character ASC.
- The owning client may present a predicted placement preview and submit target data.
- The server validates the ability, owner, current limits, and all placement constraints defined by approved Echo data.
- Only the server spawns the authoritative Echo actor.

### Echo entity and lifecycle

- Use a replicated Actor for an Echo because it has world identity, transform, relevancy, lifetime, and presentation.
- The Echo stores only the replicated identity/state required by clients, such as owner/team reference, approved state, and lifecycle timing.
- Lifecycle is server controlled: request, validate, spawn, activate, expire/cancel, destroy.
- Server destruction is the authoritative cleanup path. Client visuals respond to replication, `OnRep` callbacks, actor teardown, and Gameplay Cues.
- Do not give every Echo an ASC unless a concrete requirement cannot be represented by the creator ASC, Gameplay Effect Specs, tags, and ordinary replicated actor state.

### Effects and limits

- Echo gameplay interactions apply Gameplay Effects through GAS on the server, preserving source ASC, target ASC, tags, and effect context.
- Clients may predict cosmetics but never apply authoritative Echo effects.
- The server owns the active-Echo count and rejects placement above the approved limit.
- Initially keep a small server-side collection of active Echo references associated with the owning Character. Introduce a focused Resonance component only when lifecycle/query responsibilities become large enough to justify it.
- Remove references on Echo destruction and Character teardown. Replicate the count only if UI requires it.
- Limit values and placement rules belong to data/specification, not hard-coded architecture.

Use replication relevancy, dormancy, and cue-based cosmetics only after measuring Echo count and bandwidth. Do not build a custom synchronization protocol.

## 7. Multiplayer Architecture

- **Authority:** listen server is the current authoritative runtime. The same gameplay authority model must remain compatible with a future dedicated server.
- **Replication:** use Actor/component/property replication, RepNotify, replicated subobjects, and standard RPC ownership rules.
- **Movement:** use `ACharacter` and `UCharacterMovementComponent` prediction/correction. Custom movement must extend supported movement prediction rather than bypass it.
- **Abilities:** use GAS prediction keys and prediction policies only for abilities that are safe to predict. Server activation and commit remain final.
- **Hits:** server traces or validates hit windows and applies effects. Client hit feedback may be predicted and corrected.
- **Combat state:** prefer ASC attributes, effects, tags, and montage replication. Replicate separate state only when it has a distinct non-GAS purpose.
- **Echoes:** server-spawned replicated actors; clients receive state and cosmetic cues through standard Unreal mechanisms.

### Lag compensation

Begin with server-authoritative present-time hit evaluation plus standard Character Movement and GAS prediction. Instrument and playtest under representative latency before adding rewind.

If evidence shows unacceptable melee hit rejection, add narrowly scoped, bounded server-side rewind for the affected hit types using server-maintained history. The client may submit an action timestamp/aim context but never a trusted hit result. Do not create a general custom network protocol at the MVP stage.

### Network verification

- Use PIE with a listen server and at least two game instances for routine local checks.
- Verify owning client, remote client, and server views separately.
- Test possession, respawn, ability grants, attribute replication, combat outcomes, Echo spawn/destruction, and late observation where relevant.
- Record the number of instances and net mode in test reports. A single-instance test is not multiplayer verification.
- Do not run `VELRATHServer` with the current Launcher engine. Dedicated-server build and deployment are a future source-engine task.

## 8. Project Structure

Keep one mirrored Public/Private structure:

```text
Source/VELRATH/
  Public/
    Player/
    Combat/
    Abilities/
    Attributes/
    Resonance/
    GameModes/
    UI/
  Private/
    Player/
    Combat/
    Abilities/
    Attributes/
    Resonance/
    GameModes/
    UI/
    Tests/
```

- `Player/`: Character, PlayerController, PlayerState, and player-owned coordination.
- `Combat/`: focused hit/damage contracts and shared combat utilities that are not abilities.
- `Abilities/`: Gameplay Ability classes, ability sets, effect calculations, and ability tasks only when standard tasks are insufficient.
- `Attributes/`: AttributeSets and their replication/invariant logic.
- `Resonance/`: Echo actors and focused Resonance support types.
- `GameModes/`: GameMode, GameState, spawn metadata, and Convergence orchestration.
- `UI/`: C++ view models/controllers only when needed; widgets remain presentation.

The existing `Game/VelrathGameMode` path is functional. Move it to `GameModes/` only as part of a related change that introduces the rest of the match framework; do not create churn solely to rename a folder.

## 9. Data and Configuration

- `docs/GAME_VISION.md` defines approved product direction and non-goals.
- This document defines technical ownership and boundaries.
- `docs/combat/COMBAT_SPEC.md` defines approved combat rules and numerical balance intent.
- Core gameplay tags are centralized and documented; do not use ad hoc string tags in feature code.
- Primary Data Assets define archetype ability sets, startup loadouts, and approved configurable Echo/ability references. For TD-22, the shared startup asset references the initial-attribute effect but does not duplicate its modifier magnitudes.
- Gameplay Effect assets/classes hold costs, cooldowns, attribute modifiers, and durations once approved.
- Enhanced Input assets define input actions and mapping contexts; C++ maps them to stable input tags/actions.
- Blueprint and Data Asset `.uasset` files are binary. Codex may change their C++ schema and document required editor work, but must not edit the binary files directly.
- Source-controlled runtime balance must have one implementation authority. The Combat Specification remains the normative acceptance oracle, but a value must not be implemented independently in C++, Blueprint defaults, Data Assets, and Gameplay Effects. TD-22 starting attribute magnitudes are implemented only by the approved Instant Gameplay Effect.

## 10. Dependency Rules

- `Attributes` depends on Core/GAS contracts, not Player, UI, GameMode, or Resonance.
- `Combat` exposes small shared data/contracts and may depend on GAS; it must not depend on UI or match orchestration.
- `Abilities` may consume Attributes, Combat contracts, Resonance interfaces, tags, and approved data. Other systems must not bypass the ASC to reproduce ability behavior.
- `Resonance` may depend on GAS and narrow Combat contracts. It must not depend on UI or concrete GameMode implementation.
- `Player` composes the pawn/controller/state responsibilities and may depend on lower-level gameplay systems. It must not become the implementation home for every feature.
- `GameModes` orchestrates match flow through public gameplay interfaces. Runtime combat code must not require GameMode, because clients do not have it.
- `UI` reads PlayerState, GameState, and ASC state and sends local intent through PlayerController/GAS. UI never mutates authoritative gameplay state directly.
- Prefer forward declarations and narrow public headers. Avoid circular includes and cross-folder access to private implementation details.

## 11. AI Development Rules

- Scope each task to one responsibility and inspect its public contract, implementation, tests, and directly referenced data only.
- Preserve the ownership boundaries in this document. A task must state why a boundary changes before changing it.
- New abilities use GAS: define tags and data, implement activation/authority/cost/cancellation, connect presentation, then add focused tests.
- New attributes require a documented purpose, replication policy, AttributeSet invariant handling, and multiplayer verification.
- Keep authoritative logic in C++; use Blueprint for composition, references, montages, VFX, audio, and tuning within approved contracts.
- Do not modify binary Blueprint/Data Asset files directly. Report required editor-side changes explicitly.
- Store balance in the approved spec and data/effect layer, not scattered literals.
- Run `Scripts/Build.ps1 -Target Editor` after C++ changes and `Scripts/Test.ps1` with the narrowest relevant filter.
- Multiplayer claims require actual multi-instance PIE/listen-server execution.
- Update this document only for durable responsibility/dependency changes. Update `GAME_VISION.md` only when an approved product decision changes.

## 12. Testing Strategy

### Quality gates

| Change type | Minimum gate |
| --- | --- |
| C++ | Editor target builds; relevant automation tests pass; no unrelated warnings/errors introduced. |
| Gameplay Ability | Activation and rejection paths tested; authority/prediction policy explicit; cost, tags, cancellation, duplicate grants, and effect application covered where applicable. |
| AttributeSet | Replication properties and RepNotify verified; invariants tested; server and owning/remote client values checked in multi-instance PIE. |
| Network logic | Server, owning client, and remote client observed in at least two instances; invalid client request rejected; reconnect/respawn path tested when relevant. |
| Echo | Valid and invalid placement, authoritative spawn, active limit, effect source, expiration/destruction, cleanup, and remote visual synchronization tested. |
| Blueprint | Blueprint compiles in Editor; references are valid; authority logic remains in C++; affected gameplay is exercised in PIE. |
| Architecture refactor | Behavior preserved by existing tests; dependency direction improves or remains valid; Editor build passes; decision and migration impact documented. |

### Test layers

1. Fast C++ automation tests for deterministic helpers, tags, data validation, and AttributeSet rules.
2. Editor/functional tests for ability and actor lifecycle.
3. Multi-instance PIE/listen-server tests for possession, prediction, replication, respawn, combat, and Echoes.
4. Performance profiling on defined target hardware once representative content exists.

Tests must fail when no matching test runs. Current scripts provide unique Automation report paths and validate report counts.

## 13. Known Limitations

- Epic Launcher UE 5.7.4 cannot build the existing `VELRATHServer` target. Dedicated server verification is unavailable.
- Only Character and GameMode currently exist from the planned Gameplay Framework.
- GAS-006 initializes startup attributes through the approved TD-22 shared Primary Data Asset and Instant Gameplay Effect; no playable C++ fallback magnitudes remain.
- Current GAS content/lifecycle/CQTest coverage verifies the TD-22 asset/effect contract, validated shared ability references, retained grant ownership, server-only grants, owner-only ability-spec delivery, attribute replication, client non-authority, repossession, and pawn replacement. The production startup ability list remains intentionally empty until real foundational abilities are approved.
- Convergence rules, combat numbers, Echo rules, archetype kits, and target hardware specification remain unresolved in `GAME_VISION.md`.
- Steam sessions, matchmaking, authentication, and deployment architecture are not yet selected or implemented.
- Binary Blueprint/Data Asset content cannot be safely authored through text-only Codex changes.

## 14. Architecture Decisions

| ID | Decision | Rationale |
| --- | --- | --- |
| AD-001 | Keep one `VELRATH` runtime module for MVP. | Minimizes build and dependency complexity. |
| AD-002 | Keep ASC on Character. | Combat state intentionally resets with the pawn on death/respawn. |
| AD-003 | Store long-lived match statistics on PlayerState. | Team and score data must survive pawn replacement and replicate to clients. |
| AD-004 | Use GAS for combat actions, resources, effects, tags, and cues. | Prevents a parallel framework and uses Unreal prediction/replication support. |
| AD-005 | Make consequential combat and Echo results server-authoritative. | Required for competitive integrity. |
| AD-006 | Use standard Unreal replication, Character Movement, and GAS prediction. | Avoids premature custom networking. |
| AD-007 | Represent authoritative Echoes as replicated Actors. | Echoes need world identity, lifecycle, relevancy, and synchronized presentation. |
| AD-008 | Use standard `APlayerStart` plus tags until typed metadata is required. | Avoids an unnecessary subclass. |
| AD-009 | Keep balance outside architecture code. | Enables controlled tuning without scattering constants. |
| AD-010 | Use PIE/listen server until a source-engine dedicated-server path exists. | Matches current toolchain capability without changing authority design. |
| AD-011 | Use a server-authoritative 60 Hz logical combat clock with an eight-step bounded catch-up inside a hybrid GAS/Character Movement architecture. | Gives integer combat-window boundaries and deterministic server ordering without replacing GAS, Character Movement prediction, Unreal physics, or animation timing. |
| AD-012 | Map client presentation to combat time through synchronized GameState server time plus sparse global and per-action tick anchors. | Supports late join and correction without trusting client time or replicating every combat tick, including when server backlog breaks a direct world-time-to-tick formula. |
| AD-013 | Use one Instant Gameplay Effect as the sole runtime source of starting attribute magnitudes, referenced without duplicated values by the shared startup Data Asset and applied once per new Character by the server. | Keeps initialization GAS-native and Editor-tunable while preserving Character-lifetime state, repossession, respawn, and replication contracts. |

## 15. Open Technical Questions

These are unresolved and must not be treated as approved implementation requirements:

- What exact Convergence state machine, scoring model, and spawn rules follow from the final mode design?
- What are the exact ability sets and input bindings for Vanguard, Reaver, and Weaver?
- Which Echo placement constraints, active limit, lifecycle states, and relevancy rules are required?
- Which attack types need traces, overlaps, or another standard Unreal hit-query method?
- What measured latency threshold would justify narrowly scoped server rewind?
- Does any confirmed future feature require combat state to survive Character replacement and therefore justify reconsidering ASC ownership?
- Which Unreal online service/subsystem will provide Steam sessions, matchmaking, and authentication?
- What target hardware and network conditions define performance and multiplayer acceptance tests?
