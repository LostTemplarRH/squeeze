import nox
from pathlib import Path

_ENV = {
    'PYTHONPATH': str(Path(__file__).parent.resolve() / 'python'),
}

@nox.session
def tests(session):
    session.install('pytest')
    session.run('pytest', 'python/tests', env=_ENV)
