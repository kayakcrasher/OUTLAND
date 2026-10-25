# La Verda Basballigo: the ballparks and the league

Explore only. Every county has a high-school-sized ballpark near its main town, and a team in its
colours. The five teams play each other through the season on the island clock, and the towns
turn out to watch.

## The teams and their parks

| Team | Town (county) | Colours | Park, centre (x, z) |
|---|---|---|---|
| Verdaj Leonoj | Verda City (Verdkerno) | green and gold | east of Orienta (606, 46) |
| Altverdaj Ursoj | Espera (Altverdo) | brown and orange | west of town (−256, −1755) |
| Lumaj Mevoj | Porto Luma (Lumamaro) | navy and sky blue | (1630, −186) |
| Marĉaj Ardeoj | Suda Haveno (Marĉfluo) | teal and white | (180, 1586) |
| Feraj Ministoj | Roka (Ferarbaro) | charcoal and red | (−1708, 686) |

The map marks each one as `<NICK> BALLPARK`. On a game day the compass shows `PLAY BALL` over
the park where a game is on.

## The ballpark (modular, one model per team)

`assets/verda/city/civic/ballpark_<team>.glb`, written by `tools/generate_city_kit.py`
(`ballpark()`). The world kit places it (`ballparks()` in `src/dev/VerdaWorldKit.cpp`) with its
front toward the nearest road, and lays an access road to it. The terrain is levelled under it
(`world::ballpark::flat_radius`). It is 136 × 140 m and has:

- **The field:** a grass outfield with an 88 m fence, padded in the team's colour with a yellow
  cap. Also a dirt infield, a raised mound, the bases and plate, chalk foul lines, foul poles and
  a backstop.
- **Dugouts** either side of the plate: the home team's on the first-base side.
- **The stands:** two stepped aluminium bleachers along the baselines, six rows each. The home
  crowd sits on the first-base side and the visitors on the third-base side. Each stand has a
  rail behind it and the club's banner board.
- **The locker room** (Vestejo) behind the home stand. You can go in: there are lockers in the
  club's colours, benches, a crest and the showers. Its door is on the car park.
- **The car park** at the front, with two bus bays in the team's colour (home and visitors) and
  the gate.
- **A scoreboard** beyond centre field and four light towers.

The frame (the plate, the bases, the fielders' spots, the seats, the bays) is in
`include/outland/world/Ballparks.hpp`. `ballpark::find` reads the parks off the map. The kit
stores a city model at the centre of its bounds, so `find` shifts back to the design origin.

## The team buses

Each team has a bus (`vehicle_spawn_bus_team_<n>`), painted in its colour (`autos::paints`, after
the street colours), parked in its home bay. On the day of an away game it waits in the visitors'
bay at the host's park, then goes home. A bus is only moved while nobody is near it or its bay.
The town bus line doesn't use them.

## The season

`game::sports::League` (`src/game/sports/League.cpp`):

- **Schedule:** a double round robin. There are two games every island day at 14:00, with one
  team resting, for ten days. Each team hosts each other team once.
- **The final:** after a day off, the top two meet at the leader's park in La Finalo. A new
  season starts two days later.
- **Results:** every game is settled by the season's seed. A game's line score (runs per
  half-inning, and the minute each run crosses the plate) follows from the fixture alone. So
  where a game stands is just a function of the clock, and only the season, its seed and the
  rivalries are saved (`<career>.league`).
- **Clock:** a half-inning is about 12½ island minutes. The gates open 75 minutes before first
  pitch, and the crowd stays 40 minutes after the last out.
- **News:** first pitch, the final scores and the champion arrive as messages. Near a game you
  also hear each run and see the scoreboard panel (score, inning, LA FINALO).

## The fans

- **The crowd:** the home crowd fills the first-base stand. Most fans wear the team's jersey
  (`assets/verda/characters/outfits/team_<n>_jersey.png`), and some come in their own clothes.
  They sit, and stand up to cheer when their team scores.
- **Travelling fans:** some of the visitors' followers travel to the game and fill the
  third-base stand. How many depends on the team (the Ministoj travel most) and on the rivalry.
- **Rivalries:** they grow with every game between two teams, and most of all with a final.
- **Fights:** now and then a fight breaks out between the two sets of fans, more often between
  rivals. Pairs brawl on the concourse behind the stands, and the news reports it.
- **Riots:** the championship final always boils over. When the final ends, the beaten side's
  fans riot at the park for two island hours. Fires burn on the field. If you are near, a dozen
  rioters in their club's shirts go for anyone close, including you. The police send cars to the
  gate, and officers circle the scene until it's over.

### Sitting down

Walk up to either stand and press **E** / **USE** to take a seat. Press USE again (or move) to
get up. While you're seated the panel also shows the season table.

## Testing

`outland_league_tests` checks:

- the schedule;
- the games, read off the clock;
- the events;
- the table and the final;
- the champion and the riot;
- the fans;
- a new season;
- the save;
- that every park is on the published map where the game finds it.

The dev build's `OUTLAND_DEV_LEAGUE` takes `game[:park]`, `final[:park]` or `riot[:park]`. It
starts a game at that park, the final, or the riot, and with a park it puts the final there.

## Limits

- The crowd and the players are drawn, not simulated. The game's events come from the line
  score, not from a played ball.
- Travelling fans appear as the away section. They don't drive across the map.
- Fights in the stands are scripted pairs. Only rioters are real hostile people.
