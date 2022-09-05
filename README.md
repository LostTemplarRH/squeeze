# squeeze

A library that provides facilities to implement custom LZSS decompressors and compressors.

## Motivation

Many older games utilize some variant of LZSS compression.
They usually differ in how they encode a back reference, how large their sliding window is, whether they also support RLE (run-length encoding), and more.
While implementing a decompressor is usually straight-forward, getting a compressor to work fast and correctly can be tricky, which is why I wanted to have a general, reusable implementation.

TODO: Design explanation

## Examples

The `examples` directory contains example implementations of LZSS variants from some games.
For convenience, these compressors and decompressors are exposed both in a command-line interface `squeeze-cli`, as well as a Python module also called `squeeze`.

## Python library

So far, only the LZSS compressors and decompressors from `examples` are exposed here, namespaced using the publisher corresponding to the game they were used in.
For example:
```Python
import squeeze
data = bytes(...)
decompressed = squeeze.namco.decompress_lz03(data)
```

TODO: Expose compressor/decompressor construction interface.