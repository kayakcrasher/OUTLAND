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
