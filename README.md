# tilequant
Tile-based Image Quantization Tool

## Purpose
This tool is mostly meant for GBA/NDS graphics, where each 'tile' can use one of many palettes. However, it can be adapted to just about any use (for example, custom formats).

## Getting started
Run `make` to build the tool, then call `tilequant Input.bmp Output.bmp (no. of palettes) (entries/palette)`

## Possible issues

There are some pathological cases that will slow the quantizer to a crawl (owing to subnormals).

## Authors
* **Ruben Nunez** - *Initial work* - [Aikku93](https://github.com/Aikku93)
