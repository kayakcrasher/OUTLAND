# Careers: the gangs and the police

In Explore, a player can join one of La Mafio's gangs or the Policio de Verda. Each side gives work, pays for it, and keeps score in **respect**. Logic: `game::career::Career`. Wiring: `Renderer.cpp`.

## Joining

- **A gang:**
  - Talk to the crew's **boss**: the man standing by the hideout door (any town). Press **USE** beside him, and again within 5 s to confirm.
  - The turf panel says "talk to their boss at the door to join".
  - You can't join while that gang is shooting at you, nor while you wear the badge ("hand it in at a station first").
- **The police:**
  - Walk up to a police station's front door and press **USE** twice (USE near its parked patrol car just gets you in the car).
  - An officer hands the badge back the same way: USE twice at any station's front door.
  - You are sworn in as a Kadeto and issued a pistol and two magazines, then sent to the Chief (the questline below).
  - That station's patrol car becomes yours until the Interceptor replaces it. It leaves the AI dispatch and nobody reports it stolen.
- **Honour and the record:**
  - A gang that kills other gangsters (the crews and the ranching families) and defends its towns is **HONOURABLE** (shown by the respect meter).
  - Gang members *can* hurt and kill civilians and cops, and the gang won't call you dishonoured for it. But every reported crime (assault, murder, attacking an officer) goes on your **record** for good, and your **NOTORIETY** (0–100) rises: +8 for a serious crime, +3 for a minor one, +30 for the Chief. Notoriety shows on the respect meter in place of HONOURABLE.
  - The more notorious you are, the longer you do in prison (+0.6 s a point).
  - An honourable gang's towns win back their hold twice as fast. Every rival gangster you take down on your own gang's turf counts as defending the town: +2 respect and +4 hold.
  - **Leaving:** tell your boss you're out (USE twice beside him) to walk away from the gang.
  - The Policio turns away anyone with a record ("no criminals on the force"), and anyone still running with a gang.
- **No gang takes a cop:** an officer hands in the badge first. A crime on duty (assaulting or killing a resident, or attacking an officer) ends the career on the force and leaves a record.
- Joining a different gang starts your respect again from zero.
- **Trucking** is open to anyone but a serving officer, gang members included: sign on with the boss at Verda Transporto's depot and drive loads for pay by the trip. See [trucking](trucking.md).

## Ŝtata Malliberejo, the state prison

Verda's only prison stands alone south-east of the ring road, about a kilometre from the nearest town, on its own level ground at the end of an access road. Behind a 6 m wall with razor wire are four guard towers, a gatehouse, the intake bay, the admin block, a two-storey cell block and an exercise yard. The map always shows it.

- **Officers** bring their prisoners here (above).
- **Getting arrested:** at one star an unarmed officer who reaches you, with you not firing, makes an arrest. You do time in the prison yard: 60 s, plus 30 if you have a record, and more the more notorious a gang member is. The job in hand is off and no work comes while you're inside. Walk out of the gate early and you're put back. When your time is up you're let out at the gate.
- **The gang life:** a gang member will do a few stints here. Doing your time quietly earns +3 respect with the crew.
- **Its people:**
  - Armed guards in navy stand in all four towers and at the gate, and two more walk the yard.
  - The yard holds the island's law breakers and the gangsters already doing time, about a dozen to start with.
  - Every prisoner an officer books in joins them, up to sixteen inmates.
  - `game::law::PrisonYard`: the guards and inmates are NPC bodies; nobody there can be hurt.

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
| **ROB THE RIG** | Hijack the supply or cattle rig (`game::freight`) and drive it to your crew's hideout (15 min) | 900 | 12 |

- **Jobs that can't be finished are never offered or left hanging:**
  - TAKE TURF never targets a town the Policio has cleared, and gives 30 minutes.
  - ROB THE RIG (15 minutes) and SMUGGLE are only offered while your gang has a hideout standing to deliver to. If the last one is broken up mid-job, the job is called off.
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
   - The camp is a fenced range on open, level ground right behind Verda City's police HQ, clear of the highway and the streets. Its downrange points away from the station. (On a map without room there, it sits on open ground 300–1000 m outside the city.)
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

- **The uniform:** on the force you wear the Policio's navy uniform (the clothes you bought wait in the wardrobe for when you hand in the badge).
- **Emergency lights:** at the wheel of any police car or the Interceptor, **L** (or the **LIGHTS** touch button above HAND) switches the light bar, grille flashers and siren on or off.
- **Older saves:** officers from before the questline are already serving.
- **Dev:** `OUTLAND_DEV_CAREER=police:training`, `police:graduated` or `police:serving` starts further along. Dev builds log where the camp, the Chief and the Interceptor are.

## Police calls

| Call | What | Pay | Respect |
|---|---|---|---|
| **PATROL** | Drive a police car to three towns in turn (15 min) | 260 | 5 |
| **RAID** | Take down 2–3 of a gang crew at its hideout | 480 | 9 |
| **GANG WAR** | When a hit squad is on the road: stop 2 of either side | 500 | 10 |

- **Patrols** count in any police car: your own, a station's, the Interceptor or a cruiser. A town counts within 40 m of its centre (towns only, not the ranches).
- **Gang war calls** don't go stale: once the shooting stops, the call points at the nearest crew of either gang, and it ends if there is nobody left to stop.
- Firing your gun on duty is never reported as "shots fired".

### Arrests

- Walk up to a Krimulo and press **USE** (E) to cuff him: no body, and he counts like a takedown on a RAID or GANG WAR call (and towards a CLEAN UP, since he's off the street). +1 respect.
- He goes in the back of your car: **IN CUSTODY**, up to four at a time. Drive them in a police car to the gate of **Ŝtata Malliberejo**, the state prison (the compass points the way while you have prisoners), to book them in: 120 kronoj each (more with rank) and +2 respect each. Die and they get away.
- He comes quietly only when he's **wounded** (60 health or less) or **alone** (none of his crew within 15 m). In front of his crew he won't: "his crew is watching". Cuff a wounded man in front of them and they open fire.
- The capital's crew before the Chief allows it costs respect, the same as shooting them.

### Backup

- Serving officers can radio for backup: **Z**, or the **BACKUP** button on touch screens.
- Two armed officers set off from the nearest station in a squad car with lights and siren, drive to you by road and get out at your side. If the car is stuck for 8 s, or you are too close to the station for a drive, they come running from round the corner.
- They follow you and fight any gang at war with you. They leave a gang at peace alone, so calling them never starts a fight.
- They stay 150 s from arrival, and longer while there is shooting near you. Then they head back to the station.
- If they fall, or once they leave, dispatch needs 40 s before it can send the next squad. The squad car stays where they left it, for the next call.
- Tech: `game::career::Backup`. The officers are `ai::BattleRoyaleBots` bots; the Mafio's outsider rule decides whom they fight.

## La Ŝosea Patrolo (the highway patrol)

The law on Verda's highways, and above all the giant ring road round the island.

- **The look:** troopers wear tan shirts with olive trousers and a tan campaign hat. Their cruisers are tan with an orange waist stripe and a red and blue light bar, and carry the orange star and "ŜOSEA PATROLO" on the doors. Each outpost is one small modular building from the city kit (`civic/patrol_outpost.glb`, an 8 x 6 m tan block with a door, a counter, an orange fascia and the star sign on the roof) with a carport on its side for the cruiser. They stand on open ground, well clear of the ranches and the stockyard.
- **Outposts:** at first there is just one, beside the north highway half a kilometre out of Verda City.
  - The patrol grows with the police campaign: one more outpost for every quarter of the towns cleared, all **five** once Verda is clean. The others stand round the ring road.
  - Each outpost has a cruiser and two troopers.
  - The map shows every outpost (tan shield); the compass shows them within 1.4 km.
- **On the beat:** each cruiser drives round and round the ring road.
- **Backup:** a cruiser that spots a wanted player close by joins the chase and radios the Policio for backup (+1 star, at most every 30 s).
- **Guarding the ranchers:** when a ranching family is in trouble (the player provoked it), the nearest cruiser rides out with its lights on, and the law is told (at least 1 star).
- **At the outposts:** the two troopers stand their post, and draw on a player with two stars or more within 70 m.
- **Tech:** placement, the beat and the art are in `game::services::HighwayPatrol`; the cars and troopers are driven by `EmergencyServices` (`UnitKind::Highway`). The cruiser comes from `tools/generate_emergency_vehicles.py` (`highway_patrol`) and the hat from `tools/generate_ranch_models.py` (`trooper_hat.glb`). Dev: `OUTLAND_DEV_PATROL=5` opens every outpost.

## The campaign: rid Verda of the gangs

The goal of the police questline is to get rid of the gangs. Once you serve, about 4 calls in 10 are the Chief's own orders: **CLEAN UP** (1400 pay, 14 respect).

- **What it takes:** go to a crew's hideout and take down every one of them (arrests count). The crew is broken up for good.
  - While the order stands the crew is **under siege**: their dead stay down (no respawns), and their hit squads are called home.
  - Going down yourself doesn't cancel the order. Come back from the hospital and finish it.
  - Nobody comes back to it, and it sends no more hit squads.
  - The town becomes nobody's turf (blue on the gang map, a blue cross on the map).
  - It stays that way across saves.
- **The towns first:** Ĉefo Marko Forta won't have you start in Verda City.
  - His orders take you to the other towns, the nearest first.
  - Raids skip Verda City's crew too.
  - Shooting the Green Hand's capital crew before he gives the word costs 3 respect and earns a warning.
  - Ask him (USE) and he tells you how many towns the gangs still hold.
- **Then Verda City:** once every other town is clean, the last order is the capital (4000 pay, 25 respect).
- **The end:** when the last crew is gone, **VERDA IS CLEAN** and he makes you Komisaro.
- This is the law's side of Verda's yin and yang (see [gang wars](gangs.md#yin-and-yang)): every town you clean up makes the island safer. On the other side, a gang Leŭtenanto whose gang runs at least 75% of the island may be ordered to kill the Chief (THE BIG HIT). If that happens, the Policio is left without a head and takes no more recruits.

## Your crew

- **Gang members** recruit up to four of their gang's own Krimuloj. Walk up to one (not the boss,
  and not the men at the casino's poker table) and USE: "Recruit him (crew 1 of 4)". He wears
  the gang's colours.
- **Officers** get a partner at any police station's front desk. On duty with no partner, USE at
  the desk: "Ask the desk for a partner". USE there again to hand in the badge, as before.

Your crew and partner:

- Follow you on foot. If they fall far behind, they catch up out of sight.
- Fight beside you. Gang members fight any gang that has turned on you; your partner fights any
  gang at war with you, as backup does.
- Ride along when you drive off. Those within about 35 m get in and get out where you do.
- **Drive you** if you USE at a car's passenger door (the right-hand side) while one of them is
  near. The nearest takes the wheel and drives you by road to your map waypoint, or else to the
  job in hand. The others ride too. USE again in the passenger seat to pull over and get out. A
  goal less than 70 m away is walked.

A fallen recruit goes back to his hideout. Recruit another, or ask the desk for a new partner.
Leaving the gang or handing in the badge sends the crew home, and so does dying. The crew is not
saved: recruit again after loading a game.

## Drive-bys

- Anyone at the wheel with a **pistol or SMG** can shoot out of the driver's window: look to aim, LMB to fire, or the left **FIRE** button on touch screens.
- The shot leaves from outside the car's body, so it never hits your own car.
- A line over the speedometer shows the gun and its ammo.

## Saving

The career is saved beside the Explore loot profile (`<profile>.career`): side, gang, respect, record, honour, notoriety, towns defended, jobs done and the patrol car. The towns the Policio has cleared are saved with the territory. A job in progress is not saved; a new one comes after loading.

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
