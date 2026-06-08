#!/usr/bin/env python3
"""Flatten verbose bool-paired membership lists into plain type lists, in-place across all XML.

The idiom <Wrapper><SomeType>X</SomeType><bSomething>1</bSomething></Wrapper> encodes set
membership as a degenerate type->bool map (the bool is always 1). The flat form <Wrapper>X</Wrapper>
is what CvInfoUtil's SetOptionalVector reads and what a generic XML->JSON pass can consume without
per-field special-casing.

Targets are identified by their WRAPPER element tag (e.g. UnitAI, TerrainMakesValid), which is unique
per list, so only the intended structures are touched. An entry whose flag is 0 (rare) is DROPPED,
since 0 means "not a member". Bytes round-trip as latin-1 to preserve encoding + CRLF exactly.

Usage:  python flatten_bool_lists.py [--dry-run] Wrapper1 [Wrapper2 ...]
Example: python flatten_bool_lists.py TerrainMakesValid FeatureMakesValid
"""
import os
import re
import sys


def entry_re(wrapper):
    # <Wrapper> <anyTypeTag>VALUE</anyTypeTag> <bAnyTag>flag</bAnyTag> </Wrapper>
    return re.compile(
        r'([ \t]*)<%s>\s*'
        r'<[A-Za-z0-9_]+>\s*([A-Za-z0-9_]+)\s*</[A-Za-z0-9_]+>\s*'
        r'<b[A-Za-z0-9_]+>\s*(\d+)\s*</b[A-Za-z0-9_]+>\s*'
        r'</%s>' % (wrapper, wrapper),
        re.IGNORECASE,
    )


def convert(text, wrappers):
    total = 0
    for w in wrappers:
        pat = entry_re(w)
        out, pos, n = [], 0, 0
        for m in pat.finditer(text):
            out.append(text[pos:m.start()])
            indent, type_str, flag = m.group(1), m.group(2), m.group(3)
            n += 1
            if int(flag) == 0:
                end = m.end()
                nl = re.match(r'[ \t]*\r?\n', text[end:])
                pos = end + (nl.end() if nl else 0)
            else:
                out.append("%s<%s>%s</%s>" % (indent, w, type_str, w))
                pos = m.end()
        out.append(text[pos:])
        text = "".join(out)
        total += n
    return text, total


def main():
    argv = sys.argv[1:]
    dry = "--dry-run" in argv
    wrappers = [a for a in argv if not a.startswith("--")]
    if not wrappers:
        print("usage: flatten_bool_lists.py [--dry-run] Wrapper1 [Wrapper2 ...]")
        return
    repo = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
    roots = [os.path.join(repo, "Assets", "XML"), os.path.join(repo, "Assets", "Modules")]

    needles = ["<%s>" % w for w in wrappers]
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
                if not any(nd in text for nd in needles):
                    continue
                new, n = convert(text, wrappers)
                if n == 0 or new == text:
                    continue
                total_files += 1
                total_entries += n
                print("%5d  %s" % (n, os.path.relpath(path, repo)))
                if not dry:
                    with open(path, "wb") as f:
                        f.write(new.encode("latin-1"))

    print("\n%s: %d entries across %d files%s" % (
        "Would convert" if dry else "Converted",
        total_entries, total_files, " (dry run)" if dry else ""))


if __name__ == "__main__":
    main()
