# Porto Luma's harbour

Porto Luma's cargo terminal is where a new campaign begins. The world kit's `cargo_terminal()`
(`src/dev/VerdaWorldKit.cpp`) builds it. The ship's timetable and the arrival area's layout are in
`world::harbour` (`include/outland/world/Harbour.hpp`). The models come from
`tools/generate_city_kit.py` (the "Porto Luma's arrivals" section, and the cranes and the ship).

## Stella Maris
The feeder ship that brings the island its supplies calls **once a week**:

| Monday | |
|---|---|
| 04:00–05:00 | In from the open sea. She comes bow first, swings round in the basin and is pushed sideways onto her berth. |
| 05:00–12:00 | Alongside the cargo quay, her brow down to the quay and the cattle gangway up her side. |
| 12:00–13:00 | Off the berth sideways (her tugs), then round to the north-east and away over the horizon. |

For the rest of the week she is at sea, and her berth is empty. One island hour is two real
minutes.

**The first day.** On a new game she waits alongside, whatever the hour, while the player comes
down her brow. She sails as soon as the player starts talking to Uncle Ron, and is gone before he
has finished. The next Monday she is back on her timetable.

The ship and her two gangways are still map assets at her berth: Verda Transporto's terminal and
the cattle quay find them there. The game draws them where the timetable has her
(`world::harbour::at`). Her route stays in deep water all the way.

## The quay
- **Cranes.** The two ship-to-shore cranes are solid.
  - Their legs stand on rails either side of the quay's edge strip, 14 m apart, clear of the quay
    lane.
  - Each leg rides on its own bogie.
  - The way between the legs, under the portal, is open on foot. The legs are met by their real
    shape (`WorldCollision::elevated`).
- **Concrete.** No grass grows on the quay's concrete any more (`layout::on_quay`).
- **Arrivals.**
  - The ship's brow comes down from her side at the passenger door aft.
  - **Alvenejo**, the arrival hall, stands on the quay: glazed, with a door at each end under a
    canopy, benches and the customs desk inside. It has "ALVENEJO" over the door from the ship and
    "BONVENON AL VERDA" over the way out. Customs officers work there.
  - A zebra crosses the quay lane.
  - The **waiting area** has a pick-up bay where Uncle Ron's truck waits, a shelter, a bench, a
    lamp, the national flag and an ATENDEJO board.
- **The gate.** Over the terminal road before the ramp down: "HAVENO DE PORTO LUMA" on a gantry,
  the gatehouse beside it and the barrier raised. Rigs drive in under it; its posts are solid.

## Tests
`outland_harbour_tests` checks:
- the timetable: alongside on Monday mornings, under way at half past twelve, away all week, back
  every Monday, never jumping, always in deep water;
- the opening: held alongside, away once Ron starts talking, back the next Monday;
- the cranes: solid, with the way between their legs open and the quay lane clear for trucks;
- the walk from the brow through the arrival hall and over the zebra to the waiting area;
- the gate.
