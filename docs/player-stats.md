# Player stats

`game::player::PlayerStats` keeps the player's record on Verda, saved beside the career
(`<profile>.career.stats`) and shown in the bag under **YOUR STATS** (two columns, below the quests).

| Stat | Counted when |
|---|---|
| KM DRIVEN | driving (the vehicle's speed over each frame) |
| KM WALKED | on foot (a passenger seat, the bus, a respawn or a fast trip isn't a walk) |
| TOP SPEED | the fastest the player has driven (km/h) |
| KILLS | every kill the player makes |
| DEATHS | each time the player goes down |
| ARRESTS MADE | prisoners a cop books in at the state prison |
| TIMES BUSTED | each time the player is arrested |
| LOADS HAULED | each paid load delivered |
| RACES WON | street races won |
| EARNED / SPENT | money in and out of the wallet |
| TIME PLAYED | time in the world |

The file is `stats 1 key=value ...`: unknown keys are skipped and missing ones start at zero, so
new stats can be added without breaking old saves.
