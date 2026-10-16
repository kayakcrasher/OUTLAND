# Careers: the gangs and the police

In Explore, a player can join one of La Mafio's gangs or the Policio de Verda. Each side gives work, pays for it, and keeps score in **respect**. Logic: `game::career::Career`. Wiring: `Renderer.cpp`.

## Joining

- **A gang:**
  - Walk to a crew's hideout door (any town) and press **USE**. Press it again within 5 s to confirm.
  - The turf panel says "USE at the door to join them".
  - You can't join while that gang is shooting at you.
- **The police:**
  - Walk up to a police station (where its patrol car parks) and press **USE** twice.
  - You are sworn in as a Kadeto and issued a pistol and two magazines.
  - That station's patrol car becomes yours. It leaves the AI dispatch, nobody reports it stolen, and its light bar runs while you drive it on a call.
- **The one-way door:**
  - Joining a gang gives you a **record**, for good. The Policio turns away anyone with a record ("no Krimuloj on the force").
  - An officer can always go bad: joining a gang ends the career on the force.
  - So does a crime on duty: assaulting or killing a resident, or attacking an officer. Either one leaves a record.
- Joining a different gang starts your respect again from zero.

## Respect and ranks

Respect runs from 0 to 100. The meter sits under the money line, with ticks at each rank. Each rank pays 25% more per job.

| Respect | Gang | Police |
|---|---|---|
| 0 | Novulo | Kadeto |
| 15 | Soldato | Patrolano |
| 40 | Kapo | Serĝento |
| 70 | Leŭtenanto | Leŭtenanto |
| 95 | Estro | Komisaro |

- **Gaining respect:** you earn it by finishing jobs. Taking a rival (or, for an officer, any Krimulo) off the street outside a job also adds a little.
- **Losing respect:**
  - A job that runs out of time costs half its respect, and so does dying on a job.
  - Killing one of your own costs 15 respect. Below Soldato it gets you thrown out.

## Gang jobs ("the word")

A job comes a few seconds after joining, then 12 s after each one ends. A column of light marks the target, and the panel shows the job, progress, time left and distance.

| Job | What | Pay | Respect |
|---|---|---|---|
| **HIT** | Take out 2–3 of a rival crew (one of the closest) | 320 | 6 |
| **DRIVE-BY** | Hit 2 of a rival crew **from a car** | 450 | 9 |
| **ROB THE RIG** | Hijack the supply or cattle rig (`game::freight`) and drive it to your crew's hideout | 900 | 12 |

- **On rival turf:** within 45 m of a rival hideout, a member gets a warning ("they know your colours"). Stay 5 s more and that gang opens fire.
- **Your own crew:** they don't mind your gunfire near them. Only a real hit turns them.

## Police calls

| Call | What | Pay | Respect |
|---|---|---|---|
| **PATROL** | Drive your patrol car to three towns in turn | 260 | 5 |
| **RAID** | Take down 2–3 of a gang crew at its hideout | 480 | 9 |
| **GANG WAR** | When a hit squad is on the road: stop 2 of either side | 500 | 10 |

- Firing your gun on duty is never reported as "shots fired".

## Drive-bys

- Anyone at the wheel with a **pistol or SMG** can shoot out of the driver's window: look to aim, LMB to fire, or the left **FIRE** button on touch screens.
- The shot leaves from outside the car's body, so it never hits your own car.
- A line over the speedometer shows the gun and its ammo.

## Saving

The career is saved beside the Explore loot profile (`<profile>.career`): side, gang, respect, record, jobs done and the patrol car. A job in progress is not saved; a new one comes after loading.

## Dev and tests

- `OUTLAND_DEV_CAREER=gang:N` or `police` starts the session in a gang or on the force.
- Dev builds log every crew door and police station.
- `outland_career_tests` covers:
  - joining, and the record shutting the police door;
  - hits, drive-bys only counting from a car, and robbing a rig;
  - a lapsed job costing respect, and killing your own;
  - patrols needing the patrol car, raids, gang-war calls;
  - going bad, ranks, and save/load.
