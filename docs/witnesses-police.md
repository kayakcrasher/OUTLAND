# Witnesses and police response

In Explore, crimes now have consequences. Residents who see you commit a crime call the police. Officers come to where it happened, look for you and either arrest you or fight you, depending on how bad it was. If you stay out of sight long enough, they give up.

Only modes with civilian life (`rules_for(mode).civilian_life`, so Explore) run the justice system. Battle Royale, Zombie Survival and the Dev Lab are unchanged.

## Crimes and stars

| Crime | Stars | Triggered by |
|---|---|---|
| Shooting | 1 | Firing a gun where someone sees or hears it |
| Assault | 2 | Hurting a resident |
| Murder | 3 | Killing a resident |
| Attack on an officer | 4 | Hurting a police officer |

The **WANTED** stars show under the clock, and messages such as "A witness called the police: murder" appear for 4 seconds.

## Witnesses

When a crime happens, every living resident is checked.

**Seeing:**
- A **physical** resident (a body near you) sees you if they are within sight range and have a clear line of sight to you. The line of sight check is a `combat_world` trace from eye to eye.
- An **abstract** resident (off-screen, simulated) sees you only if they are outdoors and within half the sight range.
- A living victim of an assault always saw it.

**Sight range depends on daylight:** 55 m by day, falling to 22 m at night. Crimes in the dark are seen by fewer people.

**Hearing:** gunshots carry 150 m. If nobody saw the shot, the single nearest outdoor resident who heard it reports "shots fired". They don't know who fired, so this raises no stars.

**Calling the police:**
- Civilians call 4–14 seconds after the crime.
- Officers report immediately.
- A witness who dies before calling never reports, so killing witnesses works, but each killing seen by someone else is a new murder.
- Each witness makes one call, about the worst thing they saw.

## Police response

Officers are residents with the `PoliceOfficer` occupation. They have homes and shifts like everyone else.

**Dispatch:**
- A reported crime sends the nearest free officers: 1 plus the wanted level (so 4 officers at 3 stars).
- Off-screen officers travel at 14 m/s (as if driving) toward the scene and appear as bodies when they get close.
- Officers on the street who see you while you're wanted join the response.

**Search:**
- Whenever an officer sees you, your last known position is updated, and the responders are redirected there (at most every 2 seconds).

**Arms:**
- At 1 star, officers are unarmed and come to arrest you.
- At 2 stars and up, responders are armed:
  - they see 60 m (with line of sight) and open fire within 28 m;
  - they fire every 0.8–1.4 s for 18 damage;
  - their hit chance falls with distance, from 75% to 12%.
- Their shots are on the sound bus as pistol shots and draw tracers.
- Officers never flee. When shot, armed officers chase and unarmed officers stand their ground.

**Ending it:**
- **Arrest** (1 star only): an officer is within 2.5 m of you for 2 seconds while you haven't fired for 3 seconds. You're let off with a warning and the stars clear.
- **Escape**: stay unseen for 25 s + 12 s per star (37 s at 1 star, 73 s at 4 stars). The message is "You lost the police".
- **Investigation**: an anonymous "shots fired" report sends one officer to look around for 60 s.
- **Death**: if you die, the police stand down.

When a response ends, officers are disarmed and go back to their normal day.

## Code

- `include/outland/game/law/Justice.hpp`, `src/game/law/Justice.cpp`:
  - crimes, witnesses, calls, dispatch, arrest and escape;
  - all numbers are in `JusticeConfig`.
- `LifeSimulation::dispatch / release`:
  - a responding resident has `responding` and `response_target` set;
  - while responding, it travels toward the target instead of following its schedule;
  - it is never indoors, and its physical body is anchored at the target.
- `NpcSystem`:
  - `ResidentSpawn::police` and `NpcInstance::police / armed`;
  - `arm_resident(id, armed)`;
  - shots fired by NPCs are reported in `NpcEvents::shots`.
- `NpcBehavior.cpp`: the armed-officer branch (sight, chase via the navigation graph, fire) and the officers' no-flee rules.
- `Renderer.cpp`:
  - queues assault, murder and attack on an officer from the damage callback;
  - reports shots from the player's weapon;
  - builds the `JusticeFrame` (line of sight, daylight);
  - plays police gunfire and draws the HUD stars.

## Tests

`tests/integration/law/test_justice.cpp` (`outland_justice_tests`) covers:
- crime severities;
- a witnessed assault reported only after the call delay, then the responders closing in;
- escaping after time out of sight;
- killing witnesses before they call;
- an anonymous shots-fired investigation that ends;
- a night witness at 20 m who sees by day but not at night;
- a 1-star arrest by a physical officer;
- armed officers firing only with line of sight;
- officers holding their ground where civilians flee.

## Not yet

- Officers walk or teleport-drive. They don't use real police cars until residents use cars.
- No fines, jail or money penalty. The arrest is a warning until money and save state exist.
- No disguise or clothing changes. A witness's description is only your position.
