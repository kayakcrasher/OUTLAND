# Churches

Every town has a church, and Verda City has two (`game::culture::Faith`; placed by
`churches()` in the world kit).

| | West: Baptist | East: Esperanto Orthodox |
|---|---|---|
| Towns | Roka, Sovaĝejo, Verda City (the west boulevard) | Espera, Porto Luma, Suda Haveno, Verda City (the east suburbs) |
| Building | `civic/church_baptist.glb`: a white clapboard chapel, 10 x 18 m, a shingle roof, a portico over the green door, stained-glass windows, an open belfry with its bell under a white spire | `civic/church_orthodox.glb`: white walls under a green tin roof, a gold onion dome on a tall drum and four blue onions at the corners, each with its cross; a porch with an icon over the door |
| Inside | two banks of pews, the pulpit on a low dais, the baptistry pool behind it, a plain cross | open (the faithful stand): candle stands, benches along the walls, the gold iconostasis with its icons and royal doors |
| Patron saints | Petro, Andreo, Patriko, Kolumbo | Nikolao, Sergio, Cirilo kaj Metodio, Sofia |

## Who worships where

The west of the island is the Anglo side of its people, and Baptist; the east is the Slavic side,
and Esperanto Orthodox. Verda City has both. **La Verda Mano are Baptists** - the Ulster banner's
people - and the city's chapel on the west boulevard is theirs: their banner flies by its door,
whoever holds the town.

Verdan Baptists are not like Baptists anywhere else. Centuries beside the Orthodox have left them
keeping the saints - every chapel has its patron - and the same feasts on the same days. What
they hold to is the Book: **la Reĝo-Jakoba Biblio**, the King James in Esperanto, open on every
lectern.

## The feasts

Both churches keep the same feasts, one every four island days, in turn: Kristnasko, Teofanio (the
waters blessed), Pasko, Pentekosto, Sankta Petro kaj Paŭlo (the boats blessed in the harbours),
Sankta Mikaelo, Sankta Nikolao (gifts for the children) and Zamenhof-Tago, the Esperantists' own
day.

## In the game

- Walk up to a church's door and a line says whose it is, its saint, and today's feast or the next
  one ("BAPTISTA PREĜEJO DE SANKTA PETRO. Verdan Baptists: they keep the saints and the same feasts
  as the Orthodox, and read the Esperanto King James. La Verda Mano worships here. Next feast:
  Teofanio in 3 days.").
- Each church is a Church place: the town's pastor works there and its churchgoers go to the
  nearest one on Sunday mornings (`game::life`).
- Tests: `tests/unit/culture/test_faith.cpp` (the feasts, the notices) and the world kit test
  (a church in every town, Baptists west, Orthodox east, both in Verda City).
