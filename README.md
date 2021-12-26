# pngblank

The pngblank utility generates fully transparent PNG images.

## Contents

1. [Install](#install)
2. [Instructions](#instruction)
3. [License](#license)

## Install

### Requires

* C compiler ;
* libz.

### Build

To install globally:

    $ ./configure
    $ make
    # make install

To install in your `$HOME/bin`:

    $ ./configure PREFIX=~
    $ make
    $ make install

## Instructions

To see a description of its options see the [man](./pngblank.md) page.

`pngblank` can generates images using various colour modes, bit depths and compression level.

For a 2,5 kilobytes monstrosity one can type:

    $ pngblank -l 1 -s huffmanonly 80 > big.png
    $ ls -ngh big.png
    -rw-r--r--  1 0   2.5K Apr 24 12:43 big.png

But if you prefer your files small and lean `pngblank` got you covered too:

    $ pngblank -g -b 1 80 > small.png
    $ ls -ngh small.png
    -rw-r--r--  1 0    88B Apr 24 12:43 small.png

## License

All the code is licensed under the ISC License.
It's free, not GPLed !
