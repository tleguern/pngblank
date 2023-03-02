PNGBLANK(1) - General Commands Manual

# NAME

**pngblank** - generate images

# SYNOPSIS

**pngblank**
\[**-gnp**]
\[**-b**&nbsp;*bitdepth*]
\[**-c**&nbsp;*library*]
\[**-l**&nbsp;*level*]
\[**-s**&nbsp;*strategy*]
*width*

# DESCRIPTION

The
**pngblank**
utility generates fully transparent, square PNG images in various way.
By default the images are generated using true colours and a bit depth of 8.

The options are as follows:

**-g**

> Set the colour type to grayscale.

**-n**

> Do not generate an image, print its size in bytes instead.

**-p**

> Set the colour type to indexed.

**-b** *bitdepth*

> Set the bitdepth to a specific value.

**-c** *library*

> Set the compression library.
> Accept zlib or libdeflate, default is zlib.

**-l** *level*

> Set the compression level, the default value depends on the compresion library.

**-s** *strategy*

> Set the compression strategy (only valid for zlib).

# EXIT STATUS

The **pngblank** utility exits&#160;0 on success, and&#160;&gt;0 if an error occurs.

# SEE ALSO

pnginfo(1)

# STANDARDS

*Portable Network Graphics (PNG) Specification (Second Edition)*,
10 November 2003.

# AUTHORS

The
**pngblank**
utility was written by
Tristan Le Guern &lt;[tleguern@bouledef.eu](mailto:tleguern@bouledef.eu)&gt;.

OpenBSD 7.2 - April 23, 2020
