#!/usr/bin/env python3
"""Flatten the verbose <UnitAI><UnitAIType>X</UnitAIType><bUnitAI>1</bUnitAI></UnitAI>
membership idiom into a plain <UnitAI>X</UnitAI> list, in-place across all unit XML.

Part of the data-loading-coherence sweep: the inner <bUnitAI> flag is degenerate
(always 1; <bUnitAI>0</bUnitAI> never occurs), so membership is fully captured by
presence in the list. The flat form is what CvInfoUtil's SetOptionalVector reads and
what a generic XML->JSON pass can consume without per-field special-casing.

An entry whose flag is 0 (none exist today, but be correct) is DROPPED, since 0 means
"not a member". Bytes are round-tripped as latin-1 (iso-8859-1) to preserve encoding
and CRLF line endings exactly.

Usage:  python flatten_unitai_lists.py [--dry-run] [root1 root2 ...]
Default roots: Assets/XML, Assets/Modules (relative to repo root).
"""
import os
import re
import sys

# Matches one verbose entry, capturing indent of <UnitAI>, the type, and the flag value.
ENTRY = re.compile(
    r'([ \t]*)<UnitAI>\s*'
    r'<UnitAIType>\s*([A-Za-z0-9_]+)\s*</UnitAIType>\s*'
    r'<bUnitAI>\s*(\d+)\s*</bUnitAI>\s*'
    r'</UnitAI>',
    re.IGNORECASE,
)


def convert(text):
    count = [0]

    def repl(m):
        indent, type_str, flag = m.group(1), m.group(2), m.group(3)
        count[0] += 1
        if int(flag) == 0:
            return None  # sentinel; handled below
        return "%s<UnitAI>%s</UnitAI>" % (indent, type_str)

    # Two-step so we can drop flag==0 entries (and their trailing newline) cleanly.
    out, n = [], 0
    pos = 0
    for m in ENTRY.finditer(text):
        out.append(text[pos:m.start()])
        indent, type_str, flag = m.group(1), m.group(2), m.group(3)
        n += 1
        if int(flag) == 0:
            # Drop the entry and the line's trailing newline, leaving no blank line.
            end = m.end()
            nl = re.match(r'[ \t]*\r?\n', text[end:])
            pos = end + (nl.end() if nl else 0)
        else:
            out.append("%s<UnitAI>%s</UnitAI>" % (indent, type_str))
            pos = m.end()
    out.append(text[pos:])
    return "".join(out), n


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    dry = "--dry-run" in sys.argv[1:]
    repo = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
    roots = args or [os.path.join(repo, "Assets", "XML"),
                     os.path.join(repo, "Assets", "Modules")]

    total_files, total_entries = 0, 0
    for root in roots:
        for dirpath, _, names in os.walk(root):
            for name in names:
                if not name.lower().endswith(".xml"):
                    continue
                path = os.path.join(dirpath, name)
                with open(path, "rb") as f:
                    raw = f.read()
                text = raw.decode("latin-1")
                if "<bUnitAI>" not in text:
                    continue
                new, n = convert(text)
                if n == 0 or new == text:
                    continue
                total_files += 1
                total_entries += n
                rel = os.path.relpath(path, repo)
                print("%5d  %s" % (n, rel))
                if not dry:
                    with open(path, "wb") as f:
                        f.write(new.encode("latin-1"))

    print("\n%s: %d entries across %d files%s" % (
        "Would convert" if dry else "Converted",
        total_entries, total_files, " (dry run)" if dry else ""))


if __name__ == "__main__":
    main()
