# Stela Turo

Verda City's art deco skyscraper, downtown in the tower core: the inner corner of the block
south-west of the city centre (at -26, -26), facing the main avenue, where the old stone deco
tower stood. Model: `assets/verda/city/towers/stela_turo.glb`, written by
`tools/generate_city_kit.py` (`stela_turo()`); placed by the world kit (`downtown()` in
`src/dev/VerdaWorldKit.cpp`, its offices and loot in `government()`) and in `maps/verda_world.map`.

## The building
- **17 storeys** of dressed limestone in three setbacks: 30 x 24 m to the 11th floor, 25 x 20 m to the
  15th, 21 x 17 m to the top, then a stepped crown with gold fins and a gold spire (86 m).
- Piers run up between the windows and rise past each parapet with gold caps; bronze chevron
  spandrels between the floors; a gold-edged sill under every window; a dark granite plinth.
- Entrances front and back: gold surrounds with a sunburst and a bronze marquee. Each is one open
  doorway 6 m wide inside the gold frame, the plinth broken for it and a low granite step up to
  the lobby: walk straight in off the pavement. The sign over the door reads
  **STELA TURO - OFICEJOJ · PENTDOMO**.
- **Every window is open** (no glass). **Every floor is open plan** round four columns, on a
  terrazzo floor with brass strips.
- **Two stair cores**, one at each side: switchback flights (0.2 m risers), bronze rails round
  each floor's stair hole. The left core climbs to the penthouse; the right one goes on up to a
  stair house on the roof.
- The setback roofs (12th and 16th floors) are terraces behind parapets, through doors in the front.
- Loot on every other floor (general, weapons, medical) for Battle Royale.

## The penthouse (Explore)
The 17th floor is a furnished suite on a black-and-white marble floor: a living room with a velvet
sofa, rug and coffee table, a dining table, a kitchen bar, a grand piano, and behind a wall with two
doorways a bedroom (a bed with a gold headboard) and a bathroom.

In Explore it is **for sale for 5,000 kronoj**: walk up the stairs, and anywhere on the floor
**E / USE** buys it (the prompt says the price; without enough money it tells you how much you
have). Once it is yours (`game::economy::Penthouse`, kept with the Explore wallet as a `PROPERTY`
in the profile):
- **Sleep**: E / USE by the bed sleeps until 7:00 and mends you.
- **Wake up at home**: when you die in Explore you wake in the penthouse instead of the hospital.
- The map shows it as a gold house, **HOME - STELA TURO** (while for sale: **STELA TURO (FOR SALE)**).
- The roof is yours too: the right-hand stairs go on up to the stair house on the roof.

Dev: `OUTLAND_DEV_SPAWN="x,z,yaw,height"` (the new optional height) starts on an upper floor, e.g. the
penthouse.
