import sys

try:
    from skbuild import setup
except ImportError:
    print('Please update pip, you need pip 10 or greater,\n'
          ' or you need to install the PEP 518 requirements in pyproject.toml yourself', file=sys.stderr)
    raise

setup(
    packages=['squeeze'],
    package_dir={'': 'python'},
    cmake_source_dir='python',
    cmake_install_dir='python',
)