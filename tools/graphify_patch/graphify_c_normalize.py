"""C/C++ source normalizer for graphify's tree-sitter extraction.

tree-sitter-c cannot parse several perfectly valid GNU C constructs used
throughout this kernel, which made graphify drop whole files (it reported them
as "syntax errors" and extracted only the file node).  The real cause is that
tree-sitter parses text without running the C preprocessor, so:

  * `} PACKED tss_t;`            -- project attribute macro in a declarator
  * `int buf[N] ALIGNED(16);`    -- attribute macro after an array declarator
  * `EFI_STATUS EFIAPI f(...)`   -- calling-convention macro
  * `__builtin_va_arg(ap, char *)` / `va_arg(ap, T)` -- `T` is a type, not an
    expression, so the call arguments don't parse
  * `list_for_each(pos, head) stmt` -- statement macro that wraps a statement
  * raw `__attribute__((...))`, which tree-sitter-c accepts in most positions
    but *not* after an array declarator

This module rewrites those constructs into equivalent forms tree-sitter does
understand, without changing line or byte offsets (replacements are padded with
spaces, never newlines).  Attributes and calling conventions carry no symbol
information, so blanking them loses nothing the graph needs.

It is installed at runtime (see `sitecustomize.py`) so the installed graphify
package is never modified.
"""

from __future__ import annotations

import re
from pathlib import Path

# Raw GCC attribute: blank the whole balanced `__attribute__((...))`.
_ATTR_RE = re.compile(rb"__attribute__\s*\(")

# Project attribute/calling-convention macros that expand to attributes.
_NAME_RE = re.compile(
    rb"\b(PACKED|NORETURN|UNUSED|NONNULL|EFIAPI|PURE|CONST_FUNC|WARN_UNUSED)\b"
)

# Function-like attribute macro: `ALIGNED(n)`.
_ALIGNED_RE = re.compile(rb"\bALIGNED\s*\(")

# `__builtin_va_arg(ap, TYPE)` and `va_arg(ap, TYPE)`.
_VA_RE = re.compile(rb"(__builtin_va_arg|va_arg)\s*\(([^,()]*),([^()]*?)\)")

# Statement macros that wrap the following statement.
_ITER_CALL_RE = re.compile(
    rb"\b(list_for_each_safe|list_for_each_entry_safe|list_for_each_entry|list_for_each)\s*\("
)


def _blank(buf: bytearray, start: int, end: int) -> None:
    """Replace buf[start:end] with spaces, preserving newlines and length."""
    buf[start:end] = re.sub(rb"[^\r\n]", b" ", bytes(buf[start:end]))


def _match_paren(buf: bytes, i: int) -> int:
    """`buf[i]` is '('. Return the index just past the matching ')'.

    Newlines inside the span are preserved by `_blank`; only offsets change.
    """
    depth = 0
    j = i
    n = len(buf)
    while j < n:
        c = buf[j:j + 1]
        if c == b"(":
            depth += 1
        elif c == b")":
            depth -= 1
            if depth == 0:
                return j + 1
        j += 1
    return n


def _line_start(buf: bytes, pos: int, line_starts: list[int]) -> int:
    """Start offset of the line containing `pos`."""
    lo, hi = 0, len(line_starts)
    while lo < hi:
        mid = (lo + hi) // 2
        if line_starts[mid] <= pos:
            lo = mid + 1
        else:
            hi = mid
    return line_starts[max(0, lo - 1)]


def normalize_c(source: bytes) -> bytes:
    """Return `source` with GNU attribute/macro constructs made parseable."""
    buf = bytearray(source)

    # 1. raw `__attribute__((...))`
    for m in list(_ATTR_RE.finditer(buf)):
        _blank(buf, m.start(), _match_paren(buf, m.end() - 1))

    # 2. `ALIGNED(n)`
    for m in list(_ALIGNED_RE.finditer(buf)):
        _blank(buf, m.start(), _match_paren(buf, m.end() - 1))

    # 3. bare attribute/calling-convention macro names.  Skip preprocessor
    #    directive lines so `#define PACKED ...` keeps its name.
    line_starts = [0] + [i + 1 for i, c in enumerate(buf) if c == 0x0A]
    for m in list(_NAME_RE.finditer(buf)):
        ls = _line_start(buf, m.start(), line_starts)
        if buf[ls:m.start()].lstrip().startswith(b"#"):
            continue
        _blank(buf, m.start(), m.end())

    # 4. va_arg type argument -> `int` padded to the original length, so the
    #    call's second argument is a valid expression.
    for m in _VA_RE.finditer(buf):
        ts, te = m.start(3), m.end(3)
        buf[ts:te] = b"int" + b" " * max(0, (te - ts) - 3)

    # 5. statement macros: blank only the invocation, leaving the wrapped
    #    statement/block in place so its calls are still walked.
    for m in list(_ITER_CALL_RE.finditer(buf)):
        _blank(buf, m.start(), _match_paren(buf, m.end() - 1))

    return bytes(buf)


def install() -> bool:
    """Monkeypatch graphify's generic extractor to normalize C/C++ first.

    `_extract_generic(path, config, *, source_override=None)` already accepts a
    pre-read source, so we can inject the normalized bytes without reimplementing
    extraction.  Both the definition site and the name imported into
    `graphify.extract` must be patched, since `extract_c`/`extract_cpp` hold
    direct references.  Returns True if the patch is in place.
    """
    try:
        import graphify.extract as gext
        from graphify.extractors import engine
    except Exception:
        return False

    if getattr(engine, "_GRAPHIFY_C_NORMALIZE_INSTALLED", False):
        return True

    original = engine._extract_generic

    def _patched(path, config, *, source_override=None):
        if source_override is None and getattr(config, "ts_module", "") in (
            "tree_sitter_c",
            "tree_sitter_cpp",
        ):
            try:
                raw = Path(path).read_bytes()
                normalized = normalize_c(raw)
                if normalized != raw:
                    source_override = normalized
            except Exception:
                pass
        return original(path, config, source_override=source_override)

    engine._extract_generic = _patched
    gext._extract_generic = _patched
    engine._GRAPHIFY_C_NORMALIZE_INSTALLED = True
    return True
