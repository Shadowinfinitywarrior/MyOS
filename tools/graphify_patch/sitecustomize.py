"""Runtime hook: install the project's C normalizer into graphify.

Put this directory on ``PYTHONPATH`` and Python imports ``sitecustomize`` at
startup, which patches graphify's C/C++ extraction in-process.  Nothing in the
installed graphify package is modified; upgrades are unaffected.

Usage:
    PYTHONPATH=/path/to/myos/tools/graphify_patch graphify extract ...
"""

try:  # pragma: no cover - best effort, never break interpreter startup
    from graphify_c_normalize import install

    install()
except Exception:
    pass
