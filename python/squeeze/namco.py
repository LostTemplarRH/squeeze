from ._squeeze import (
    _decompress_lz80, _compress_lz80,
    _decompress_lz01, _compress_lz01,
    _decompress_lz03, _compress_lz03
)

decompress_lz80 = _decompress_lz80
decompress_lz01 = _decompress_lz01
decompress_lz03 = _decompress_lz03
compress_lz01 = _compress_lz01
compress_lz03 = _compress_lz03

def compress_lz80(binary, window_size=32768):
    return _compress_lz80(binary, window_size)

def compress_lz80_fast(binary):
    return _compress_lz80(binary, 1024)
