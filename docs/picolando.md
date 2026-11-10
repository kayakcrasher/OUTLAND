# Picolando: pizza delivery

**Picolando** ("Pizza Land") is the island's pizza chain. It has a pizzeria in four cities, its own
flashy delivery cars, and three drivers of its own on the road all day. In Explore anyone can sign
on and deliver for it.

Code:
- `world::picolando` (`include/outland/world/Picolando.hpp`): the pizzerias and whose area is whose.
- `game::pizza::Delivery`: the player's deliveries.
- `game::pizza::Fleet`: the chain's own drivers (`include/outland/game/pizza/Picolando.hpp`, `src/game/pizza/Picolando.cpp`).
- `dev::pizzerias` in the world kit.

Models:
- `tools/generate_city_kit.py` (`pizzeria()`, `civic/pizzeria.glb`);
- `tools/generate_pizza_car.py` (`vehicles/runtime/pizza_body.glb`).

Test: `outland_picolando_tests`.

## The pizzerias
- **Where:** Verda City (downtown, west of the boulevard, south of the avenue), Roka, Espera and
  Suda Haveno, each facing a paved street near the middle of town. Built last by the world kit,
  so nothing else on the map moved for them.
- **The building:** 12 x 9 m and one storey. It has white stucco over a red-tiled plinth, a
  red-and-white chequer band under the eaves and green trim. At the front there is a glass door
  between two big windows under red-and-white striped awnings.
- **The giant pizza** on the roof is a two-metre round of cheese on its crust, with pepperoni and
  basil. It faces the street on two steel posts.
- **The sign:** PICOLANDO · PICOJ · LIVERADO · VARMEGE AL VIA PORDO ("pizzas, delivery, piping hot
  to your door").
- **Inside:** the counter with its till and a pick-up shelf of hot boxes, two brick ovens glowing at
  the back, a prep table, and three little tables with stools.
- **A place on the island:** each pizzeria is a pub-like place. People eat there and staff work there.
- **On the map and the compass:** all four are on the map. The compass shows the nearest
  within 450 m, or from anywhere while you're delivering.

## Whose area is whose
| Pizzeria | Delivers to |
|---|---|
| **Picolando Verda** | Everything **inside the ring road**: Verdkerno, the suburbs, Norda Kvartalo, the hamlets on the ring. Also the ring itself, with **La Ringo's** bar and grills. **Nobody else delivers inside the ring.** |
| **Picolando Roka** | All of **Ferarbaro**: Roka, **Sovaĝejo** with its mountain, Fajrejo and the country homes. |
| **Picolando Espera** | Altverdo, the north. |
| **Picolando Suda Haveno** | Marĉfluo, the south. |

Porto Luma has no pizzeria. Lumamaro, the east coast, is served by whichever of Espera and Suda
Haveno is nearer. The customers are the island's households (every home with someone living
there), each on the books of the pizzeria whose area it is in.

## Delivering
1. **Sign on** at any pizzeria: walk in and press **E / USE** at the counter. The first order is
   already boxed and goes out with you.
2. **Pick up** each later order from **the pizzeria nearest you**. Pull up in its lay-by and stop
   (or walk in to the counter): the boxes come out in two seconds. An orange beam marks where to
   go, and the compass leads you there.
3. **Deliver to the customer's door.** Walk the boxes to the door, or pull up at the kerb right
   outside and stop. The household pays on the doorstep.
4. The next order comes straight away, from the pizzeria nearest the doorstep.

- **Pay:** 70 kr, plus 110 kr a kilometre by the road. A pizza that arrives **hot** also earns a
  tip of 25 to 70 kr. A cross-town delivery in Verda City pays about 150 to 250 kr. The long
  runs (Roka up into Sovaĝejo's hills, Espera or Suda Haveno out to Porto Luma) pay more.
- **Hot or cold:** the clock starts when the boxes come out. You have 45 s to set off, plus time
  for the distance at about 30 km/h. The panel at the top right counts it down (HOT 1:42).
  A late pizza is **cold**: three quarters of the pay, and no tip.
- **The cars:** Picolando's Kurieroj wait in every lay-by (three at Verda's, two at the others').
  Take any one that's free. Any car, or your own feet, will do too.
- **Who can work:** anyone, but an officer on duty is sent away ("Not in uniform, officer").
- **Quitting:** press USE at a counter twice to hand in your cap. Your deliveries and earnings are
  kept with the career save (`<save>.pizza`).

## The Picolando Kuriero
The chain's delivery car (manifest id `pizza`) is a short, bubbly three-door hatchback you can see
coming a kilometre off:
- tomato-red paint with a white roof;
- a chequered yellow-and-black band all round at the waist;
- a green-white-red flash over the bonnet and yellow wheel arches;
- big headlamps and yellow fog lamps;
- **the giant lit slice on the roof**: a yellow wedge of cheese on its crust, with pepperoni and
  basil, pointing the way it goes, on a glowing light box with chequered sides;
- the hot box in the back, behind the glass.

It drives like a nippy hatchback (40 m/s top speed), and it's in the Creator's vehicle list.
Residents never take one home.

## Picolando's drivers
Three of the chain's own drivers are on the road all day: **Luko** and **Mira** from Verda City's
pizzeria, and **Tonio** from Roka's. Each one:
1. takes their own Kuriero out to a household in their pizzeria's area;
2. stops at the kerb for a few seconds while they run the boxes to the door;
3. drives back to their bay in the lay-by;
4. goes out again.

They drive the roads like the big rigs, kinematically along the lane, slowing for corners, and
holding back for people and cars in their way. Where two roads of different widths meet, they
ease across to the new lane. Take a driver's car and that driver is off the run for the session.
