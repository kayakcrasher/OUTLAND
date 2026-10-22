# Armilejo: the gun shops

**Armilejo** ("armoury") is Verda's gun shop chain. There is one shop in each of the five main
cities: Verda City, Espera, Porto Luma, Suda Haveno and Roka. The nearest one shows on the
compass as ARMILEJO.

## The shops

Each shop is a one-storey building from the city kit (`civic/gun_shop.glb`):

- Olive walls, a brass sign band (ARMILEJO · PAFILOJ · MUNICIO · KIRASOJ), and a glass door with
  two barred display windows.
- Inside: a long counter with a glass case, rifles racked on the back wall, shelves of
  ammunition, and a stand of vests.

The world kit (`gun_shops` in `src/dev/VerdaWorldKit.cpp`) places the shops by a paved street
near each city's centre.

## What they sell

Walk up to the door and press USE to trade, as at any shop. Armilejo sells every gun in the
game, ammunition for each, grenades and armour. Prices are in kronoj (buy / sell back):

| Item | Buy | Sell |
| --- | --- | --- |
| V9 pistol | 180 | 70 |
| VD2 double barrel | 380 | 150 |
| VS12 shotgun | 520 | 210 |
| VK45 SMG | 650 | 260 |
| Hunting rifle | 700 | 280 |
| VA12 auto shotgun | 900 | 360 |
| VP7 sniper rifle | 1400 | 560 |
| VR7 rocket launcher | 3000 | 1200 |
| 9mm / .45 / 12-gauge / rifle / 7.62 rounds | 2 / 3 / 4 / 4 / 8 each | |
| Rockets | 150 | 60 |
| Frag grenade | 60 | 25 |
| Light vest / plate carrier | 300 / 750 | 120 / 300 |

Stock is limited and restocks over time (`assets/verda/economy/offers.tsv`, kind `guns`).

## Starting out

A new campaign or Explore game starts with only the **V9 pistol**, 60 rounds of 9mm, a little
food and water, and a bandage. Anything bigger comes from Armilejo, or from what you find.
