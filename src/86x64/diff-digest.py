#!/usr/bin/env python3
"""Summarise a working-tree diff by MEANING instead of by line.

Line-based diff is a terrible encoding for the two artifact shapes this repo
generates in bulk:

  * sorted JSON maps (shimdb/observed.json) -- git interleaves the boilerplate
    ("}," / '"stub": "function"') of neighbouring records, so a purely ADDITIVE
    change reports thousands of phantom deletions;
  * generated C/ObjC shims (shimdb/generated/*ShimAuto.m) -- what matters is
    which functions appeared or vanished, not the 500 lines of body.

Both are dispatched on STRUCTURE (does the JSON parse to a dict-of-dicts? does
the source contain function-like definitions?), never on filename, so this keeps
working for artifacts that do not exist yet.

Untracked files are included: `git diff` cannot see them, which is how ten new
libsvn_*ShimAuto.m files stayed invisible in the status above.

Usage:
    diff-digest.py [--base REV] [--limit N] [--full] [paths...]
"""

from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
from collections import Counter

C_SUFFIXES = (".c", ".m", ".mm", ".cc", ".cpp", ".h", ".hpp")

# Trailing identifier of the text preceding a definition's open paren --
# `long AbortPrePrerollMovie` -> AbortPrePrerollMovie.
NAME_RE = re.compile(r"([A-Za-z_]\w*)\s*$")

# Keywords that start a column-0 line looking like a call but are not one.
NOT_A_DEF = {"if", "for", "while", "switch", "return", "else", "do", "sizeof"}


def func_names(text: str) -> set[str]:
    """Names of function-like DEFINITIONS at column 0.

    Handles both styles the repo generates:
        long Foo(args) { shim_note("_Foo"); return 0; }   -- brace on the line
        long Foo(args)                                    -- brace on the next
        {
    and skips the paired `__asm` declarations, which carry no brace at all.
    Deliberately loose: we only need names to set-diff, and a stray miss is far
    cheaper than paging the raw hunk into context.
    """
    lines = text.splitlines()
    found: set[str] = set()
    for i, line in enumerate(lines):
        if not line or line[0].isspace() or line[0] == "#":
            continue
        paren = line.find("(")
        if paren <= 0:
            continue
        before = line[:paren]
        match = NAME_RE.search(before)
        if not match or match.group(1) in NOT_A_DEF:
            continue
        close = line.find(")", paren)
        if close < 0:
            continue
        tail = line[close + 1:]
        # A definition either opens its brace on this line, or the next
        # non-empty line is the brace. Anything else is a declaration.
        if "{" in tail:
            found.add(match.group(1))
            continue
        if tail.strip():
            continue
        nxt = next((l for l in lines[i + 1:] if l.strip()), "")
        if nxt.lstrip().startswith("{"):
            found.add(match.group(1))
    return found


def run(args: list[str]) -> str:
    """Run a git command, returning stdout ('' on failure)."""
    proc = subprocess.run(args, capture_output=True, text=True)
    return proc.stdout if proc.returncode == 0 else ""


def repo_root() -> str:
    root = run(["git", "rev-parse", "--show-toplevel"]).strip()
    if not root:
        sys.exit("diff-digest: not inside a git repository")
    return root


def changed_paths(base: str, only: list[str]) -> list[tuple[str, str]]:
    """Return (status, path) for tracked changes plus untracked files."""
    out: list[tuple[str, str]] = []
    cmd = ["git", "diff", "--name-status", base]
    if only:
        cmd += ["--"] + only
    for line in run(cmd).splitlines():
        parts = line.split("\t")
        if len(parts) >= 2:
            out.append((parts[0][0], parts[-1]))

    cmd = ["git", "ls-files", "--others", "--exclude-standard"]
    if only:
        cmd += ["--"] + only
    for path in run(cmd).splitlines():
        if path:
            out.append(("?", path))
    return sorted(out, key=lambda sp: sp[1])


def blob(base: str, path: str) -> str:
    return run(["git", "show", f"{base}:{path}"])


def worktree(root: str, path: str) -> str:
    full = os.path.join(root, path)
    try:
        with open(full, encoding="utf-8", errors="replace") as handle:
            return handle.read()
    except OSError:
        return ""


def cap(items, limit: int) -> str:
    """Render a sample of `items`, capped, with an honest overflow count."""
    items = sorted(items)
    shown = ", ".join(items[:limit])
    extra = len(items) - limit
    return f"{shown}{f', +{extra} more' if extra > 0 else ''}"


def group_field(values) -> str | None:
    """Pick the scalar field present in most records -- the natural grouping."""
    tally: Counter = Counter()
    for value in values:
        if isinstance(value, dict):
            for key, item in value.items():
                if isinstance(item, (str, int, bool)):
                    tally[key] += 1
    return tally.most_common(1)[0][0] if tally else None


def digest_json(old: str, new: str, limit: int) -> list[str] | None:
    """Key-level diff of a JSON object. None if it is not a JSON map."""
    try:
        old_obj = json.loads(old) if old.strip() else {}
        new_obj = json.loads(new) if new.strip() else {}
    except json.JSONDecodeError:
        return None
    if not isinstance(old_obj, dict) or not isinstance(new_obj, dict):
        return None

    old_keys, new_keys = set(old_obj), set(new_obj)
    added, removed = new_keys - old_keys, old_keys - new_keys
    changed = [k for k in old_keys & new_keys if old_obj[k] != new_obj[k]]

    lines = [
        f"  {len(old_keys)} -> {len(new_keys)} keys   "
        f"+{len(added)} added  -{len(removed)} removed  ~{len(changed)} changed"
    ]
    field = group_field(list(new_obj.values()) + list(old_obj.values()))
    for label, keys, source in (
        ("added", added, new_obj),
        ("removed", removed, old_obj),
    ):
        if not keys:
            continue
        if field:
            by_group = Counter(
                source[k].get(field) if isinstance(source[k], dict) else None
                for k in keys
            )
            summary = "  ".join(f"{g}:{n}" for g, n in by_group.most_common(limit))
            lines.append(f"  {label} by {field}: {summary}")
        else:
            lines.append(f"  {label}: {cap(keys, limit)}")
    if changed:
        lines.append(f"  changed: {cap(changed, limit)}")
    return lines


def digest_source(old: str, new: str, limit: int) -> list[str] | None:
    """Set-diff of function-like definitions. None if neither side has any."""
    old_fns, new_fns = func_names(old), func_names(new)
    if not old_fns and not new_fns:
        return None

    added, removed = new_fns - old_fns, old_fns - new_fns
    lines = [
        f"  {len(old_fns)} -> {len(new_fns)} functions   "
        f"+{len(added)} added  -{len(removed)} removed"
    ]
    if added:
        lines.append(f"  added: {cap(added, limit)}")
    if removed:
        lines.append(f"  removed: {cap(removed, limit)}")
    return lines


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Summarise a diff by meaning instead of by line."
    )
    parser.add_argument("--base", default="HEAD", help="revision to compare against")
    parser.add_argument(
        "--limit", type=int, default=15, help="max items listed per category"
    )
    parser.add_argument(
        "--full", action="store_true", help="do not cap listings (limit=100000)"
    )
    parser.add_argument("paths", nargs="*", help="restrict to these paths")
    args = parser.parse_args()
    limit = 100000 if args.full else args.limit

    root = repo_root()
    entries = changed_paths(args.base, args.paths)
    if not entries:
        print(f"diff-digest: no changes vs {args.base}")
        return 0

    print(f"diff-digest vs {args.base} -- {len(entries)} file(s) changed\n")
    plain: list[tuple[str, str]] = []

    for status, path in entries:
        old = "" if status in "A?" else blob(args.base, path)
        new = "" if status == "D" else worktree(root, path)

        lines = None
        if path.endswith(".json"):
            lines = digest_json(old, new, limit)
        elif path.endswith(C_SUFFIXES):
            lines = digest_source(old, new, limit)

        if lines:
            tag = {"?": " (untracked)", "A": " (added)", "D": " (deleted)"}.get(
                status, ""
            )
            print(f"{path}{tag}")
            print("\n".join(lines))
            print()
        else:
            plain.append((status, path))

    if plain:
        print("other changes (line counts):")
        for status, path in plain:
            old_n = len(blob(args.base, path).splitlines()) if status not in "A?" else 0
            new_n = len(worktree(root, path).splitlines()) if status != "D" else 0
            print(f"  [{status}] {path}  {old_n} -> {new_n} lines")
    return 0


if __name__ == "__main__":
    sys.exit(main())
