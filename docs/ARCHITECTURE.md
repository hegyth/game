# VELRATH - Technical Architecture

Status: architecture baseline for the MVP. This document defines responsibilities and dependency boundaries; it does not approve unresolved game rules or balance values. `GAME_VISION.md` remains the product authority.

## 1. Overview

VELRATH is a Windows PC / Steam competitive 3v3 Action RPG built with Unreal Engine 5.7.4, C++, Gameplay Ability System (GAS), Enhanced Input, and standard Unreal networking.

The MVP uses one runtime module, `VELRATH`. Combat is server-authoritative. The owning client may predict supported movement and abilities, but the server decides ability validity, hits, damage, resource changes, Echo placement, and match results.

Current implementation includes `AVelrathCharacter`, its Character-owned Ability System Component (ASC), a placeholder base `UAttributeSet`, and `AVelrathGameMode`. The remaining types described here are planned responsibilities, not existing classes.

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

The current base `UAttributeSet` is a compile-time placeholder and must be replaced before adding real attributes.

- Define approved combat attributes only; their formulas and values come from `docs/combat/COMBAT_SPEC.md`.
- Replicate attributes that clients need for prediction, UI, or presentation.
- Use `GetLifetimeReplicatedProps`, per-attribute RepNotify functions, and `GAMEPLAYATTRIBUTE_REPNOTIFY`.
- Enforce invariants in standard AttributeSet hooks such as pre-change clamping and post-effect processing, without duplicating damage logic in Character.
- The server is authoritative for final attribute values.

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
3. The server applies initial attribute effects and grants the archetype's startup ability set.
4. A server-only per-Character guard prevents duplicate grants. The grant operation must also retain granted handles when later removal is required.
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

Do not introduce a general Combat Manager or parallel ability component preemptively. A focused component is allowed only for a proven shared non-GAS responsibility, such as reusable weapon trace state, and must not own ability activation, costs, cooldowns, effects, or authoritative combat state.

No balance values, timings, trace dimensions, damage formulas, or Stamina numbers are defined here.

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
- The future `docs/combat/COMBAT_SPEC.md` defines approved combat rules and numerical balance intent.
- Core gameplay tags are centralized and documented; do not use ad hoc string tags in feature code.
- Primary Data Assets define archetype ability sets, startup loadouts, and approved configurable Echo/ability references.
- Gameplay Effect assets/classes hold costs, cooldowns, attribute modifiers, and durations once approved.
- Enhanced Input assets define input actions and mapping contexts; C++ maps them to stable input tags/actions.
- Blueprint and Data Asset `.uasset` files are binary. Codex may change their C++ schema and document required editor work, but must not edit the binary files directly.
- Source-controlled balance must have one authority. Do not duplicate the same value across C++, Blueprint defaults, Data Assets, and documentation.

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
- The current `UAttributeSet` instance is a placeholder with no project attributes or attribute replication.
- Current automation covers module loading, not GAS replication or multiplayer behavior.
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

## 15. Open Technical Questions

These are unresolved and must not be treated as approved implementation requirements:

- What exact Convergence state machine, scoring model, and spawn rules follow from the final mode design?
- Which attributes and damage formula will `docs/combat/COMBAT_SPEC.md` approve?
- What are the exact ability sets and input bindings for Vanguard, Reaver, and Weaver?
- Which Echo placement constraints, active limit, lifecycle states, and relevancy rules are required?
- Which attack types need traces, overlaps, or another standard Unreal hit-query method?
- What measured latency threshold would justify narrowly scoped server rewind?
- Does any confirmed future feature require combat state to survive Character replacement and therefore justify reconsidering ASC ownership?
- Which Unreal online service/subsystem will provide Steam sessions, matchmaking, and authentication?
- What target hardware and network conditions define performance and multiplayer acceptance tests?
