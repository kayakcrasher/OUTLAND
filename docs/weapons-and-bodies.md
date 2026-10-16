# Weapons and body animation

The guns are modelled at real-world sizes, and characters hold them in their hands. Their proportions follow real guns and how PUBG and Fortnite present them; no code or art was taken from either game.

## The armoury

| Gun | Type | Damage | Fire rate | Magazine | Aim zoom | Range role |
|---|---|---|---|---|---|---|
| V9 pistol | Semi-automatic 9 mm | 35 | 0.135 s | 15 | 55° | Sidearm |
| VR30 rifle | 5.56 automatic | 41 | 700 rounds/min | 30 | 46° (holographic sight) | All-rounder |
| **VK45 SMG** | .45 automatic | 32 | 800 rounds/min | 25 | 52° (red dot) | Close range; steady from the hip; damage falls off with range |
| **VS12 shotgun** | 12-gauge pump | 9 pellets × 22 | 0.8 s pump | 5 | 60° | Inside 15 m; useless at 60 m |
| **VP7 sniper** | 7.62 bolt action | 82 (205 to the head) | 1.55 s bolt | 5 | 15° (4× scope) | Long range |
| **VD2 double barrel** | 12-gauge side-by-side | 12 pellets × 22 per barrel | 0.24 s (both barrels) | 2 | 62° | Hardest hit inside 8 m; nothing past 30 m; slow break-open reload |
| **VA12 auto shotgun** | Drum-fed automatic 12-gauge | 8 pellets × 14 | 300 rounds/min | 8 | 60° | Close-range spray; a long drum change |
| **VR7 rocket launcher** | Shoulder-fired rocket | Blast: 220 within 3 m, nothing past 10 m | 1 shot, 3.6 s reload | 1 | 50° | Cars, groups, anyone behind thin cover |

- **Shotgun pellets** spread in a fixed cone (about 3°; 4° for the double barrel) around the aim point, whatever the stance. Air drag soon bleeds their damage away.
- **Shells:** all three shotguns load the same 12-gauge shells (`combat::ammo_of`). Walking over shells picks them up if you carry any shotgun.
- **Rockets:**
  - A rocket flies straight at 60 m/s and drops only a little. It goes off on the first person, car or wall it touches, or after 5 s if it hits nothing.
  - The blast reaches 10 m; walls between it and a target stop it, as with frag grenades. It hurts the shooter too.
  - It does double damage to cars, so one rocket wrecks most of them.
  - Rockets are their own ammo item (`rockets`, 4 to a stack). They are rare in weapon spawns, and the general store sells them.
  - In LAN play the rocket is replayed on everyone's screen. Like a grenade, each player's own hits are theirs to send.
- **The sniper scope** replaces the view with a scope picture: a black surround, a fine cross-hair and thick outer posts. The normal camera hides while scoping, in first or third person.
- **The pump and bolt** visibly work after each shot.
- **Sound:** each gun has its own pitch on one of the two recorded reports. Each also has its own hearing distance on the sound bus (180–420 m), which bots and zombies react to.
- **Where to find them:**
  - loot in weapon spawns (`loot_tables.tsv`);
  - the general store in Explore (`offers.tsv`);
  - new ammo items: `.45 rounds`, `12-gauge shells`, `7.62 rounds`, `Rockets`.
- **Battle Royale bots carry mixed loadouts:** 40% rifle, 20% SMG, 18% pistol, 7% pump shotgun, 4% double barrel, 3% auto shotgun, 8% sniper. Each bot fights at its gun's range. No bot carries a rocket launcher.
- **Saves** list every gun's loaded rounds. Older saves with only two guns still load.

## Real-scale guns

`CombatRenderer::draw_gun(grip, aim, up, …)` builds every gun in metres. The gun's origin is the trigger, on the bore line.

| Gun | Overall length |
|---|---|
| Rifle | 0.84 m |
| Pistol | 0.19 m |
| SMG | 0.56 m |
| Shotgun | 0.89 m |
| Sniper | 1.12 m |
| Double barrel | 0.83 m |
| Auto shotgun | 0.84 m |
| Rocket launcher | 1.35 m |

- **Rifle detail:** quad-rail handguard, holographic sight, raked magazine, buffer tube and stock.
- **Double barrel:** side-by-side barrels on a box lock, with twin hammers and a brass bead. The barrels drop open while it reloads.
- **Auto shotgun:** ribbed shroud, muzzle brake, carry handle with an aperture sight, and a 20-round drum that drops away on a reload.
- **Rocket launcher:** an olive tube with a venturi bell, a warhead in the muzzle while loaded, flip-up sights, and pistol and fore grips.
- `gun_layout(id)` gives each gun's stock length, where the support hand goes, and the sight height.
- **First person:** the gun sits at the hip with gloved hands and sleeves. When aiming, its sight line lines up with the eye.
- **Third person:** the camera now looks over the right shoulder, from closer in (3.6 m, or 1.9 m when aiming), as PUBG and Fortnite frame it. That puts the gun in view.
- **On the ground:** guns lie on their side.

## Body animation (procedural, `SkeletalGait` + `BodyPose`)

All of this works on every rig the game already animates. No new clips are needed.

- **Holding a gun:**
  - **Long guns:** the stock sits in the right shoulder pocket and the chest turns into a bladed stance. Both arms are solved with two-bone reach: the trigger elbow flares out, and the support hand goes under the fore-end.
  - **Pistols:** both arms out, the left hand cupping the right.
  - **Hands:** each hand turns so its fingers close round the grip.
  - **Aiming:** the chest, neck and gun follow the aim up and down.
  - **Sprinting:** the gun drops to a low carry.
- **The gun is placed from the hands:** the renderer draws it where the hands hold it (`HeldGun`), so the hands and the gun always agree.
  - Police carry their pistols this way.
  - Battle Royale bots carry their loadout, at a low ready while walking.
- **Crouch:** knees and hips bend, the body leans forward, and the feet stay planted. It blends in and out.
- **Idle:** the body breathes, and the arms hang a little away from the hips instead of in a T-pose.
- **Walking:** the hips sway with the stride and the chest counters it.
- **Zombies:** they stoop, limp on one leg, and lurch with both arms out. A bite chops the arms down and back up.
- **Death:** the legs give way, the body topples backwards over about 0.8 s and comes to rest on its back. This replaces standing in a T-pose.

## Dev tools

- `OUTLAND_DEV_ARMOURY=1` (dev builds): starts Explore, Zombie Survival or Battle Royale with every gun, loaded.

## Tests

- **`outland_combat_tests`:**
  - The SMG out-fires the rifle, the shotgun fires 9 pellets in a cone, and the sniper's bolt never auto-fires.
  - The double barrel fires 12 pellets, the auto shotgun out-fires the pump, and all shotguns share shells.
  - The rocket launcher hands back a rocket instead of firing bullets.
  - Every gun has an item key, a length and a hold.
- **`outland_grenade_feedback_tests`:** a rocket bursts on the wall it hits, at shoulder height, within a third of a second. A rocket that hits nothing bursts when its motor is spent.
- **`outland_skeletal_gait_tests`** uses a full body to check:
  - limb lengths are kept under reach;
  - the grip is ahead of the chest, with the support hand forward;
  - aiming up raises the gun;
  - the pistol is held in both hands;
  - crouching drops the hips with the feet planted;
  - zombies reach forward;
  - the dead lie flat on their backs, never under the ground.
- **`outland_loot_tests` and the loot asset audit** cover the new items and tables.
