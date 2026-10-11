# Verda: the island

## Canon

These points come from the game's creator.

- **Verda is a mini nation that speaks Esperanto.** Its people call themselves *Esperantistoj*. As much of the world's writing as possible is in Esperanto.
- **The people are Slavic and Anglo.** Their first names are Esperantised (Petro, Johano, Ludmila). Their family names are Slavic or English (Novak, Kovač, Walker, Fletcher).
- **The whole island was a Cold War experiment that was forgotten.** Nobody came back for it. It carried on and survived on its own.
- **The island is strange.** That strangeness is why everything happens here: the battle royales, the zombies, and an everyday life that carries on as if neither were unusual.
- **The flag looks like Cuba's but is green like the Esperanto flag.**

## The flag

The Verda flag has Cuba's layout in Esperanto colours:
- five horizontal stripes, green, white, green, white, green;
- a white equilateral triangle at the hoist, with a green five-pointed star in it.

This mirrors the Esperanto flag's white canton with its green star. A thin green rim keeps the triangle apart from the white stripes.

The green is Esperanto green, `#009900`. The flag is drawn by `game::culture::flag_image()` in `src/game/culture/Signage.cpp`, and the colours are constants there, so a different design is a small change.

## Working details

These are proposals that fill gaps in the canon. Change them freely.

- **The founding year is 1961.** The courthouse square plaque reads "PLACO DE LA RESPUBLIKO, FONDITA 1961".
- **Instituto 47** is one of the capital's office signs, a nod to whatever the experiment really was.
- **Cold War leftovers** show up in the signs:
  - a numbered factory ("FABRIKO N-RO 7") and a works co-op;
  - a *kolĥozo* (collective farm) at Espera;
  - Radio Verda and a weather institute;
  - a pub called *La Forgesita Insulo*, "The Forgotten Island".
- **Faction names.** *Krimulo* means "criminal", and the Green Star Federation is *Verda Stelo* in Esperanto.

### What the place names mean

| Place | Meaning |
|---|---|
| Verda (the capital, and the island) | green |
| Porto Luma | the shining port |
| Suda Haveno | south harbour |
| Roka | rocky |
| Espera | hopeful |
| Sovaĝejo | the wild place |

## The five zones (zonoj)

Verda is split into five zones, each grown round one of its towns. Their names and borders show on the map (**M**, or the MAP button), and a banner names each zone as you enter it.

| Zone | Meaning | Seat | Character |
|---|---|---|---|
| **Verdkerno** | Green Core | Verda | The capital, farms, administration and infrastructure: the heart of the nation |
| **Altverdo** | High Green | Espera | Northern highlands: forests, hills, hunting, sparse settlements, colder ground |
| **Lumamaro** | Sea of Light | Porto Luma | The bright eastern coast: the port, beaches, markets, the lighthouse |
| **Marĉfluo** | Marsh Flow | Suda Haveno | Southern wetlands: rivers, marshes, reeds, fishing, floodplain life |
| **Ferarbaro** | Iron Forest | Roka, Sovaĝejo | The western backcountry: gravel roads, trailers, mines, self-reliant people |

### Ferarbaro and Sovaĝejo

Ferarbaro is Verda's backcountry, and **Sovaĝejo** (the wild place) is its heart. These are Esperantists who live farther from the capital, where paved roads give way to gravel and dirt.

- **The land:** trailers sit under trees, and old cars stay alive through sheer stubbornness.
- **The people:**
  - Everybody seems to know somebody who can weld, hunt, repair an engine, reload ammunition or get a generator running.
  - They are not "poor dumb country people". They are mechanically competent, suspicious of bureaucracy, tight-knit, proud, funny and deeply local.
  - To them, people from Verda City are soft: they call a mechanic instead of crawling underneath the car themselves.
- **The look is improvised, not ruined.** Things look patched because people keep them going.
  - Homes are trailers, small shacks, lean-tos and workshops.
  - Yards hold a car that runs, with a parts donor or two on blocks beside it, plus rain barrels, firewood, pallets and fire pits.
  - "Why would I replace it? It still starts."
- **Politics grew out of lived reality.** People settled far apart, the roads were bad and police were slow, so problems got solved locally.
  - Their creed: *Verda belongs to Verdans. My land belongs to me. Your land belongs to you. Help your neighbour. Leave people alone.*
  - They want Verda to stay a small independent nation. Foreign pressure makes them suspicious, and so does Verda City growing too powerful.
- **Hand-painted boards along the back roads say so:**

  | Sign | Meaning |
  |---|---|
  | VERDA ESTAS LIBERA | Verda is free |
  | LASU NIN VIVI | Let us live |
  | NENIU MASTRO | No master |
  | VERDA APARTENAS AL VERDANOJ | Verda belongs to Verdans |
  | MIA TERO ESTAS MIA. VIA TERO ESTAS VIA. | My land is mine. Your land is yours. |
  | HELPU VIAN NAJBARON. LASU HOMOJN EN PACO. | Help your neighbour. Leave people alone. |
  | RIPAROJ · MOTOROJ · VELDADO | Repairs, engines, welding |
  | FREŜAJ OVOJ | Fresh eggs |
  | ATENTU LA HUNDON! | Beware of the dog! |

## Language in the game

- **The world is written in Esperanto:**
  - shopfronts;
  - town signs and distance signs;
  - street names;
  - bus stops;
  - notices.
  
  See [esperanto-signage.md](esperanto-signage.md).
- **Menus and the HUD stay in English** so players can follow them. The one exception is the home screen's greeting, *Bonvenon al Verda*.
- **Esperanto letters:** ĉ ĝ ĥ ĵ ŝ ŭ need the sign font (`assets/fonts/DejaVuSans-Bold.ttf`). raylib's built-in font doesn't have them.
