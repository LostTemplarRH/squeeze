from ._squeeze import _decompress_lz80, _compress_lz80

decompress_lz80 = _decompress_lz80

def compress_lz80(binary, window_size=32768):
    return _compress_lz80(binary, window_size)

def compress_lz80_fast(binary):
    return _compress_lz80(binary, 1024)