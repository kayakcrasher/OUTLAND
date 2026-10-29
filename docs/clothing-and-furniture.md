# Vestaĵejo Verda and furnished interiors

## Vestaĵejo Verda, the clothing store

Verda City's clothing store sells every set of clothes on the island. It takes the three-storey,
18 m Main Street building nearest the city centre. On the map and compass it is the green shop
marked VESTAĴEJO, and its sign over the windows reads "VESTAĴEJO VERDA - VESTOJ · ĈAPELOJ · KOSTUMOJ".

- **What it sells:**
  - Every civilian body's clothes, 44 looks besides your own: men's and women's styles and the
    yellow hazard suits ("Protekta kostumo").
  - La Verda Mano's two outfits: the white shirt with the green hand (400 kr) and the officers'
    charcoal suit (2500 kr). These are sold to members only.
  - Two hats: the Vakeroj's cattleman hat (150 kr) and the patrol's tan campaign hat (300 kr).
  - Other looks cost 180–495 kr.
- **On display:** every look stands on a mannequin.
  - Ground floor: three in the window, a row down the side wall, the counter, three fitting
    rooms, a mirror, a hat stand with both hats, racks and tables of folded clothes.
  - The two floors above: rows of mannequins between racks. The stairs are on the right.
- **Buying:**
  - Stand by a mannequin and press **E** to try on what it wears. At the counter or the fitting
    rooms, **E** browses everything.
  - The panel on the right dresses you as you browse.
  - **LEFT/RIGHT** changes the look, **UP/DOWN** the hat, **ENTER** buys or wears the look,
    **T** the hat, **X** takes the hat off, **ESC** closes. Every button can also be tapped.
  - Closing puts you back in what you own.
  - The clothes you started in are always yours.
- **Saved:** what you own and what you have on are kept in the Explore profile (`WARDROBE` and
  `WEARING` lines).

## Furniture

Furniture models come from `tools/generate_furniture.py` (`assets/verda/furniture/`, 26 pieces,
also in the Creator under Props).

- **Every pub** (Main Street pubs) has:
  - the bar along the back wall, with a brass foot rail, beer taps, a till and a back bar of
    bottles under a mirror;
  - four stools;
  - tables with chairs down one wall, and a darts panel between them;
  - a booth and the jukebox by the stairs;
  - a pool table in the wide pubs, another table in the narrow ones;
  - a plant in the window.
- **Every home somebody lives in.** Empty homes (nobody assigned to them by island life) stay bare.
  - A detached house gets a kitchen with a fridge, a dining table and chairs, a sofa facing the
    television across a rug and a coffee table, a double bed with nightstands, a wardrobe, a
    bookshelf and a plant. Half the houses use the mirrored layout, and some add a floor lamp or
    a second plant, so neighbouring houses don't look identical. The choice is fixed per building.
  - A Main Street flat gets the same on its ground floor.
- **Kept clear:** the doors, the middle of the room (residents walk there) and the stairs.
- **Not furnished:** buildings put together in the Creator (no known shell) and shops.

Pieces are ordinary world assets (`furnish_*`, rebuilt with island life each session) and collide
like other props, except the rug. Layout and the store: `game::interiors::Furnishing`. Catalogue,
buying and wearing: `game::wardrobe::Wardrobe`. The panel: `game::wardrobe::StoreUI`. Tests:
`outland_clothing_and_furniture_tests` and `outland_generated_furniture`.

## Homes for sale

Besides the Stela Turo penthouse, fourteen homes are for sale in Explore:
- six detached houses in Okcidenta Verda, Verda City's west suburb (3,500–7,500 kr);
- two Main Street flats in each of Espera, Porto Luma, Suda Haveno and Roka (1,500–3,000 kr).
  These are the rows furthest from each town's centre.

Each has a red **VENDATA** board with its price out front. The map marks each one as FOR SALE,
or as HOME once it is yours. Nobody lives in a home for sale.

Walk in and press **E / USE** to buy it. The furniture moves in at once: a kitchen, a table, a
sofa and the television, a bed and a wardrobe.

At home:
- **USE** by the bed sleeps till morning and heals you.
- When you die, you wake up at the nearest clinic or the hospital, not at home.

Homes are kept with the Explore wallet. Code: `game::economy::Homes`. Tests:
`outland_homes_for_sale_tests`.

## The hidden stash

Somebody left a holdall with **1,000,000 kronoj** in an empty home on the island: a house nobody
lives in if there is one, otherwise an empty Main Street flat. It is the same home every session
(`game::interiors::stash_site`), never a home for sale. Walk up to the holdall and press
**E / USE** to take the money. It can be taken once per Explore wallet (`game::economy::Stash`).
With the default map it is on the ground floor of an empty Main Street flat in Porto Luma, near
(1816, -419).
