# Careers: the gangs, the police and the outlaw truckers

In Explore, a player can join one of La Mafio's gangs or the Policio de Verda. Each side gives work, pays for it, and keeps score in **respect**. Logic: `game::career::Career`. Wiring: `Renderer.cpp`.

**Leaving from the menu:** any time in Explore (out of prison), the pause menu (ESC / MENU) has a red button at the bottom: **LEAVE THE GANG**, **HAND IN YOUR BADGE** or **LEAVE THE BROTHERHOOD**. Tap it twice ("TAP AGAIN TO ...") to leave. You don't have to go back to the boss, the desk or Ivo.

## Joining

- **A gang:**
  - Talk to the crew's **boss**: the man standing by the hideout door (any town). Press **USE** beside him and a menu opens: **Join** or **Talk**. Nothing happens until you pick a row.
  - The turf panel says "talk to their boss at the door to join".
  - You can't join while that gang is shooting at you, nor while you wear the badge ("hand it in at a station first").
- **The police:**
  - Walk into any police station (the doors are level with the street now) and press **USE** at the front door or the desk. A menu opens: **Join the Policio**. If you can't join, the row says why (a record, a gang, the brotherhood). USE near its parked patrol car just gets you in the car.
  - An officer hands the badge back at any station's desk: **Hand in your badge**, then **Yes** on the confirm screen.
  - You are sworn in as a Kadeto and issued a pistol and two magazines, then sent to the Chief (the questline below).
  - That station's patrol car becomes yours until the Interceptor replaces it. It leaves the AI dispatch and nobody reports it stolen.
- **Honour and the record:**
  - A gang that kills other gangsters (the crews and the ranching families) and defends its towns is **HONOURABLE** (shown by the respect meter).
  - Gang members *can* hurt and kill civilians and cops, and the gang won't call you dishonoured for it. But every reported crime (assault, murder, attacking an officer) goes on your **record** for good, and your **NOTORIETY** (0–100) rises: +8 for a serious crime, +3 for a minor one, +30 for the Chief. Notoriety shows on the respect meter in place of HONOURABLE.
  - The more notorious you are, the longer you do in prison (+0.6 s a point).
  - An honourable gang's towns win back their hold twice as fast. Every rival gangster you take down on your own gang's turf counts as defending the town: +2 respect and +4 hold.
  - **Talking to your boss is safe.** USE beside him opens **Talk** and **Leave the gang**. Leaving asks again on a confirm screen (**Yes, I'm out** / **No**), so you can't quit by accident.
  - The Policio turns away anyone with a record ("no criminals on the force"), and anyone still running with a gang.
- **No gang takes a cop:** an officer hands in the badge first. A crime on duty (assaulting or killing a resident, or attacking an officer) ends the career on the force and leaves a record. Bystanders hit in a gunfight with a gang (on a gang's turf, or while a gang is shooting) are crossfire: expected, and no crime, no wanted level, no lost respect (`Career::stray_round`). Murder is another story: three bystanders dead by your gun before your next job is done, and the badge goes. Outside a gang fight, hurting a resident is a crime as before.
- Joining a different gang starts your respect again from zero.
- **Trucking** is open to anyone but a serving officer, gang members included: sign on with the boss at Verda Transporto's depot and drive loads for pay by the trip. See [trucking](trucking.md).

## Ŝtata Malliberejo, the state prison

Verda's only prison stands alone south-east of the ring road, about a kilometre from the nearest town, on its own level ground at the end of an access road. Behind a 6 m wall with razor wire are four guard towers, a gatehouse, the intake bay, the admin block, a two-storey cell block and an exercise yard. The map always shows it.

- **Officers** bring their prisoners here (above).
- **Getting arrested:** at one star an unarmed officer who reaches you, with you not firing, makes an arrest. At any level, on foot with **no gun in hand** (cycle to UNARMED), you give up to an officer within 9 m. You do time here: 60 s, plus 30 if you have a record, and more the more notorious a gang member is. The job in hand is off and no work comes while you're inside. Walk out of the gate early and you're put back. When your time is up you're let out at the gate. See [callouts and prison](callouts-and-prison.md) for the booking photo, the cell, the snitch and the SHU.
- **The gang life:** a gang member will do a few stints here. Doing your time quietly earns +3 respect with the crew; shanking the snitch earns +8 and a trip to the SHU.
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
| **ROBBERY** / **DRIVE-BY** | A crime in progress: two gang members and their getaway car (colour, model, plate on the radio). Stop them, cuff them, bring them in | 650 / 600 | 10 |

- **Callouts** (ROBBERY, DRIVE-BY) come up instead of the next job about two times in three; the very first call is always one, and comes with a partner, Officer Novak, who talks you through it. See [callouts and prison](callouts-and-prison.md).

- **Patrols** count in any police car: your own, a station's, the Interceptor or a cruiser. A town counts within 40 m of its centre (towns only, not the ranches).
- **Gang war calls** don't go stale: once the shooting stops, the call points at the nearest crew of either gang, and it ends if there is nobody left to stop.
- Firing your gun on duty is never reported as "shots fired".

### Arrests

- Walk up to a Krimulo and press **USE** (E) to cuff him. A suspect with his **hands up** is cuffed at once.
- Cuffed, he **walks with you** (hands behind his back). Get in a police car with him within 14 m and he climbs into the back: **IN CUSTODY**, up to four at a time. Then he counts like a takedown on a RAID or GANG WAR call (and towards a CLEAN UP), +1 respect. Leave him 170 m behind and he slips away.
- Drive them in a police car to the gate of **Ŝtata Malliberejo**, the state prison (the compass points the way while you have prisoners), to book them in: 120 kronoj each (more with rank) and +2 respect each. Your first drop-off comes with the warden's tour. Die and they get away.
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
  - Officers build it too (THE PATROL, below): every outpost an officer opens comes on top of those.
- **Authority:** an officer's AUTHORITY (0-100) is what the patrol is built on.
  - It grows with the work: +3 for every job done, +8 for every town cleaned up, +5 for every outpost opened, +1 for every prisoner booked at the state prison. A secure Verda makes it 100.
  - At authority 15, 35, 55 and 75 the Chief orders **THE PATROL** (900 pay, 10 respect): drive any police car out to the next outpost site on the ring (the waypoint), and the outpost opens with its cruiser and troopers.
  - The bag's QUESTS shows authority and how many outposts you built.
  - Each outpost has a cruiser and two troopers.
  - The map shows every outpost (tan shield); the compass shows them within 1.4 km.
- **On the beat:** each cruiser drives round and round the ring road.
- **Backup:** a cruiser that spots a wanted player close by joins the chase and radios the Policio for backup (+1 star, at most every 30 s).
- **Guarding the ranchers:** when a ranching family is in trouble (the player provoked it), the nearest cruiser rides out with its lights on, and the law is told (at least 1 star).
- **At the outposts:** the two troopers stand their post, and draw on a player with two stars or more within 70 m.
- **Tech:** placement, the beat and the art are in `game::services::HighwayPatrol`; the cars and troopers are driven by `EmergencyServices` (`UnitKind::Highway`). The cruiser comes from `tools/generate_emergency_vehicles.py` (`highway_patrol`) and the hat from `tools/generate_ranch_models.py` (`trooper_hat.glb`). Dev: `OUTLAND_DEV_PATROL=5` opens every outpost.

## The campaign: rid Verda of the gangs

The goal of the police questline is to get rid of the gangs. Once you serve, about 4 calls in 10 are the Chief's own orders: **CLEAN UP** (1400 pay, 14 respect).

**Why it matters (the Chief's words):** "Our people are becoming lawless. If we do not get our act together, our sovereignty is at stake." He means it: if the gangs win Verda, Russian troops land "to restore order" and must be fought town by town (see [gangs](gangs.md#how-it-ends)). Talk to him (USE) and he says so, between the count of towns still held.

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
- **The end: a secure Verda.** When the last crew is gone, **VERDA IS SECURE**: the Chief makes you Komisaro, your authority is full and every outpost is open.
  - La Verda Mano, the last of the gangs, lay down their guns. Their Verda City bar becomes **La Verda Mano Kultura Societo**, a cultural society: four of them stand outside under the banner, in their colours, unarmed. The mural stays.
  - Talk to them (USE): Esperanto classes, the old songs and the flag, "a pen, not a pistol".
  - "VERDA IS SECURE" shows at the top of the screen from then on.
- This is the law's side of Verda's yin and yang (see [gang wars](gangs.md#yin-and-yang)): every town you clean up makes the island safer. On the other side, a gang Leŭtenanto whose gang runs at least 75% of the island may be ordered to kill the Chief (THE BIG HIT). If that happens, the Policio is left without a head and takes no more recruits.

## The outlaw truckers: La Frateco de Kamionistoj

Verda's trucking is a tough grind, so the truckers stick together. They are Esperantistoj, working class, in every town on the island, and they hold out for a higher rate. The brotherhood is the third road, beside the gangs and the Policio.

- **Home: La Ringo.** The brotherhood's home is La Ringo Okcidento, the bar and grill on the ring road's west side, with its truck park full of brothers' rigs (marked on the map: LA FRATECO - LA RINGO). See [highways](highways.md#la-ringo-the-rings-bar-and-grills).
- **Joining:** talk to **Ivo Kovač**, at La Ringo's door (USE opens a menu: **Join** or **Talk**). Anyone but a cop or a gangster can join (they leave their side first, and a brother leaves the brotherhood before joining one). A brother's menu has **Talk**, **Call a strike**, **A contraband run** and **Leave the brotherhood** (which asks again before you go).
- **Ranks:** Novulo, Ŝoforo, Veterano, Ŝosea Lupo, Reĝo de la Vojo.
- **The union rate:** every Verda Transporto load pays a brother 25% more than the company rate, plus every raise won on strike (up to +50% in all). Each load is also +3 respect. The pay message shows both rates.
- **Truckers in every town:** brothers stand about La Ringo's door and among its rigs, one is at the diesel lane in Verda Transporto's yard, and a Kamionisto stands outside a pub (or the shop) in every town. They talk shop with a brother, and point anyone else to Ivo at La Ringo.
- **The CB (BACKUP / Z):** two brothers come from the nearest truck stop in a pickup to fight beside you, the same way an officer's backup does. They stay a few minutes, longer while there's shooting.
- **The brotherhood's work is voluntary.** Nothing comes over the CB on its own: ask Ivo for a strike or a contraband run when you want one, and he sends it ("ON THE CB").

| Job | What | Pay | Respect |
|---|---|---|---|
| **STRIKE** | Bring a big rig into Verda Transporto's gate and keep it there for 60 s in all. See [the strike](#the-strike) below. Win and the brotherhood's rate goes up 5% for good (up to +25% from strikes). | 500 | 10 |
| **CONTRABAND RUN** | Unmarked crates at a smugglers' jetty: pick them up in a big rig, and somebody talks (2 stars, the Ŝosea Patrolo is out). Run them to a freight yard across the island. | 2200 | 14 |

### The strike

A strike only happens when you call it: talk to Ivo with nothing else on and pick **Call a strike**.

1. **Bring a big rig to the gate.** The gate is where the yard road meets Verda Transporto's depot, marked on the compass. Any big rig you drive there counts: your own, one off the truck park or the company drivers' rigs, with its trailer on or not. The rigs parked in the truck park don't count on their own.
2. **Hold it.** Stop the rig across the gate (within 14 m of the marker) and stay with it. You can stay in the cab, or get out and stand on the picket line (within 40 m). Four brothers come out to picket either side of the gate, with signs (STRIKO!, JUSTA SALAJRO).
3. **Watch the clock.** A line on screen counts down ("the gate is blocked: hold it 0:42 more"). The bag shows how many of the 60 seconds are held.
   - Halfway through, Tomaso Rado comes out shouting. Let him.
   - If you drive away, the clock stops and waits for you to come back. It doesn't start again from zero.
4. **Win.** After 60 s held in all, the company gives in: 500 kr, +10 respect, and every load pays 5% more for good.

A strike under way doesn't time out. Once you've won all five raises (+25%), Ivo says the company has given all it will.

- **Your own rig:** a brother of rank Ŝoforo or higher is given a rig of his own, parked in the last bay of La Ringo's truck park (Ivo says so). It is saved with your cars and can haul any of the company's loads except cattle.
- **The depot's service bays:** Verda Transporto's shed is a drive-in garage. Drive in through either open door and stop in a bay for **VERDA TRANSPORTO · SERVO**: service (repair and fill up), engine (the diesels, see [trucking](trucking.md#engines-and-tanks)), **long-range tanks**, **armour plating** (steel 6000 kr, fortress 15 000 kr; the cab takes a half or a quarter of the damage, and fortress plating stops bullets reaching the driver), a **roof gun** (12 000 kr), paint and a bumper. Plating and the roof gun fit rigs and trucks only. Brothers pay 70%.
- **The trailer:** the shed only takes a rig without its trailer. A rig with its trailer hitched is stopped at the doors ("drop it out here first"). With the rig stopped, J (touch: HITCH) drops the trailer where it stands. Back the rig up to it, lined up, and J again to hitch it. A load only counts with the trailer on.
- **The roof gun:** at the wheel of a rig with one fitted, look to aim and fire (LMB / FIRE). It fires rifle rounds along your view.
- **Hijackers:** gangs sometimes send two or three men in a car after a loaded rig. A plated rig with a roof gun is the answer.
- **Outlaws:** a crime goes on a brother's record and raises his notoriety, like a gangster's. The Policio won't take him with a record.
- **If the gangs win**, the brothers fight the occupiers like anyone else (LIBERATION orders, see [gangs](gangs.md#how-it-ends)).

## Your crew

Your side, rank, respect, honour or notoriety, and crew are in the bag's **QUESTS** section,
beside the job in hand, so the screen stays clear. The turf you walk onto and a gang's hideout
are named for a few seconds under the compass.


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
  job in hand. The others ride too. USE again in the passenger seat to pull over and get out. From the passenger seat you can shoot any gun out of the
  window (the shot leaves from outside the car, so it never hits your own ride). A
  goal less than 70 m away is walked.

A fallen recruit goes back to his hideout. Recruit another, or ask the desk for a new partner.
Leaving the gang or handing in the badge sends the crew home, and so does dying. The crew is not
saved: recruit again after loading a game.

## Drive-bys

- **The driver can't shoot** (except a fitted roof gun, above). The shooting is done by the passengers.
- Your **crew ride at the windows** and shoot out of them at your enemies. Drive past a rival gang's people with your crew aboard and the fight is on; kills they make count for your gang job.
- Gangsters driving their own cars do the same: their passengers fire from the windows.
- From the passenger seat (your crew driving) you can shoot out of the window yourself.

## The armory and the gun man

- **The Policio's armory** is at every station's desk (USE on duty: partner, armory, badge). It is the best kit on the island, and all of it is free, but restricted by rank: a Kadeto draws a pistol and a plate carrier, the shotgun and VM9 come at the next rank, rifles and grenades after that, the sniper and auto shotgun later, the rocket launcher only for a Komisaro.
- **An officer on duty wears body armour** (level II), and every police car carries a pump shotgun: get in one for the first time and you take it with 10 shells.
- **The gang's GUN MAN** stands by the door of each gang's main base, in its colours. He sells to family only: guns cheaper than the Armilejo, the heavier ones for those of higher rank. Code: `game::career::armory` (`include/outland/game/career/Armory.hpp`).

## Saving

The career is saved beside the Explore loot profile (`<profile>.career`): side, gang, respect, record, honour, notoriety, towns defended, jobs done and the patrol car. The towns the Policio has cleared are saved with the territory. A job in progress is not saved; a new one comes after loading.

## Dev and tests

- `OUTLAND_DEV_CAREER=gang:N`, `police` or `trucker` starts the session in a gang, on the force or in the brotherhood.
- `OUTLAND_DEV_SECURE=1` opens the Green Hand's cultural society at once.
- `OUTLAND_DEV_STRIKE=1` (with `OUTLAND_DEV_CAREER=trucker`) has Ivo call a strike at once.
- Dev builds log every crew door and police station.
- `outland_training_camp_tests`: a drill starts from the line, targets take real traces and head shots, leaving the line stops it, retries, movers and graduation.
- `outland_career_tests` covers:
  - joining, and the record shutting the police door;
  - hits, drive-bys only counting from a car, and robbing a rig;
  - a lapsed job costing respect, and killing your own;
  - the questline (orders, the Chief, the camp, the Interceptor);
  - patrols needing the patrol car, raids, gang-war calls;
  - going bad, ranks, and save/load;
  - authority and THE PATROL; the brotherhood, the union rate, a strike and a contraband run.
