import squeeze
import pytest
from pathlib import Path
import urllib.request

_CORPUS = {
    'jquery': {
        'url': 'https://code.jquery.com/jquery-3.6.1.slim.min.js',
        'file': 'jquery.min.js',
    },    
    'tod2_cover': {
        'url': 'https://upload.wikimedia.org/wikipedia/en/d/dc/Tod2cover.jpg',
        'file': 'tod2cover.jpg',
    },
}

@pytest.fixture
def compression_corpus():
    download_dir = Path('.download')
    download_dir.mkdir(parents=True, exist_ok=True)
    corpus = {}
    for name, asset in _CORPUS.items():
        asset_file = download_dir / asset['file']
        if not asset_file.exists():            
            print(f'Downloading test data {name}...')
            urllib.request.urlretrieve(asset['url'], asset_file)
        corpus[name] = asset_file
    return corpus

def test_lz01(compression_corpus):
    data = compression_corpus['tod2_cover'].open('rb').read()
    compressed = squeeze.namco.compress_lz01(data)
    decompressed = squeeze.namco.decompress_lz01(compressed)
    assert decompressed == data

def test_lz03(compression_corpus):
    data = compression_corpus['tod2_cover'].open('rb').read()
    compressed = squeeze.namco.compress_lz03(data)
    decompressed = squeeze.namco.decompress_lz03(compressed)
    assert decompressed == data
