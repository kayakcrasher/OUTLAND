# La Verda Basballigo: the ballparks and the league

Explore only. Every county has a high-school-sized ballpark near its main town, and a team in its
colours. The five teams play each other through the season on the island clock. The games are
played out on the field, pitch by pitch, and the towns' own people come to watch.

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

- **The diamond, to the rule book:**
  - 90 ft (27.43 m) base paths;
  - the pitcher's rubber 60 ft 6 in (18.44 m) from the plate, on an 18 ft mound;
  - the infield dirt reaching 95 ft (28.96 m) from the mound, with the grass square inside the
    base paths;
  - a 13 ft dirt circle round a pentagon home plate;
  - chalked batter's boxes (4 × 6 ft) and a catcher's box;
  - the running lane to first, the coaches' boxes, and on-deck circles in the team's colour.
- **The outfield:** an 88 m fence, padded in the team's colour with a yellow cap, a dirt warning
  track inside it, foul lines and foul poles.
- **A backstop:** netting behind the plate, angled in toward the lines.
- **Dugouts** along the lines in foul ground, facing the diamond, the home team's by first base.
  Each has block walls, a roof in the team's colour, the bench and a rail.
- **The stands:** two stepped aluminium bleachers along the first- and third-base lines behind
  the dugouts, six rows each, angled square-on to the diamond. The home crowd sits by first and
  the visitors by third. A low fence separates them from the field, and each has a rail behind
  it and the club's banner board.
- **Bullpens** down each line: a mound and a plate 60 ft 6 in apart, and a bench.
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

- **Schedule:** a double round robin. There are two games every island day at 13:00, with one
  team resting, for ten days. Each team hosts each other team once.
- **Games:** seven innings (the high-school length), extra innings if tied, and the home side
  doesn't bat in the last if it's ahead. A walk-off ends the game when the winning run scores.
- **The final:** after a day off, the top two meet at the leader's park in La Finalo. A new
  season starts two days later.
- **Results:** every game is settled by the season's seed, plate appearance by plate appearance:
  - each play is a strikeout, walk, groundout, flyout, single, double, triple or home run;
  - where the ball goes and who fields it come with each play;
  - the runners move by the rules: forced on a walk, a productive groundout, the sacrifice fly,
    first to third on a double, and so on;
  - every run in the line score comes from a play (`Linescore::plays`).
- **Saving:** a game follows from the fixture alone, so where it stands is just a function of the
  clock. Only the season, its seed and the rivalries are saved (`<career>.league`).
- **Clock:** a plate appearance takes as long as its running needs: about 2 island minutes for a
  strikeout, up to about 9 for a bases-clearing triple. After it comes the time for everyone to
  get back to their places. The change-over between halves is 3 minutes.
  - A game averages a little over seven island hours (about 15 real minutes), ending under the
    lights around 20:00.
  - The gates open 75 minutes before first pitch, and the crowd leaves within 40 minutes of the
    last out.
- **News:** first pitch, the final scores and the champion arrive as messages. Near a game you
  also hear each run and see the scoreboard panel: the score, the inning, the outs, a little
  diamond with the runners on, and LA FINALO for the final.

## The game on the field

`game::sports::scene` (`src/game/sports/BallGame.cpp`) reads off the plays where everyone is at any
moment, and the renderer draws it. A right-handed batting order throughout.

- **Each plate appearance:**
  - **The pitch:** the set, then the wind-up with the leg kick, the stride, the arm over the top
    and the follow-through. The ball flies to the plate.
  - **At the plate:** the batter waits in his stance and swings (the bat is drawn round). He takes
    ball four, or swings through strike three into the catcher's mitt, and the catcher lobs it
    back to the mound.
  - **Ball in play:** grounders bounce out to an infielder, who scoops and throws to first, with
    the first baseman on the bag. Fly balls arc up and an outfielder runs under them and makes
    the catch. Hits drop in or run to the wall: the outfielder chases them down and throws to the
    cut-off man. Home runs sail over the fence while the outfielder watches from the warning
    track.
  - **Runners:** they lead off and run the bases: forced along, scoring, tagging up on a sacrifice
    fly. Then they walk off to the dugout. A batter who's out runs it out, then heads back.
  - **The others:** everyone else watches the ball. The catcher squats, the plate umpire stands
    behind him, and the batting side's coaches are in their boxes. The next man takes practice
    swings in the on-deck circle, and the rest sit on the benches.
- **After each play:** everyone jogs back to their places, and the next man walks up from the
  on-deck circle.
- **Between halves:** the fielders run out from their dugout, and at the end run back in. The
  batting side's runners left on base walk off.
- **Before the game:** both teams line up along the lines.
- **After it:** the winners mob the mound, jumping, while the losers sit in their dugout.
- **The body motions** are procedural (`characters::BodyPlay` in `SkeletalGait`): a fielder's
  crouch, the catcher's squat, the stance and swing, the pitch, a throw, a catch high or low, a
  cheer.

## The fans

- **The crowd is the town:** the fans are island life's own residents
  (`LifeSimulation::attend`). Ahead of each game, up to 70 people of the home town (about three in
  ten) set off for the park. On the day they drop their plans, go to the game and come home again
  after it. Their activity reads "at the ball game".
  - Near the player you see them walk (or drive) up and go in at the gate. From there they sit in
    their own seat in the home stand, in their own clothes.
  - They stand up to cheer when their team scores.
  - Police officers and doctors stay on duty.
- **Travelling fans:** the visitors' followers come from their own town across the island. Those
  with a car drive, and they leave an hour earlier. They sit in the visitors' stand. How many
  come depends on the team (the Ministoj travel most) and on the rivalry.
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

`outland_league_tests` also checks:

- the plays: three outs a half (bar a walk-off), the runs adding up, the base rules;
- the game on the field: nine fielders and a batter, every runner, the pitch, the ball in play,
  nobody faster than 16 m/s, the line-ups and the celebration.

`outland_life_tests` checks going to a game and coming home.

The dev build's `OUTLAND_DEV_LEAGUE` takes `game`, `gates`, `final` or `riot`, each optionally
followed by `:park`:

- `game` starts a game at that park;
- `gates` starts 20 minutes before that game's gates open, to watch the crowd arrive;
- `final` and `riot` put the final at that park.

## Limits

- The game is played out from its plays, which are settled by the seed. Nothing the player does
  on the field changes it, and there is no physics in the ball.
- The players are drawn figures, not island residents.
- Fans in the stands are drawn in their seats; they only become bodies in the world on the way in
  and out.
- The time scale is compressed: running is a little quick (up to about 15 m/s), and a
  plate appearance is one pitch.
- Fights in the stands are scripted pairs. Only rioters are real hostile people.
