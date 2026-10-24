# Callouts and prison

Crimes in progress for the Policio to answer, an easier arrest, and what happens to the player who gets busted.

## Callouts (police)

A serving officer gets **ROBBERY** and **DRIVE-BY** calls, about two times in three instead of the next job.

- **The call:** the radio gives the place (a shop or pub for a robbery, a house for a drive-by), the town, the gang and its shirts, and the getaway car: colour, model and plate ("2 Feraj Hundoj (rust-red shirts), a racing red pickup, plate VRD-472K").
- **The scene:** two of the nearest crew wait by the car (never the boss, never Verda City's casino crew). The compass and the job marker follow them. The car carries a red **SUSPECT VEHICLE · plate** label, and the men **SUSPECT**.
- **The chase:** when an officer comes within 110 m they get in the car and drive for their hideout.
  - Stay on their bumper (16 m) in a police car **with the lights on** (L) for 6 s and they pull over.
  - A wrecked or stuck car stops them too.
  - If they make it home the call is lost. If nobody comes in 7 minutes they slip away.
- **The stop:** out of the car, each man decides when an officer is within 20 m.
  - **Hands up** (most of them; nearly all after a pull-over): he stands with his hands up. USE within 3.4 m cuffs him.
  - **Runs:** he heads away on foot. Catch him (3.5 m) or wound him (under 70 health) and he gives up.
  - Shoot at them and the gang shoots back, as always.
- **Bringing them in:** cuffed, he walks behind you. Get in a police car with him close by and he climbs into the back. The call is done when both are in the car, or down. The prison's gate books them in.
- **The first call** comes with a partner, **Officer Novak**, who appears at your side and talks you through it: the shirts, the plate, the lights, hands up and USE, walk him to the car, the prison. His tips follow what's happening.
- **Pay:** 650 kronoj for a robbery, 600 for a drive-by, +10 respect (more pay with rank), then 120 kronoj and +2 respect a prisoner at the gate.

## The warden's tour

An officer's first drop-off begins with a tour. Within 200 m of the gate with prisoners in the back, **Warden Brana** shows you round. The camera glides from one view to the next: the compound, the gate, intake, and the yard and cell block. Her five lines cover:

- how drop-offs work (drive up to the gate and stop);
- the pay;
- the cells and the SHU.

USE skips a line. After the tour the guards take the prisoners.

## Busted (the player)

You're arrested at one star when an officer reaches you and you're calm. You're also arrested at any level when you're on foot with **no gun in hand** and an officer is within 9 m (cycle weapons to UNARMED to give up).

1. **The ride:** "BUSTED", then black: "In the back of a Policio car - to Ŝtata Malliberejo".
2. **Booking:** a card with your **mugshot** in front of the height chart, holding a board with your **prison number**. The number is yours for good and is saved with the career. The card also shows your affiliation, the charge, the sentence and your notoriety. USE takes you to your cell, and the sentence starts.
3. **Inside:** your cell is on the cell block's back row. The first time, a guard tells you how it works.
   - **Sleep it off:** USE at the bunk. The screen goes dark and you wake up at the gate, released.
   - **Do the time awake:** walk the block and the yard until your number's up. Guns stay holstered inside. Leave the walls and you're put back.
4. **The snitch (gang members):** a cellmate passes on the gang's word. A law breaker in the yard talked to the Policio, and he's marked **SNITCH** (and on the compass).
   - You have a shank: USE beside him (he stops when you come close).
   - **Put in work:** +8 respect with the gang. It also adds a record and some notoriety.
   - Then the guards drag you to the **SHU**: a solitary cell behind a steel door, with +30 s on the sentence. There's nothing to do there but lie down on the bunk and sleep out your time.
5. **Out:** as before, Uncle Ron may be waiting at the gate the first time.

## The prison's cell block

`tools/generate_city_kit.py` `state_prison()` (the `PRISON_*` constants, mirrored in `world::layout::prison_cell_*` and `prison_shu_*`) has a ground floor you can walk in:

- the yard door;
- a day room with steel tables and the guard's desk (a guard stands there);
- twelve cells along the back wall, each with an open barred front, a bunk, a toilet and a shelf;
- walled off at the east end, the SHU: a red band, and two solitary cells behind shut steel doors.

## A week inside (NPCs)

- Anyone the Policio books in does **one game week** (7 island days) in Ŝtata Malliberejo. That covers a hit-and-run driver pulled over by the police and a Krimulo an officer brings to the gate. They stand in the yard with the other inmates, and a resident is off the street while inside.
- Within the first two days, inmates who aren't in a gang face a choice. Most (60%) **join one of the five gangs** for protection; they change into its colours in the yard.
- The rest get **shanked** in the yard: taken to the hospital, treated, and let out from there (not back to prison).
- Gangsters booked in are with their gang already. When the week is up, everyone left walks out of the prison gate back into their life.
- The news reports a named resident's choices: "PRISON: … joined … inside", "… shanked in the yard", "… walked out of the gate".
- Terms aren't saved: a new session starts with the yard's usual inmates.
- Code: `game::law::JailTerms`, `LifeSimulation::imprison` / `set_free`, `PrisonYard::recruit`.

## Gang shirts

Every gang wears its colours so officers can tell them apart; see [gangs](gangs.md).

## Code

- `game::mafia::Mafio`: `stage_heist`, `heists()`, `HeistStage` (Waiting, Fleeing, Stopped, Over), `surrendered`, `hold`/`held` (hands up or cuffed: asleep where the game puts him), `settle`.
- `game::career::Callouts`:
  - `dispatch` picks the scene, the crew and the car, and puts out the call;
  - `update` follows the heist, moves the cuffed, boards them and counts the suspects;
  - `to_cuff` / `cuff`;
  - the partner's lesson.
- `game::career::Career`:
  - `JobKind::Callout`, `callout`, `due_call`, `suspect_down`, `callout_failed`, `track`;
  - prison: `prison_toured`, `inmate_number`, `inside_told`, `shanked_snitch`;
  - career save version 6.
- `game::law::Justice`: `JusticeFrame::player_armed` / `player_driving` and `JusticeConfig::surrender_radius`.
- `game::law::PrisonYard`: `mark_snitch`, `pause`, `remove`, and the guard at the block's desk.
- `characters::BodyPose::hands_up` / `cuffed`.
- `ai::BotDrivers::halt` (a pull-over).
- `game::vehicles::VehicleSystem::remove` (the getaway car is towed once out of sight).
- Renderer: the booking state (the fade, the mugshot render texture, the card), the cell and SHU, the bunk and the shank, the tour camera, and the labels.

## Dev and tests

- `OUTLAND_DEV_CAREER=police:serving OUTLAND_DEV_HEIST=1` (dev builds) skips the chase. The officer lands at the scene, the getaway car is gone, and you're put next to a suspect with his hands up. Once he's cuffed you're put in the car, taken to the prison road for the tour, then to the gate.
- `OUTLAND_DEV_JAIL=1` busts you a few seconds in. Add `OUTLAND_DEV_SHANK=1` to be put beside the snitch once the guard's done talking.
- `tests/integration/law/test_callouts.cpp` covers:
  - the call (two suspects, the plate on the radio, the partner);
  - waiting, the chase and the pull-over;
  - hands up or a run, then caught;
  - cuffing, walking with the officer, boarding, the call done, booking;
  - the snitch;
  - the new save fields.
- `tests/integration/law/test_justice.cpp`: wanted for murder, unarmed, busted.
