# tilequant
Tile-based Image Quantization Tool

## Purpose
This tool is mostly meant for GBA/NDS graphics, where each 'tile' can use one of many palettes. However, it can be adapted to just about any use (for example, custom formats).

## Getting started
Run `make` to build the tool, then call `tilequant Input.bmp Output.bmp -np:(no. of palettes) -ps:(entries/palette)` (eg. `tilequant Input.bmp Output.bmp -np:16 -ps:16` to use all sixteen 16-colour GBA palettes).

## Examples

All conversions performed with `-tilepasses:500 -colourpasses:500 -dither:ord8`.

| Palettes | Result |
| - | - |
| Baseline truth | ![Baseline truth](/cat.png?raw=true) |
| `-np:1` | ![1 palette](/cat-q1.png?raw=true) |
| `-np:2` | ![2 palettes](/cat-q2.png?raw=true) |
| `-np:4` | ![4 palettes](/cat-q4.png?raw=true) |
| `-np:8` | ![8 palettes](/cat-q8.png?raw=true) |
| `-np:16` | ![16 palettes](/cat-q16.png?raw=true) |

## Authors
* **Ruben Nunez** - *Initial work* - [Aikku93](https://github.com/Aikku93)
* **Marco Köpcke** - *Modifications and motivation for DLL interface* - [Parakoopa](https://github.com/Parakoopa)
* **zvezdochiot** - *Code and git cleanup* - [zvezdochiot](https://github.com/zvezdochiot)
