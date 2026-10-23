# The motor trade: Marnafto, Meĥanikejo, Aŭtovendejo Verda

Explore. Verda runs on its own oil, its own garages and one car lot.

Code: `game::autos` (`Autos.hpp`: parts, paint, tuning, repairs, pump prices, the lot's stock,
bought cars; `AutoMenu.hpp`: the counter's menu), `world::motor` (`MotorTrade.hpp`: where the
pumps, bays and desk are in each building), `dev::build_verda_towns` (`motor_trade()`), the
models in `tools/generate_city_kit.py` (`gas_station()`, `mechanic_shop()`, `car_lot()`,
`oil_rig()`). Tests: `outland_autos_tests`, `outland_world_kit_tests`, `outland_road_audit_tests`.

## Marnafto (filling stations)
Marnafto is the island's oil company. Its rigs, **Platformoj Marnafto**, stand out at sea off
the north-east, east and south coasts; it refines on the coast and sells at its own stations.
Its colours are deep blue and orange, and its logo is an orange drop over a white rig on the
sea line. The slogan reads *Nafto el niaj propraj platformoj* ("oil from our own platforms").

- **Seven stations:** two in Verda City (one by Verda Transporto's yard on the truckers' road,
  one out east on the Porto Luma road; both clear of downtown and the stadium ground), one each in
  Espera, Porto Luma, Suda Haveno and Roka, and one off the ring road.
- **The station:** a forecourt, a canopy on four columns over two pump islands (two pumps each),
  a kiosk shop at the back (enterable; it sells what a general store does) and a price pylon
  with the logo. Cars drive under the canopy; they meet the pumps and columns, not a box.
- **Filling up:** pull up beside a pump and stop. The attendant comes over: *Fill up* costs 2 kr
  a litre for a 60 litre tank (a full tank from empty is 120 kr), or whatever your cash buys.
- **The tank:** a car runs about 25 km on a full tank in Explore (a tuned engine drinks more).
  Empty, it won't pull. The compass shows the nearest station within 500 m, or from anywhere on the
  island once the tank is under 30%.
- **Big rigs** go four times as far on a tank and have their own drive-up diesel lanes; see
  [trucking.md](trucking.md#diesel-for-the-rigs).

## Meĥanikejo (workshops)
A modular workshop with two drive-in bays, lifts, benches and tyre stacks, and an office on one
side. There is one in Verda City (beside the car lot) and one each in Espera, Porto Luma, Suda
Haveno and Roka. Drive into a bay and stop: the mechanic asks what you want. You can leave at any
time (LEAVE / ESC), or go through the list:

| Job | Choices | Price |
|---|---|---|
| Paint | factory colour, or Verda green, racing red, sea blue, midnight black, pearl white, sunflower yellow, rig orange, gunmetal grey, plum purple, sky teal | 1,200 kr |
| Engine | factory; tuned (+30% power); V8 big block (+65% power, +15% top speed) | 2,500 / 7,000 kr |
| Tyres | low-profile (small, sits low, grips); factory; all-terrain (bigger, a lift, climbs 6° steeper); monster (huge, a big lift, climbs 14° steeper) | 800 / 1,500 / 4,000 kr |
| Front bumper | factory; steel push bar (crashes do 35% less damage); bull bar (60% less) | 900 / 1,600 kr |
| Repair | body, engine and tyres back to new | 6 kr a point of damage |

- **Paint** recolours the body's own paint and leaves the glass, chrome and lamps alone.
- **Tyres** change the wheels' size and lift (or drop) the body with them.
- **Bumpers** are drawn on the car's nose.
- The work is saved with the car (`OUTLAND_VEHICLES 2`).

## Aŭtovendejo Verda (the car lot)
The car lot is in Verda City, west of downtown: a paved lot with bunting, two rows of bays, a sign
and a glass sales office. One of each car it sells is on show, with its price over it (nobody can
drive those off). To buy, walk up to the salesman's desk and press USE:

| Car | Price |
|---|---|
| Classic hatchback | 6,000 kr |
| Vakero side-by-side | 8,500 kr |
| Verda Sedano | 11,000 kr |
| Verda pickup | 14,000 kr |
| Verda Mano GT | 45,000 kr |

The car waits at the lot's exit, nose to the street. It is yours: it comes back where you left
it next time you play, with whatever the mechanic fitted and the fuel in its tank. You can own
up to 12 cars.
