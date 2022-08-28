# Contributing to heatshrink

Thanks for taking time to contribute to heatshrink!

Some issues may be tagged with `beginner` in the issue tracker, those
should be particularly approachable.

Please send patches or pull requests against the `develop` branch.
Changes need to be carefully checked for reverse compatibility before
merging to `master`, since heatshrink is running on devices that may not
be easily recalled and updated.

Sending changes via patch or pull request acknowledges that you are
willing and able to contribute it under this project's license. (Please
don't contribute code you aren't legally able to share.)


## Documentation

Improvements to the documentation are welcome. So are requests for
clarification -- if the docs are unclear or misleading, that's a
potential source of bugs.


## Embedded & Portability Constraints

heatshrink primarily targets embedded / real-time / memory-constrained
systems, so enhancements that significantly increase memory or code
space (ROM) requirements are probably out of scope.

Changes that improve portability are welcome, and feedback from running
on different embedded platforms is appreciated.


## Versioning & Compatibility

The versioning format is MAJOR.MINOR.PATCH.

Performance improvements or minor bug fixes that do not break
compatibility with past releases lead to patch version increases. API
changes that do not break compatibility lead to minor version increases
and reset the patch version, and changes that do break compatibility
lead to a major version increase.

Since heatshrink's compression and decompression sides may be used and
updated independently, any change to the encoder that cannot be
correctly decoded by earlier releases (or vice versa) is considered a
breaking change. Changes to the encoder that lead to different output
that earlier decoder releases handle correctly (such as pattern
detection improvements) are *not* breaking changes.

Essentially, improvements to the compression process that older releases
can't decode correctly will need to wait until the next major release.


## LZSS Algorithm

heatshrink uses the [Lempel-Ziv-Storer-Szymanski][LZSS] algorithm for
compression, with a few important implementation details:

1. The compression and decompression state machines have been designed
   to run incrementally - processing can work a few bytes at a time,
   suspending and resuming as additional data / buffer space becomes
   available.

2. The optional [indexing technique][index] used to speed up compression
   is unique to heatshrink, as far as I know.

3. In general, implementation trade-offs have favored low memory usage.

[index]: http://spin.atomicobject.com/2014/01/13/lightweight-indexing-for-embedded-systems/
[LZSS]: http://en.wikipedia.org/wiki/Lempel-Ziv-Storer-Szymanski


## Testing

The unit tests are based on [greatest][g], with additional
property-based tests using [theft][t] (which are currently not built by
default). greatest tests are preferred for specific new functionality
and for regression tests, while theft tests are preferred for
integration tests (e.g. "for any input, compressing and uncompressing it
should match the original"). Bugs found by theft make for great regression
tests.

Contributors are encouraged to add tests for any new functionality, and
in particular to add regression tests for any bugs found.

[g]: https://github.com/silentbicycle/greatest
[t]: https://github.com/silentbicycle/theft

