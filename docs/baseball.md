# La Verda Basballigo: the ballparks and the league

Explore only. Every county has a high-school-sized ballpark near its main town, and a team in its
colours, named for its city. The five teams play each other through the season on the island clock. The games are
played out on the field, pitch by pitch, and the towns' own people come to watch.

## The teams and their parks

| Team | County | Colours | Park, centre (x, z) |
|---|---|---|---|
| Verda City | Verdkerno | green and gold | east of Orienta (606, 46) |
| Espera | Altverdo | brown and orange | west of town (−256, −1755) |
| Porto Luma | Lumamaro | navy and sky blue | (1630, −186) |
| Suda Haveno | Marĉfluo | teal and white | (180, 1586) |
| Roka | Ferarbaro | charcoal and red | (−1708, 686) |

The map marks each one as `<CITY> BALLPARK`. On a game day the compass shows `PLAY BALL` over
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
  - **the field is drawn over:** the ground is laid in layers well apart (the pad's grass, the
    dirt, the infield grass, the circle round the plate), and the chalk sits on top of them all,
    12 cm wide (the foul lines 15 cm), so the lines show from the stands and don't flicker. Every
    disc and fan of dirt faces up (the skin, the mound, the circles);
  - the running lane to first, the coaches' boxes, and on-deck circles in the team's colour.
- **The outfield:** an 88 m fence, padded in the team's colour with a yellow cap, a dirt warning
  track inside it, foul lines and foul poles.
- **A backstop:** behind the plate, angled in toward the lines: a padded wall waist high and a
  see-through net above it (thin posts, light cables, a top rail), so the grandstand behind it
  watches the game through it.
- **Dugouts** along the lines in foul ground (16–28 m from the plate), facing the diamond, the
  home team's by first base. Each has block walls, a roof in the team's colour, the bench and a
  rail. They sit clear of the grandstand's view of the infield.
- **The grandstand:** covered, behind the plate and the backstop, like a high-school park's main
  stand. Two wings of stepped aluminium seats, eight rows each: the home crowd on the first-base
  side, the visitors on the third-base side. A concrete aisle runs up the middle and one at each
  end, with a low padded wall in front (open at the middle aisle). A rail runs along the back, a
  roof on steel posts covers it all, and the press box sits at the top of the middle aisle with
  its window on the field. It is solid underneath: walk up the rows. The home crowd sits by first and
  the visitors by third. A low fence separates them from the field, and each has a rail behind
  it and the club's banner board.
- **Bullpens** down each line: a mound and a plate 60 ft 6 in apart, and a bench.
- **The locker room** (Vestejo) by the car park on the home side, beyond the grandstand's end. You can go in: there are lockers in the
  club's colours, benches, a crest and the showers. Its door is on the car park.
- **The car park** at the front, with two bus bays in the team's colour (home and visitors) and
  the gate, behind the grandstand.
- **A scoreboard** beyond centre field (see [the scoreboard](#the-scoreboard)) and four light towers.

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
- **Results: nothing is decided in advance.** Every game is simulated between the two clubs'
  rosters (`game::sports::simulate_game`, `src/game/sports/GameSim.cpp`), and the score and the
  winner are whatever the game produces.
  - **The players:** each club has nine batters (the designated hitter bats for the pitcher) and
    three pitchers, rated 0–1 for contact, power, eye, speed, arm and glove (pitchers: velocity,
    control, stuff, stamina). A club's strength sets its players' average, and every season
    brings a new roster (it's high school).
  - **Every pitch:** in the zone or not (control, worse as the pitcher tires), taken or swung at
    (the batter's eye, the count), missed, fouled off or put in play (contact against stuff).
    Strikeouts and walks come from the count.
  - **Every ball in play:** an exit speed (about 70 mph on average, up to about 98), a launch
    angle and a direction.
    - **In the air,** it's flown with gravity and drag: over the 88 m fence is a home run, off it
      is a ball in play; otherwise it comes down where the flight says.
    - **On the ground,** it skips along and slows down.
    - **Foul pop-ups** can be caught.
  - **Every fielder:** whether he can get there in time (his speed, a first step, the angle). Fly
    balls are caught, dived for or dropped. Grounders are cut off, booted (errors, from his glove)
    or thrown to first in time or not (his arm, the transfer).
    - **Double plays:** turned when the force at second and the relay beat the runners.
    - **Base hits:** the batter beats the throw (infield singles), or the ball gets through.
  - **Every runner:** his time to the next bag against the ball's: retrieved, relayed, thrown. He
    takes the extra base when he's safe, sometimes tries when it's close and gets thrown out.
    Tagging up on a fly ball works the same way.
  - **The game:** the starter tires and the bullpen comes in. Extra innings start with a runner
    on second, and a walk-off ends the game.
  - **Typical numbers:** about 5 runs a game between the two sides, a .260 batting average, 23%
    strikeouts and 11% walks, about 0.6 home runs, 0.9 doubles and 0.75 double plays a game, and
    extra innings in about one game in seven. The better club wins more often, but not always.
  - The same season seed plays the same games, so a game can still be read off the clock.
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
  also hear each run (who drove it in, and how) and see the scoreboard panel: the score, the
  inning, the outs, a little diamond with the runners on, who's at bat with the count, who's
  pitching with his pitch count, and LA FINALO for the final.

### The scoreboard

The board beyond centre field shows the game as it stands (`Linescore::box`):

- each side's runs inning by inning (seven columns, more for extra innings as they start; a
  half not yet begun is left blank);
- R, H and E (runs, hits, errors charged to the fielding side);
- the inning and the outs, or FIRST PITCH before the game and FINAL after it.

A run goes up the moment the runner crosses the plate: the same moment as on the panel and in the
news, so the board, the panel and the field always agree. A hit or an error counts as the ball is
put in play. Between games the board shows the home club's name. The board is drawn into a
texture only when what it shows changes (`ParkBoards` in the renderer).

## The game on the field

`game::sports::scene` (`src/game/sports/BallGame.cpp`) reads off the plays where everyone is at any
moment, and the renderer draws it. A right-handed batting order throughout.

- **Each plate appearance:**
  - **The pitch:** the set, then the wind-up with the leg kick, the stride, the arm over the top
    and the follow-through. The ball flies to the plate.
  - **At the plate:** the batter waits in his stance and swings (the bat is drawn round). He takes
    ball four, or swings through strike three into the catcher's mitt, and the catcher lobs it
    back to the mound.
  - **The tracer:** once the ball is hit, a bright yellow line follows its flight from the bat:
    the arc of a fly ball or a home run, the bounces of a grounder, a single dropping in and
    rolling, a double to the wall. It stays to the end of the play, then fades over six seconds
    (`Scene::trail`).
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
  - Seats face out over the backstop, turned a little toward the plate.
  - Police officers and doctors stay on duty.
- **Travelling fans:** the visitors' followers come from their own town across the island. Those
  with a car drive, and they leave an hour earlier. They sit in the visitors' stand. How many
  come depends on the team (Roka's travel most) and on the rivalry.
- **Rivalries:** they grow with every game between two teams, and most of all with a final.
- **Fights:** now and then a fight breaks out between the two sets of fans, more often between
  rivals. Pairs brawl in the car park behind the grandstand, and the news reports it.
- **Riots:** the championship final always boils over. When the final ends, the beaten side's
  fans riot at the park for two island hours. Fires burn on the field. If you are near, a dozen
  rioters in their club's shirts go for anyone close, including you. The police send cars to the
  gate, and officers circle the scene until it's over.

### Sitting down

Walk up to the grandstand (either wing) and press **E** / **USE** to take a seat. Press USE again (or move) to
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
- the scoreboard: minute by minute it shows the score the panel shows, and its innings add up to
  its runs and match the line score;
- the tracer: every ball in play leaves one from the bat, and none appears before contact or on a
  strikeout or a walk;
- the game on the field: nine fielders and a batter, every runner, the pitch, the ball in play,
  nobody faster than 16 m/s, the line-ups and the celebration.

`outland_life_tests` checks going to a game and coming home.

The dev build's `OUTLAND_DEV_LEAGUE` takes `game`, `gates`, `final` or `riot`, each optionally
followed by `:park`:

- `game` starts a game at that park;
- `gates` starts 20 minutes before that game's gates open, to watch the crowd arrive;
- `final` and `riot` put the final at that park.

## Limits

- The simulation is a model, not a physics engine: batted balls are flown with drag, but fielders
  run at constant speeds and throws are straight lines. Only the deciding pitch of each plate
  appearance is shown on the field (the scoreboard has the count). Nothing the player does
  changes a game.
- The players are drawn figures, not island residents.
- Fans in the stands are drawn in their seats; they only become bodies in the world on the way in
  and out.
- The time scale is compressed: running is a little quick (up to about 15 m/s), and a
  plate appearance is one pitch.
- Fights in the stands are scripted pairs. Only rioters are real hostile people.
