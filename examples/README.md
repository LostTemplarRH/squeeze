# Examples

This directory contains example LZSS implementations using `squeeze` for "real" use cases in retro games.
The examples are ordered by publisher/developer.

# Namco

There are three LZSS variants implemented:
- LZ80 (used, for example, by Tales of Innocence R).
    This variant uses multi-byte encodings for back references and thus demonstrates the usage of multiple match classses.
- LZ01 and LZ03 (used, for example, by Tales of Destiny 2).
    These two only differ in that LZ03 also supports RLE (run-length encoding).
    Note that these implementation only decode the compressed data and do not process the header.