# heatshrink

A data compression/decompression library for embedded/real-time systems.


## Key Features:

- **Low memory usage (as low as 50 bytes)**
    It is useful for some cases with less than 50 bytes, and useful
    for many general cases with < 300 bytes.
- **Incremental, bounded CPU use**
    You can chew on input data in arbitrarily tiny bites.
    This is a useful property in hard real-time environments.
- **Can use either static or dynamic memory allocation**
    The library doesn't impose any constraints on memory management.
- **ISC license**
    You can use it freely, even for commercial purposes.


## Getting Started:

There is a standalone command-line program, `heatshrink`, but the
encoder and decoder can also be used as libraries, independent of each
other. To do so, copy `heatshrink_common.h`, `heatshrink_config.h`, and
either `heatshrink_encoder.c` or `heatshrink_decoder.c` (and their
respective header) into your project. For projects that use both,
static libraries are built that use static and dynamic allocation.

Dynamic allocation is used by default, but in an embedded context, you
probably want to statically allocate the encoder/decoder. Set
`HEATSHRINK_DYNAMIC_ALLOC` to 0 in `heatshrink_config.h`.


### Basic Usage

1. Allocate a `heatshrink_encoder` or `heatshrink_decoder` state machine
using their `alloc` function, or statically allocate one and call their
`reset` function to initialize them. (See below for configuration
options.)

2. Use `sink` to sink an input buffer into the state machine. The
`input_size` pointer argument will be set to indicate how many bytes of
the input buffer were actually consumed. (If 0 bytes were conusmed, the
buffer is full.)

3. Use `poll` to move output from the state machine into an output
buffer. The `output_size` pointer argument will be set to indicate how
many bytes were output, and the function return value will indicate
whether further output is available. (The state machine may not output
any data until it has received enough input.)

Repeat steps 2 and 3 to stream data through the state machine. Since
it's doing data compression, the input and output sizes can vary
significantly. Looping will be necessary to buffer the input and output
as the data is processed.

4. When the end of the input stream is reached, call `finish` to notify
the state machine that no more input is available. The return value from
`finish` will indicate whether any output remains. if so, call `poll` to
get more.

Continue calling `finish` and `poll`ing to flush remaining output until
`finish` indicates that the output has been exhausted.

Sinking more data after `finish` has been called will not work without
calling `reset` on the state machine.


## Configuration

heatshrink has a couple configuration options, which impact its resource
usage and how effectively it can compress data. These are set when
dynamically allocating an encoder or decoder, or in `heatshrink_config.h`
if they are statically allocated.

- `window_sz2`, `-w` in the CLI: Set the window size to 2^W bytes.

The window size determines how far back in the input can be searched for
repeated patterns. A `window_sz2` of 8 will only use 256 bytes (2^8),
while a `window_sz2` of 10 will use 1024 bytes (2^10). The latter uses
more memory, but may also compress more effectively by detecting more
repetition.

The `window_sz2` setting currently must be between 4 and 15.

- `lookahead_sz2`, `-l` in the CLI: Set the lookahead size to 2^L bytes.

The lookahead size determines the max length for repeated patterns that
are found. If the `lookahead_sz2` is 4, a 50-byte run of 'a' characters
will be represented as several repeated 16-byte patterns (2^4 is 16),
whereas a larger `lookahead_sz2` may be able to represent it all at
once. The number of bits used for the lookahead size is fixed, so an
overly large lookahead size can reduce compression by adding unused
size bits to small patterns.

The `lookahead_sz2` setting currently must be between 3 and the
`window_sz2` - 1.

- `input_buffer_size` - How large an input buffer to use for the
decoder. This impacts how much work the decoder can do in a single
step, and a larger buffer will use more memory. An extremely small
buffer (say, 1 byte) will add overhead due to lots of suspend/resume
function calls, but should not change how well data compresses.


### Recommended Defaults

For embedded/low memory contexts, a `window_sz2` in the 8 to 10 range is
probably a good default, depending on how tight memory is. Smaller or
larger window sizes may make better trade-offs in specific
circumstances, but should be checked with representative data.

The `lookahead_sz2` should probably start near the `window_sz2`/2, e.g.
-w 8 -l 4 or -w 10 -l 5. The command-line program can be used to measure
how well test data works with different settings.


## More Information and Benchmarks:

heatshrink is based on [LZSS], since it's particularly suitable for
compression in small amounts of memory. It can use an optional, small
[index] to make compression significantly faster, but otherwise can run
in under 100 bytes of memory. The index currently adds 2^(window size+1)
bytes to memory usage for compression, and temporarily allocates 512
bytes on the stack during index construction (if the index is enabled).

For more information, see the [blog post] for an overview, and the
`heatshrink_encoder.h` / `heatshrink_decoder.h` header files for API
documentation.

[blog post]: http://spin.atomicobject.com/2013/03/14/heatshrink-embedded-data-compression/
[index]: http://spin.atomicobject.com/2014/01/13/lightweight-indexing-for-embedded-systems/
[LZSS]: http://en.wikipedia.org/wiki/Lempel-Ziv-Storer-Szymanski


## Build Status

  [![Build Status](https://travis-ci.org/atomicobject/heatshrink.png)](http://travis-ci.org/atomicobject/heatshrink)
