# ILBM loader for GIMP

## Prerequisites

Install **gimptool-2.0** to compile and install ```/src/file_ilbm.c``` which implements a GIMP 2.10 loader plugin using the include ```src/libilbm.c``` ILBM parser library. **gimptool-2.0** is available in most if not all package managers that also provide GIMP itself.

## Installation

```
gimptool-2.0 --install src/file-ilbm.c
```

## Particularities

The included *libilbm* library only supports basic core features of the image file ILBM standard. It supports color palettes but not real color bitmaps. It only supports masking by color not by plane.

It does however support basic heuristics to supported ILBM formatted images that were customized by the creators with non-standard chunk names.

### Chunk heuristics

Many commercial software developers used ILBM images in their products, especially early Amiga and DOS game developers who internally used *Deluxe Paint 2* in their art department. To obfuscate the fact and maybe discourage easy modding, they changed standardized chunk identifiers from `FORM` or `IMHD` to something more custom.

When it detects a valid chunk structure in a file but the chunk identifiers do not match up with the standard, *libilbm* will try to make educated guesses to determine the chunks' functions. Hopefully it works around the obfuscation and arrives at a workable interpretation and successfully parse and load the image content.

The GIMP plugin supports detection of standardized ILBM and some obfuscated variants, using GIMP's magic load handler to support files without *.ilbm* extensions.

The magic loader is limited to the official magic identifier and a few known obfuscated magic identifiers. To make sure that GIMP will use *libilbm* to open an unknown file add an `.ilbm` file extension to the image file.

## Source and test images used

The library was development against and tested with the following source ILBM images.

* New ILBM images created with the DOS version of *Deluxe Paint 2*
* Non-obfuscated images found in the `GRAPHICS` directory of Austrian game studio NEO's *The Clue*
* Obfuscated images in the `GFX` directory in the later NEO games *Whale's Voyage* and *Whale's Voyage 2*

# References

* ["EA IFF 85" Standard for Interchange Format Files](https://www.martinreddy.net/gfx/2d/IFF.txt) - Original Electronic Arts standard definition
* [Interleaved Bitmap (ILBM)](https://en.wikipedia.org/wiki/ILBM) - wikipedia.org
* 
