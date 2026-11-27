#!/usr/bin/env python3
"""Replace project-board item links in a Markdown file with issue links.

Board item links look like
    [Label](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=N)
and become
    [#123](https://github.com/mikepadiernos/plasma-impresario/issues/123)
for "[Board]" labels, or "Label [#123](...)" for any other label.

The script is idempotent: links that already point at issues are left alone,
so it can be rerun after merging a branch that still has board links.

Usage:
    tools/roadmap-issue-links.py [--map items.json] [--check] [FILE ...]

Without --map, the item-to-issue mapping is fetched from the project with
`gh api graphql`. --check exits non-zero if any board item link remains,
without changing files. FILE defaults to docs/ROADMAP.md.
"""

import argparse
import json
import re
import subprocess
import sys

PROJECT_ID = "PVT_kwHOA0BSgc4BsxSB"
REPO = "mikepadiernos/plasma-impresario"
ISSUE_URL = f"https://github.com/{REPO}/issues/"
LINK = re.compile(
    r"\[([^\]]+)\]\(https://github\.com/users/mikepadiernos/projects/4"
    r"\?pane=issue&itemId=(\d+)\)"
)

QUERY = (
    """query($c:String){ node(id:"%s"){ ... on ProjectV2 {
  items(first:100, after:$c){ pageInfo{hasNextPage endCursor}
    nodes{ databaseId content{ __typename
      ... on Issue{ number repository{nameWithOwner} } } } } } } }"""
    % PROJECT_ID
)


def fetch_map():
    out, cursor = {}, None
    while True:
        args = ["gh", "api", "graphql", "-f", f"query={QUERY}"]
        if cursor:
            args += ["-f", f"c={cursor}"]
        res = subprocess.run(args, capture_output=True, text=True, check=True)
        data = json.loads(res.stdout)
        if "errors" in data:
            sys.exit(f"GraphQL error: {data['errors']}")
        items = data["data"]["node"]["items"]
        for node in items["nodes"]:
            content = node["content"] or {}
            if content.get("__typename") == "Issue":
                out[str(node["databaseId"])] = {
                    "number": content["number"],
                    "repo": content["repository"]["nameWithOwner"],
                }
        if not items["pageInfo"]["hasNextPage"]:
            return out
        cursor = items["pageInfo"]["endCursor"]


def load_map(path):
    with open(path, encoding="utf-8") as fh:
        raw = json.load(fh)
    return {
        k: {"number": v["number"], "repo": v.get("repo", REPO)}
        for k, v in raw.items()
        if v.get("number")
    }


def convert(text, mapping, missing):
    def repl(match):
        label, item_id = match.group(1), match.group(2)
        entry = mapping.get(item_id)
        if not entry or entry["repo"] != REPO:
            missing.add(item_id)
            return match.group(0)
        link = f"[#{entry['number']}]({ISSUE_URL}{entry['number']})"
        return link if label == "Board" else f"{label} {link}"

    return LINK.sub(repl, text)


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("files", nargs="*", default=["docs/ROADMAP.md"])
    parser.add_argument("--map", help="JSON map of item database ID to issue")
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()

    if args.check:
        found = False
        for path in args.files:
            with open(path, encoding="utf-8") as fh:
                for lineno, line in enumerate(fh, 1):
                    if LINK.search(line):
                        print(f"{path}:{lineno}: board item link")
                        found = True
        return 1 if found else 0

    mapping = load_map(args.map) if args.map else fetch_map()
    missing = set()
    for path in args.files:
        with open(path, encoding="utf-8") as fh:
            text = fh.read()
        new = convert(text, mapping, missing)
        if new != text:
            with open(path, "w", encoding="utf-8") as fh:
                fh.write(new)
            print(f"updated {path}")
    if missing:
        print("unresolved item IDs: " + ", ".join(sorted(missing)), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
