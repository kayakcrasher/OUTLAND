# Careers: the gangs and the police

In Explore, a player can join one of La Mafio's gangs or the Policio de Verda. Each side gives work, pays for it, and keeps score in **respect**. Logic: `game::career::Career`. Wiring: `Renderer.cpp`.

## Joining

- **A gang:**
  - Walk to a crew's hideout door (any town) and press **USE**. Press it again within 5 s to confirm.
  - The turf panel says "USE at the door to join them".
  - You can't join while that gang is shooting at you.
- **The police:**
  - Walk up to a police station (where its patrol car parks) and press **USE** twice.
  - You are sworn in as a Kadeto and issued a pistol and two magazines, then sent to the Chief (the questline below).
  - That station's patrol car becomes yours until the Interceptor replaces it. It leaves the AI dispatch and nobody reports it stolen.
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
- Smuggling, extortion and taking turf, and the fight for the island: [gang wars](gangs.md).

## The police questline

Every recruit, at any station, starts a Kadeto **in training**. There are no calls until they graduate. The panel shows ORDERS and a light column marks the next place to go.

1. **Report to the Chief.**
   - **Ĉefo Marko Forta** heads the Policio de Verda; the Verda City HQ outranks every other police department.
   - He stands beside the capital's first patrol car in a navy uniform, peaked cap and gold braid. His name and "USE to talk" show over his head.
   - Press **USE** next to him and he sends you to the training camp. Nobody can shoot him.
2. **Qualify at the training camp.**
   - The camp is a fenced range on open, level ground 300–1000 m outside Verda City, clear of roads and buildings. Its downrange points away from the city.
   - It has a gravel apron, a firing line with booth dividers and a lectern, distance posts at 10, 15 and 25 m, an earth berm, a range hut and a blue-and-white flag.
   - Stand at the lectern and press **USE** to start the next drill. A 3 s countdown runs, then silhouette targets flip up one at a time and drop when hit or when their time runs out.
   - Leaving the firing line stops the drill. Real bullets hit the targets (`CombatWorld::bind_drill`).

   | Drill | Targets | Up for | To qualify |
   |---|---|---|---|
   | PISTOL – 10 m | 10 | 2.6 s | 7 hits |
   | RIFLE – 25 m | 10 | 2.2 s | 7 hits |
   | MOVERS – 15 m | 8, sliding across the lanes | 3.4 s | 5 hits |

   - The camp panel (right) lists the drills with your best score. During a drill it shows target, hits, shots and accuracy.
   - Qualify all three and you **graduate**. Officers can come back to practise.
3. **Report back.**
   - The Chief promotes you to **Patrolano** (respect 15).
   - He hands over the **Interceptor**: a long, low pursuit sedan in black with white doors and roof and a gold pinstripe. It has a slim LED light bar, a push bar with flashers, a bonnet scoop and a boot spoiler.
   - It is quicker than a patrol car (top speed 60, 220 health) and becomes your patrol car. Then the calls begin.

- **Emergency lights:** at the wheel of any police car or the Interceptor, **L** (or the **LIGHTS** touch button above HAND) switches the light bar, grille flashers and siren on or off.
- **Older saves:** officers from before the questline are already serving.
- **Dev:** `OUTLAND_DEV_CAREER=police:training`, `police:graduated` or `police:serving` starts further along. Dev builds log where the camp, the Chief and the Interceptor are.

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
- `outland_training_camp_tests`: a drill starts from the line, targets take real traces and head shots, leaving the line stops it, retries, movers and graduation.
- `outland_career_tests` covers:
  - joining, and the record shutting the police door;
  - hits, drive-bys only counting from a car, and robbing a rig;
  - a lapsed job costing respect, and killing your own;
  - the questline (orders, the Chief, the camp, the Interceptor);
  - patrols needing the patrol car, raids, gang-war calls;
  - going bad, ranks, and save/load.
