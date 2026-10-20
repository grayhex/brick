#!/usr/bin/env python3
"""Inlines the WebAssembly module into the simulator page: one self-contained HTML file.

    python3 make_page.py index.html brick-sim.wasm brick-sim.html [--fragment]

--fragment drops the <!doctype>/<html>/<head>/<body> wrappers and the charset/viewport metas (what a host that wraps the
page itself, such as the Artifact publisher, expects); everything else is identical.
"""
import re
import base64
import sys
from pathlib import Path


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    template, wasm, out = (Path(a) for a in args[:3])
    html = template.read_text(encoding="utf-8")
    marker = "__WASM_B64__"
    if html.count(marker) != 1:
        raise SystemExit(f"{template}: expected exactly one {marker}")
    if "--fragment" in sys.argv:
        html = re.sub(r"<!doctype html>\s*|<html[^>]*>\s*|<head>\s*|</head>\s*|<body>\s*|</body>\s*|</html>\s*", "", html, flags=re.I)
        html = re.sub(r'<meta charset="utf-8">\s*|<meta name="viewport"[^>]*>\s*', "", html)
    out.write_text(html.replace(marker, base64.b64encode(wasm.read_bytes()).decode("ascii")), encoding="utf-8")
    print(f"{out}: {out.stat().st_size} bytes")


if __name__ == "__main__":
    main()
