# VELRATH - Combat Specification

Status: approved initial MVP implementation baseline. The values in this document are authoritative starting values for implementation and automated tests, but they are not final or validated by playtesting. Balance changes require an explicit design decision and an update to this document.

## 1. Overview

This document is the shared technical and game-design contract for VELRATH combat. It translates `GAME_VISION.md` and `ARCHITECTURE.md` into deterministic rules for Unreal Engine 5.7.4, C++, Gameplay Ability System (GAS), and server-authoritative multiplayer.

Source alignment:

- `GAME_VISION.md` remains the authority for product direction and non-goals.
- `ARCHITECTURE.md` remains the authority for ownership, subsystem boundaries, and Character-owned ASC.
- This document supplies the initial numerical combat and Echo baseline that those documents previously left open.
- Exact archetype-specific abilities remain Pending Design Approval.

Current code has a replicated Character-owned ASC in `Mixed` replication mode, initialized on the server in `PossessedBy` and on the owning client in `OnRep_PlayerState`. `UVelrathAttributeSet` owns the four replicated attributes, and GAS-006 implements TD-22 through the shared startup Primary Data Asset and one server-applied Instant Gameplay Effect with no playable C++ fallback magnitudes.

## 2. Design Goals

- Combat is readable, deterministic, and offers clear counterplay.
- The server is authoritative for damage, resources, cooldowns, control effects, state transitions, and Echoes.
- The owning client may predict supported actions, but prediction never determines the final result.
- The same combat foundation is shared by Vanguard, Reaver, and Weaver.
- No pay-to-win power is permitted.
- MVP critical chance is `0%`; there is no random critical damage.
- Gameplay rules use GAS rather than a competing state, cooldown, resource, or effect framework.
- Initial values are tuning hypotheses to validate through playtests, not claims of final balance.

## 3. Time Model

### Simulation contract

- Target combat simulation frequency: `60 Hz`.
- One simulation tick represents `1/60` second, approximately `16.6667 ms`.
- All combat windows and durations are integer simulation ticks.
- All intervals use half-open notation `[start, end)`: the start tick is included and the end tick is excluded.
- Tick `0` is the activation tick of the action unless a section states otherwise.
- Server simulation tick indices are authoritative. Client prediction uses the same authored tick windows and is corrected by the server.

TD-01 approves a hybrid scheduler for this contract. One server-world logical combat clock is driven once per Unreal World Tick by the world's gameplay `DeltaSeconds`. It retains elapsed time in a double-precision accumulator and emits zero or more fixed `1/60`-second combat steps; an Actor/Component tick interval or Unreal timer is not treated as an exact `60 Hz` clock. Each emitted step increments one monotonic `uint64` tick before processing that tick. Step extraction preserves a normalized sub-step remainder and must pass long-run drift tests rather than depending on exact floating-point equality at a boundary.

At most eight combat steps may run in one Unreal World Tick. Any additional accumulated time remains pending for later World Ticks: the scheduler does not discard ticks, enlarge the step, or jump window boundaries after a hitch. A server that persistently cannot drain the backlog is outside competitive performance acceptance and must expose overrun telemetry; it must not silently shorten or skip Startup, Active, Recovery, Parry, Dodge, or Echo windows.

An authoritative action stores the server `StartTick` accepted by GAS, and its local action tick is `CurrentTick - StartTick`. Catch-up steps advance discrete combat state and may repeat queries against the authoritative world transform currently available, but they do not substep or rewind Character Movement or Unreal physics. GAS owns ability/effect lifecycle, Character Movement owns physical movement and correction, and montages visualize/reconcile to the combat state rather than authorizing it.

TD-02 approves a hybrid network-time mapping. The authoritative clock epoch begins at `CurrentTick = 0` when the server gameplay world's clock initializes at world begin play. Respawn retains the epoch and tick sequence; world teardown/travel creates a new epoch. `AVelrathGameState` pairs sampled authoritative ticks with `AGameStateBase::GetServerWorldTimeSeconds()` in a replicated anchor containing epoch, sequence, tick, and sampled server time. The anchor updates at `10 Hz` under normal operation and requests an immediate update at epoch creation and combat-clock overrun begin/end. No per-combat-tick RPC or property update is permitted.

Clients estimate presentation time from the newest valid pair as `AnchorTick + floor(max(0, EstimatedServerTime - AnchorServerTime) * 60)`, capped at 30 unconfirmed ticks beyond the anchor. The estimate rebases on each newer anchor and holds/falls back after the 500 ms extrapolation horizon. It is expected to correct after server hitch/backlog because authoritative `CurrentTick` is not defined as `floor(ServerWorldTime * 60)`. Replicated action/feature state carries authoritative instance, revision, start, phase/state-change, and optional end anchors; that state overrides any estimate. Client time remains presentation/prediction input only and cannot authorize any combat result.

### Determinism rules

- Authored durations are stored and compared as integer ticks, not accumulated floating-point seconds.
- Fractional per-second resource rates may accumulate with sufficient precision, but authoritative window boundaries remain integer ticks.
- A transition scheduled for tick `N` is visible to authoritative resolution for tick `N` according to the ordered resolution rules in this document.
- Wall-clock conversions are informational only; tick values are the source of truth.

## 4. Movement

### Initial parameters

| Parameter | Design value | Unreal value |
| --- | ---: | ---: |
| Walk speed | 5.2 m/s | 520 cm/s |
| Sprint speed | 6.6 m/s | 660 cm/s |
| Ground acceleration | 30 m/s^2 | 3000 cm/s^2 |
| Ground deceleration | 36 m/s^2 | 3600 cm/s^2 |
| Backward movement multiplier | 0.85 | 0.85 |
| Movement speed while attacking | 0.45 | 0.45 multiplier |
| Movement speed while blocking | 0.60 | 0.60 multiplier |
| Character collision radius | 0.38 m | 38 cm capsule radius |
| Character collision height | 1.8 m | 90 cm capsule half-height |
| Target dodge distance | 4.0 m | 400 cm |

Use `ACharacter` and `UCharacterMovementComponent` for locomotion, acceleration, deceleration, prediction, and correction. Do not introduce a custom movement protocol.

Movement restrictions are driven by authoritative/predicted combat state. TD-03 approves the following movement-speed rules:

1. In Normal/Locomotion, use Walk `520 cm/s`, or Sprint `660 cm/s` while Sprint intent is held and the current state permits Sprint.
2. During Attack or Block, Sprint is prohibited. Use the Walk base `520 cm/s` regardless of Sprint intent.
3. Apply exactly one directional multiplier: `1.0` for forward/sideways movement or `0.85` for backward movement.
4. Apply exactly one combat-state multiplier: Normal `1.0`, Attack `0.45`, or Block `0.60`. Attack and Block are mutually exclusive canonical combat states, so their multipliers never stack.
5. The server computes and clamps the final allowed speed:

```text
FinalSpeed = ServerClamp(BaseSpeed × DirectionalMultiplier × CombatMultiplier)
```

6. Entering Attack or Block suppresses Sprint speed without clearing held Sprint intent. On exit, Sprint resumes when its intent is still held and the resulting state permits Sprint; otherwise movement uses Walk.

Expected speeds for the approved inputs are:

| Combat state | Sprint intent | Base speed | Direction | Directional multiplier | Combat multiplier | Expected final speed |
| --- | --- | ---: | --- | ---: | ---: | ---: |
| Normal | Not held | Walk `520` | Forward/sideways | `1.0` | `1.0` | `520 cm/s` |
| Normal | Not held | Walk `520` | Backward | `0.85` | `1.0` | `442 cm/s` |
| Normal | Held and allowed | Sprint `660` | Forward/sideways | `1.0` | `1.0` | `660 cm/s` |
| Normal | Held and allowed | Sprint `660` | Backward | `0.85` | `1.0` | `561 cm/s` |
| Attack | Either | Walk `520` | Forward/sideways | `1.0` | `0.45` | `234 cm/s` |
| Attack | Either | Walk `520` | Backward | `0.85` | `0.45` | `198.9 cm/s` |
| Block | Either | Walk `520` | Forward/sideways | `1.0` | `0.60` | `312 cm/s` |
| Block | Either | Walk `520` | Backward | `0.85` | `0.60` | `265.2 cm/s` |

The owning client may predict these factors only from movement intent and predicted combat state available to Character Movement replay. The server remains authoritative and corrects invalid combinations through the standard Character Movement path. No additional movement mechanics are defined by this specification.

## 5. Combat State Machine

GAS gameplay tags are the canonical combat-state representation. The proposed tag names below define semantic contracts; they do not require a second state-machine framework.

| State | Gameplay Tag | Purpose and entry | Exit and allowed transitions | Authority |
| --- | --- | --- | --- | --- |
| Locomotion | `State.Combat.Locomotion` | Default living, controllable state after possession, action completion, or recovery. | May enter LightAttack, HeavyAttack, Block, Parry, Dodge, AbilityCast, or EchoCast when GAS requirements pass. Server control effects or death may override it. | Server final; client predicts normal input-driven transitions. |
| LightAttack | `State.Combat.LightAttack` | Entered by successful Light Attack ability activation. | Returns to Locomotion at tick 36; may cancel to Dodge from tick 27 or Block from tick 29; may be overridden by authoritative control or death. | Server validates activation, hits, and exit; owner may predict. |
| HeavyAttack | `State.Combat.HeavyAttack` | Entered by successful Heavy Attack ability activation. | Returns to Locomotion at tick 60; may cancel to Dodge from tick 50 or Block from tick 54; may be overridden by authoritative control or death. | Server validates activation, hits, and exit; owner may predict. |
| Block | `State.Combat.Block` | Entered by Block ability activation when requirements pass. Protection begins after the 3-tick startup. | Returns to Locomotion on release/cancel; enters GuardBreak when authoritative Stamina exhaustion triggers it; may be overridden by control or death. A direct Block-to-Parry rule is Pending Design Approval. | Server resolves protection and Stamina; owner may predict pose/startup. |
| Parry | `State.Combat.Parry` | Entered by Parry ability activation after cost/cooldown checks. | Returns to Locomotion at tick 26; no normal cancel window is approved; may be overridden by authoritative control or death. | Server determines active-window success, refund, and attacker Stagger. |
| Dodge | `State.Combat.Dodge` | Entered by Dodge ability activation after cost/cooldown checks. | Returns to Locomotion at tick 24; no additional cancel window is approved; may be overridden outside invulnerability by authoritative control or death. | Server validates cost and final movement/result; owner may predict supported movement. |
| AbilityCast | `State.Combat.AbilityCast` | Generic state for an approved non-Echo archetype ability that requires a cast window. | Ability-specific completion and interruption rules are Pending Design Approval. Death and applicable control are authoritative overrides. | Server final; prediction policy is ability-specific. |
| EchoCast | `State.Combat.EchoCast` | Entered by Echo creation ability after activation and placement requirements pass. | Character returns to Locomotion after the 18-tick cast; the Echo entity then owns Formation and Active lifecycle. Exact cast interruption rules are Pending Design Approval. | Server validates placement/spawn; owner may predict preview and cast presentation. |
| Stagger | `State.Control.Stagger` | Applied by an authoritative control Gameplay Effect, including successful Parry against the attacker. | Returns to Locomotion when the server-calculated duration ends, unless another higher-priority state applies. | Server only for final application/duration. |
| GuardBreak | `State.Control.GuardBreak` | Applied when blocking Stamina is exhausted. | Returns to Locomotion after 36 ticks unless Dead overrides it. | Server only. |
| Dead | `State.Life.Dead` | Entered when authoritative HP reaches the death threshold. | Cancels combat abilities and remains until GameMode begins respawn. | Server only for final transition. |
| Respawn | `State.Life.Respawn` | Represents the server-controlled replacement of the dead Character. | Ends when the new Character is spawned, possessed, initialized, and enters Locomotion. | Server only. Exact representation across old/new Character lifetime is an open technical decision. |

### Transition and cancellation rules

1. GAS activation requirements and blocked/required tags decide whether an action may start.
2. Explicit authored cancel windows decide normal action-to-action cancellation.
3. Light and Heavy attacks only gain the specified Dodge and Block cancels; no other normal cancels are implied.
4. Authoritative Dead overrides all living states and cancels active combat abilities.
5. Authoritative Stagger and GuardBreak cancel abilities that declare themselves interruptible by those control tags.
6. An ability owns its montage/task cancellation and removes its state tags during deterministic cleanup.
7. Re-initializing ASC Actor Info must not grant abilities or duplicate state.
8. Input buffering, Block-to-Parry transition, and generic AbilityCast cancellation are not approved by this document.

## 6. Attack System

Light and Heavy attacks are Gameplay Abilities. Their tick phases, hit windows, state tags, cancellation, montage playback, and server hit requests are controlled through GAS and standard Unreal animation integration.

### Light Attack

| Phase/property | Value |
| --- | ---: |
| Startup | `[0, 12)` |
| Active | `[12, 19)` |
| Recovery | `[19, 36)` |
| Total | 36 ticks |
| Base damage | 115 |
| Range | 240 cm |
| Half-angle | 45 degrees |
| Stamina cost | 0 |
| Dodge cancel | `[27, 36)` |
| Block cancel | `[29, 36)` |

### Heavy Attack

| Phase/property | Value |
| --- | ---: |
| Startup | `[0, 22)` |
| Active | `[22, 31)` |
| Recovery | `[31, 60)` |
| Total | 60 ticks |
| Base damage | 180 |
| Range | 300 cm |
| Half-angle | 35 degrees |
| Stamina cost | 0 |
| Dodge cancel | `[50, 60)` |
| Block cancel | `[54, 60)` |

Only the Active interval can produce new attack hit candidates. Attack movement uses the `0.45` movement multiplier. The server owns AttackInstanceId creation and authoritative hit resolution. Client prediction may start the ability and presentation but does not confirm hits or damage.

## 7. Hit Detection

### Collision concepts

- **Collision Capsule:** the Character Movement capsule (`38 cm` radius, `90 cm` half-height) used for world movement and broad actor collision. It is not automatically the final combat hurtbox.
- **Hurtbox:** server-queryable target geometry representing valid damageable space. Exact bone/body segmentation is Pending Design Approval.
- **HitVolume:** the attack's authored server-side trace/sweep volume, constrained by range and directional half-angle. Exact sweep shape and sampling method are open technical decisions.

### Authoritative process

1. The server assigns an `AttackInstanceId` unique for the attacking Character's lifetime.
2. During the Active interval, the server performs the approved trace/sweep against authoritative world state.
3. Candidate targets must be valid, alive, inside range, and inside the attack direction sector.
4. Direction uses the attacker forward vector and direction to the candidate. A target passes when the angular test is within the attack half-angle.
5. The server keeps a per-attack set of targets already resolved. A target can be resolved at most once for one `AttackInstanceId`, even if multiple sweeps or hurtboxes find it.
6. Client-local hit detection may drive provisional cosmetics but is never the source of truth.

### Same-tick resolution

- Gather all server-valid hit candidates for the simulation tick before applying order-sensitive post-hit state changes.
- Snapshot the defensive state needed to resolve that tick.
- Resolve defensive priority independently for each target.
- Valid mutual hits on the same tick both resolve from the tick snapshot unless invulnerability, Parry, Block, or another approved rule rejects one.
- Apply results in a stable deterministic order for reproducible logs and tests.
- The exact stable network identifier used for ordering is an open technical decision; it must not depend on non-deterministic container iteration.

## 8. Damage Calculation

### Base values

| Parameter | Value |
| --- | ---: |
| Maximum HP | 1000 |
| Light base damage | 115 |
| Heavy base damage | 180 |
| Block damage reduction | 70% |
| Parry damage | 0 |
| Critical chance | 0% |

### Formula

```text
Damage = RoundHalfUp(
    BaseDamage
    x AttackModifier
    x DefenseModifier
    x StateModifier
    x GuardModifier)
```

For the MVP:

- `AttackModifier = 1.0`.
- `DefenseModifier = 1.0`.
- `StateModifier = 1.0`.
- `GuardModifier = 0.30` for a successful Block.
- `GuardModifier = 1.0` without Block.
- A successful Parry cancels damage before the normal formula is applied.

Expected examples:

| Case | Result |
| --- | ---: |
| Light, unguarded | 115 |
| Heavy, unguarded | 180 |
| Light, successful Block | 35 |
| Heavy, successful Block | 54 |
| Successful Parry | 0 |

The server performs the formula and `RoundHalfUp`. Damage is applied through a server-created GAS Gameplay Effect Spec with authoritative effect context. Client-provided final damage values are ignored. HP is clamped to its approved range; reaching the death threshold produces the authoritative Dead transition.

## 9. Attributes and Resources

### Initial shared attributes

All three MVP archetypes begin with:

- Health/Maximum Health: `1000/1000`.
- Stamina/Maximum Stamina: `100/100`.

`UVelrathAttributeSet` owns the replicated current and maximum values. Under TD-22, one Instant Gameplay Effect is the sole runtime implementation source for all four starting magnitudes. The shared startup Data Asset references that effect without repeating the numbers. The server applies it once to each new Character after Actor Info initialization, maxima before current values; clients only observe replication. Repossession of the same Character never reapplies it, while respawn initializes the new Character/ASC/AttributeSet once.

### Stamina rules

| Rule | Value |
| --- | ---: |
| Maximum | 100 |
| Regeneration | 18 per second (`0.3` per 60 Hz tick) |
| Regeneration delay after spending | 45 ticks |
| Dodge cost | 28 |
| Parry initial cost | 18 |
| Successful Parry refund | 12 |
| Successful Parry net cost | 6 |
| Block upkeep | 12 per second (`0.2` per 60 Hz tick) |
| Block-hit cost | 20% of incoming base damage |

Spending Stamina starts or refreshes the 45-tick regeneration delay. No regeneration is applied during that delay. Fractional regeneration/upkeep must retain sufficient precision so that rate behavior is not changed by per-tick rounding.

Parry ordering is mandatory:

1. Commit and spend `18` Stamina at authoritative activation.
2. If the server confirms success during `[2, 8)`, apply a `12` Stamina refund.
3. A success therefore costs `6` net; a failure costs the original `18`.

This can be represented by an activation cost Gameplay Effect and a server-only success refund Gameplay Effect. `UVelrathAttributeSet` can represent the resource changes; the ability/effects that execute this sequence remain future combat tasks.

Derived block-hit costs are `23` Stamina for Light (`20%` of 115) and `36` for Heavy (`20%` of 180), assuming the resource cost follows the same integer rounding rule. The exact rounding rule for resource costs, and the outcome when available Stamina is below the required block-hit cost, remain open technical/design questions.

## 10. Defensive Mechanics

### Ordered defensive resolution

For each authoritative hit candidate, resolve exactly in this order:

1. Invulnerability.
2. Parry.
3. Block.
4. Other approved mitigation.
5. Normal damage.

Once a higher-priority stage fully rejects the hit, lower stages do not apply.

### Block

| Property | Value |
| --- | ---: |
| Startup | 3 ticks (`[0, 3)`) |
| Defensive sector | 120 degrees total |
| Damage reduction | 70% |
| GuardBreak on Stamina exhaustion | 36 ticks |
| Upkeep | 12 Stamina/sec |
| Hit cost | 20% incoming base damage |

After startup, the attacker must be inside the defender's forward 120-degree sector for Block to succeed. The server evaluates direction and Stamina. GuardBreak applies when Stamina is exhausted. Whether the hit that exhausts insufficient Stamina receives Block reduction or normal damage is Pending Design Approval.

### Parry

| Phase/property | Value |
| --- | ---: |
| Startup | `[0, 2)` |
| Active | `[2, 8)` |
| Recovery | `[8, 26)` |
| Total | 26 ticks |
| Initial cost | 18 Stamina |
| Success refund | 12 Stamina |
| Success damage received | 0 |
| Attacker Stagger on success | 18 ticks |
| Cooldown | 36 ticks from activation |

The server determines whether the incoming attack intersects the Active interval. A successful Parry prevents normal damage, refunds Stamina, and applies authoritative Stagger to the attacker. Parry direction/sector requirements are not specified and remain Pending Design Approval.

### Dodge

| Phase/property | Value |
| --- | ---: |
| Startup | `[0, 4)` |
| Invulnerability | `[4, 13)` |
| Recovery | `[13, 24)` |
| Total | 24 ticks |
| Cost | 28 Stamina |
| Cooldown | 36 ticks from activation |
| Maximum distance | 400 cm |

The server owns final invulnerability and movement validity. The owning client may predict a GAS Dodge and supported Character Movement/root motion, then accept correction. Exact displacement profile and root-motion implementation are open technical decisions; the maximum distance remains `400 cm`.

## 11. Crowd Control

Approved control categories:

- Flinch.
- Stagger.
- GuardBreak.
- Slow.
- Knockback.

Control is applied through server-authoritative Gameplay Effects and gameplay tags. Exact Flinch, generic Stagger, Slow, and Knockback durations/strengths are Pending Design Approval except where this specification gives a value.

### Stagger diminishing returns

Within a rolling `120`-tick window per target:

- First Stagger: `100%` duration.
- Second Stagger: `50%` duration.
- Third and subsequent Staggers: `25%` duration.

The maximum cumulative loss of control is `42` ticks within a rolling `120`-tick window.

The server tracks the window and authoritative control budget. A new control effect cannot make counted loss of control exceed the remaining budget. Two details are unresolved:

- `25%` of the approved 18-tick Parry Stagger is `4.5` ticks, which conflicts with the integer-tick contract until a rounding rule is approved.
- The exact set of Flinch, GuardBreak, and Knockback intervals counted toward the 42-tick cap is not yet defined. Slow is not assumed to count without approval.

## 12. Resonance / Echo

Echo creation is a Character ASC Gameplay Ability. Placement preview may be client-predicted, but the server validates and spawns the authoritative replicated Echo actor.

### MVP parameters

| Property | Value |
| --- | ---: |
| Active Echoes per player | 1 |
| Maximum Echoes in 3v3 | 6 |
| Cast | 18 ticks |
| Formation | 24 ticks |
| Active lifetime | 180 ticks |
| Cooldown after expiration/destruction | 720 ticks |
| Maximum placement distance | 400 cm |
| Removal delay after owner death | 12 ticks |

### Lifecycle

```text
Cast -> Forming -> Active -> Expired/Destroyed -> Cooldown
```

- **Cast:** Character is in EchoCast for 18 ticks. Completion sends the server-approved spawn into Forming.
- **Forming:** the replicated Echo exists for 24 ticks but its Rift Field is not Active.
- **Active:** Rift Field is active for 180 ticks unless the Echo is destroyed earlier.
- **Expired/Destroyed:** the server ends effects, schedules actor cleanup, and begins the 720-tick cooldown.
- **Cooldown:** server-authoritative reactivation block lasting 720 ticks from expiration/destruction.
- **Owner death:** the server removes the Echo after 12 ticks. The point at which the cooldown is stored across Character respawn is unresolved because the MVP ASC is Character-owned and recreated.

### Rift Field

| Property | Value |
| --- | ---: |
| Radius | 220 cm |
| Enemy Slow | 25% |
| Allies | Unaffected |
| Same-type Slow stacking | Does not stack |
| Damage | 0 |
| Disrupt duration | 30 ticks |
| Disrupt distance | Within 150 cm |
| Disrupt interruption | Damage or control |

The server applies the non-stacking Slow through GAS to enemy targets inside the authoritative field. Rift Field deals no damage. A completed valid Disrupt ends the Echo through the server; exact team eligibility and interaction ownership are Pending Design Approval.

### Placement validation

At minimum, the server verifies:

- requester owns the ASC/ability activation;
- requester is in an allowed living/action state;
- Echo ability is granted and not on cooldown;
- no existing Forming/Active Echo already uses the player's one-Echo allowance;
- total authoritative Echo count does not exceed six;
- requested point is within 400 cm using server transform;
- placement does not overlap server-blocking geometry in a way forbidden by the final placement rule;
- submitted target data is finite, current, and valid for the server world.

Surface, line-of-sight, navigation, vertical offset, and slope rules are not approved and must not be invented during implementation.

### Replication

- Replicate actor identity, transform, owner/team identity, lifecycle state, and authoritative lifecycle timing needed by clients.
- Do not replicate a per-frame timer; clients derive presentation from replicated state/tick anchors and accept correction.
- Rift Field effects originate on the server through GAS.
- Gameplay Cues and replicated state drive formation, active, disruption, expiration, and destruction presentation.
- Use standard Unreal relevancy/dormancy after profiling; do not add a custom protocol.

## 13. Archetypes

MVP archetypes:

- Vanguard.
- Reaver.
- Weaver.

All three share:

- `1000` maximum HP.
- `100` maximum Stamina.
- the same foundational movement, Light Attack, Heavy Attack, Block, Parry, and Dodge rules in this document.

Archetype-specific abilities are separate configurable Gameplay Abilities referenced by archetype/startup data. Their exact mechanics, timings, costs, cooldowns, and effects are **Pending Design Approval** and must not be inferred from archetype names.

## 14. Animation Integration

- Gameplay Ability tick phases are the authority contract; montages present those phases.
- Ability tasks control montage start, completion, interruption, and cancellation.
- Animation notifies/events identify authored windows and presentation events but do not independently authorize hits, damage, invulnerability, or resource changes.
- Server hit validation uses the authoritative simulation phase even if a client montage is ahead or behind.
- Predicted montages must reconcile with server rejection/cancellation through standard GAS behavior.
- Light and Heavy movement multipliers apply for their action duration; Block uses its blocking multiplier while active.
- Dodge should use standard Character Movement/root-motion support compatible with prediction. The displacement curve is not specified.
- Montage frame rate, section boundaries, and authored notify placement must be validated against the 60 Hz tick contract. Montage time and notifies may request or present transitions, but the authoritative action-local tick decides every gameplay boundary. Predicted/remote montage alignment uses the approved TD-02 action and clock anchors and accepts correction.

## 15. Multiplayer Requirements

- Use the server as the authority for action validity, state transitions, hit validation, defensive resolution, damage, HP, Stamina, cooldowns, crowd control, Echo placement, Echo effects, and death.
- Use `UCharacterMovementComponent` prediction/correction for locomotion and supported Dodge movement.
- Use GAS prediction keys and prediction policies for responsive abilities where safe.
- Replicate ASC abilities/effects/tags according to GAS and replicate approved AttributeSet values with RepNotify.
- Apply initial attributes only on the server after Actor Info initialization; owning and remote clients must obtain them only through replication.
- Use GameState server-time synchronization plus sparse TD-02 clock/action anchors for presentation; never replicate or RPC every combat tick.
- Treat clock/action replication as latest-state snapshots with epoch, instance, and revision ordering. Late join must reconstruct current state without replaying an earlier multicast.
- Replicate Echoes as standard Unreal Actors and use Gameplay Cues for cosmetics.
- Never accept client-local hit or final damage as authoritative.
- Test under artificial latency, jitter, and packet loss before claiming multiplayer readiness.
- Treat lag compensation/rewind as a separate technical design task after measurements. No custom UDP protocol is approved.
- Current Launcher UE 5.7.4 supports PIE/listen-server testing but cannot build the existing Dedicated Server target. Do not run that target.

Multiplayer verification requires a listen server plus at least one separate client instance. A single Editor process without multiple game instances is not proof of replication behavior.

## 16. Balance Principles

- Values in this specification are initial MVP values and must be implemented exactly before comparative tuning.
- They are not described as final, fair, or playtest-proven.
- Balance changes require recorded rationale, affected acceptance-test updates, and specification changes in the same task.
- No random critical damage in MVP.
- No pay-to-win modifiers, paid statistical advantages, or client-selectable authoritative values.
- Prefer readable windows and explicit counters over hidden exceptions.
- Shared archetype baseline values remain equal until an approved archetype specification changes them.
- Do not duplicate authoritative values across unrelated C++, Blueprint, Data Assets, and documentation. Runtime data location must be selected during implementation while this document remains the design contract.

## 17. Debugging and Telemetry

Development diagnostics should make server decisions reproducible without becoming gameplay dependencies.

Record or expose on demand:

- authoritative simulation tick and action phase;
- attacker, target, and `AttackInstanceId`;
- hit candidate source and rejection reason;
- defensive result: Invulnerable, Parry, Block, other mitigation, or normal;
- base damage, modifiers, rounding result, and applied damage;
- HP and Stamina before/after authoritative effects;
- state/tag transitions and cancellation reason;
- crowd-control window/count/budget decisions;
- Echo owner, lifecycle state, placement rejection reason, active count, and destruction reason;
- prediction rejection/correction and relevant network conditions.

Use scoped logging categories, Unreal Insights/trace events, Visual Logger, and debug draw where appropriate. Debug visualization is non-authoritative, disabled for Shipping, and must not expose hidden competitive information to normal clients.

## 18. Acceptance Criteria

Each row is a binary PASS/FAIL condition. Criteria blocked by an open technical decision cannot pass until that decision is approved and the criterion is made executable.

### Combat time

| ID | PASS condition |
| --- | --- |
| TIME-01 | Equal total gameplay `DeltaSeconds` supplied as `60 x 1/60`, `30 x 1/30`, `20 x 1/20`, or `120 x 1/120` produces the same authoritative tick count after backlog drains. |
| TIME-02 | One World Tick executes no more than eight combat steps; excess time remains accumulated and later steps are contiguous with no duplicate or skipped tick index. |
| TIME-03 | A hitch followed by recovery preserves every scheduled transition and half-open window boundary; no variable-size combat step is introduced. |
| TIME-04 | For an action with authoritative `StartTick`, local tick is exactly `CurrentTick - StartTick`, `[start, end)` includes `start`, excludes `end`, and a transition at tick `N` is visible to resolution at tick `N`. |
| TIME-05 | Paused/zero gameplay delta advances no combat tick, and render FPS or replication cadence cannot directly change the authoritative tick sequence. |
| TIME-06 | A long deterministic run using varied delta partitions produces the expected tick count and a normalized remainder without cumulative boundary drift. |

### Movement

| ID | PASS condition |
| --- | --- |
| MOV-01 | Authoritative locomotion configuration reports walk `520 cm/s`, sprint `660 cm/s`, acceleration `3000 cm/s^2`, and deceleration `3600 cm/s^2`. |
| MOV-02 | Character capsule reports radius `38 cm` and half-height `90 cm`. |
| MOV-03 | Server uses `FinalSpeed = ServerClamp(BaseSpeed × DirectionalMultiplier × CombatMultiplier)`; Attack and Block force the Walk base and ignore Sprint intent for current speed; all Normal, Attack, Block, forward/sideways, and backward cases match the Section 4 expected-speed table; held Sprint resumes on exit only when the resulting state permits it. |
| MOV-04 | Authoritative Dodge displacement never exceeds `400 cm`. |

### Attack timing

| ID | PASS condition |
| --- | --- |
| ATK-01 | Light produces no hit candidates outside `[12, 19)` and uses total `[0, 36)`. |
| ATK-02 | Heavy produces no hit candidates outside `[22, 31)` and uses total `[0, 60)`. |
| ATK-03 | Light rejects Dodge cancel before tick 27 and Block cancel before tick 29; both are accepted at their start tick when other requirements pass. |
| ATK-04 | Heavy rejects Dodge cancel before tick 50 and Block cancel before tick 54; both are accepted at their start tick when other requirements pass. |
| ATK-05 | Light and Heavy commit `0` Stamina cost. |

### Hit Detection

| ID | PASS condition |
| --- | --- |
| HIT-01 | Server rejects Light candidates beyond `240 cm` or outside the `45-degree` half-angle. |
| HIT-02 | Server rejects Heavy candidates beyond `300 cm` or outside the `35-degree` half-angle. |
| HIT-03 | One `AttackInstanceId` applies at most one hit to the same target despite repeated sweeps/hurtboxes. |
| HIT-04 | Client-only hit reports cannot apply HP, Stamina, control, score, or Echo changes. |
| HIT-05 | Two valid mutual same-tick hits both resolve from the same defensive-state snapshot. |
| HIT-06 | Same-tick candidate processing produces the same ordered result across repeated identical server runs. |

### Damage

| ID | PASS condition |
| --- | --- |
| DMG-01 | Unguarded Light applies exactly `115` damage and unguarded Heavy exactly `180`. |
| DMG-02 | Successful Block applies exactly `35` Light damage and `54` Heavy damage. |
| DMG-03 | Successful Parry applies `0` damage before the normal formula. |
| DMG-04 | Server uses `RoundHalfUp`; client-supplied final damage cannot alter the result. |
| DMG-05 | Critical chance is `0%` and repeated identical hits do not produce random critical results. |

### Block, Parry, and Dodge

| ID | PASS condition |
| --- | --- |
| DEF-01 | Block protection is unavailable in `[0, 3)` and available from tick 3 when direction and Stamina requirements pass. |
| DEF-02 | Block rejects attackers outside the authoritative 120-degree defensive sector. |
| DEF-03 | Parry succeeds only in `[2, 8)`, has total `[0, 26)`, and starts a 36-tick cooldown at activation. |
| DEF-04 | Successful Parry applies an 18-tick base Stagger to the attacker before diminishing returns. |
| DEF-05 | Dodge is invulnerable only in `[4, 13)`, has total `[0, 24)`, costs 28, and starts a 36-tick cooldown at activation. |
| DEF-06 | Resolution order is Invulnerability, Parry, Block, other mitigation, then normal damage. |
| DEF-07 | Stamina exhaustion while blocking applies GuardBreak for 36 ticks. |

### Stamina

| ID | PASS condition |
| --- | --- |
| STA-01 | Maximum Stamina is exactly `100`. |
| STA-02 | After spending, regeneration is zero for 45 ticks and resumes at the approved 18/sec rate when the delay ends. |
| STA-03 | Block upkeep consumes 12/sec without losing fractional accumulation. |
| STA-04 | A failed Parry consumes exactly 18; a server-confirmed successful Parry spends 18 then refunds 12 for net 6. |
| STA-05 | Dodge activation fails without 28 available Stamina and consumes exactly 28 on successful commit. |
| STA-06 | Block-hit Stamina cost is computed from 20% of incoming base damage using the approved resource rounding rule. |

### Attribute initialization

| ID | PASS condition |
| --- | --- |
| ATTR-01 | Validated startup content has exactly one Instant initialization effect with `Override` modifiers for MaxHealth, Health, MaxStamina, and Stamina, producing `1000/1000` and `100/100`; no runtime C++/Blueprint/Data Asset magnitude duplicates those values. |
| ATTR-02 | A new authoritative Character applies the initialization effect exactly once after valid Actor Info initialization and owns exactly one `UVelrathAttributeSet`. |
| ATTR-03 | Repossession/repeated Actor Info initialization of the same Character preserves changed current and maximum values and does not apply the initialization effect again. |
| ATTR-04 | Respawn/pawn replacement creates a different Character, ASC, and AttributeSet and runs one fresh initialization that converges on server, owning client, and remote client. |
| ATTR-05 | No client path applies the initialization effect or supplies initial magnitudes; an unauthorized client mutation cannot change server or remote values. |
| ATTR-06 | Missing or invalid startup data/effect fails validation and startup without fallback constants, partial success, or a completed initialization guard. |

### Resonance / Echo

| ID | PASS condition |
| --- | --- |
| ECHO-01 | Server rejects placement beyond `400 cm`, a second player-owned Echo, or a seventh total Echo in a 3v3 match. |
| ECHO-02 | Echo follows 18-tick Cast, 24-tick Formation, and 180-tick Active lifetime before expiration when not destroyed. |
| ECHO-03 | Expiration/destruction begins a 720-tick authoritative cooldown. |
| ECHO-04 | Owner death causes server removal after exactly 12 ticks. |
| ECHO-05 | Active Rift Field radius is `220 cm`, applies one non-stacking 25% Slow to enemies, affects no allies, and deals zero damage. |
| ECHO-06 | Valid Disrupt requires 30 uninterrupted ticks within 150 cm; authoritative damage or control interrupts it. |
| ECHO-07 | Remote clients observe the same authoritative Echo transform and lifecycle order without receiving per-frame authoritative timer replication. |

### State transitions

| ID | PASS condition |
| --- | --- |
| STATE-01 | Every approved action enters and removes its canonical gameplay tag exactly once per activation. |
| STATE-02 | Normal cancels are rejected outside authored cancel windows. |
| STATE-03 | Dead cancels active combat abilities and prevents new living actions. |
| STATE-04 | Respawn creates a new Character/ASC combat state and grants startup abilities exactly once. |
| STATE-05 | Authoritative Stagger and GuardBreak interrupt only abilities configured for their control tags. |

### Multiplayer and automation

| ID | PASS condition |
| --- | --- |
| NET-01 | In listen-server PIE with at least one separate client, server, owner, and remote observer converge on HP, Stamina, state tags, and Echo lifecycle. |
| NET-02 | A client cannot cause damage, resource, cooldown, control, or Echo changes without server validation. |
| NET-03 | Character Movement and predicted GAS actions correct rejected client prediction without leaving stale tags/effects. |
| NET-04 | Tests run under recorded latency/jitter/packet-loss conditions before multiplayer readiness is claimed. |
| TEST-01 | Editor target builds successfully after combat C++ changes. |
| TEST-02 | Relevant automation filter executes at least one test and reports `failed = 0` and `notRun = 0`. |
| TEST-03 | Missing, invalid, stale, or zero-test Automation reports fail the test command. |
| TEST-04 | No Dedicated Server build is required or attempted with the current Launcher engine. |

## 19. Open Technical Questions

The following items are unresolved. Implementations must not silently choose gameplay behavior for them. TD-01, TD-02, TD-03, and TD-22 are approved in their owning sections/documents, so their original item numbers are omitted:

4. What hurtbox topology and server sweep shape/sampling method are used for Light and Heavy attacks?
5. Which stable replicated identifiers define deterministic same-tick hit ordering?
6. When a Block hit requires more Stamina than remains, does that hit retain Block reduction or resolve as normal damage before GuardBreak?
7. Which rounding rule applies to block-hit Stamina costs?
8. Which rounding rule converts a 25% diminished 18-tick Stagger (`4.5` ticks) to the required integer duration?
9. Which control categories count toward the 42-tick rolling control-loss cap, and how are overlapping controls budgeted?
10. Does Parry use a directional sector, and if so, what sector?
11. Is Block-to-Parry transition allowed, and what is its cancel window?
12. What are AbilityCast and EchoCast interruption rules?
13. What is the exact Dodge displacement/root-motion curve within the 400 cm maximum?
14. Which Echo surfaces, slope, vertical offset, line-of-sight, and navigation constraints are valid?
15. Which players may Disrupt an Echo, and what authoritative state does successful Disrupt apply beyond destruction?
16. Must the 720-tick Echo cooldown survive owner death/respawn? If yes, where is it stored while ASC remains Character-owned?
17. Which Echo lifecycle states and transition anchors must its replicated snapshot expose through the approved TD-02 mapping?
18. What are the approved durations/strengths for Flinch, generic Stagger, Slow outside Rift Field, and Knockback?
19. What archetype-specific Gameplay Abilities and values are approved for Vanguard, Reaver, and Weaver?
20. What measured latency threshold and attack types justify a separate lag-compensation/rewind design?
21. How is the Respawn state represented across destruction of the old Character and initialization of the new Character?
