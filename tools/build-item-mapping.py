#!/usr/bin/env python3
"""Builds item-mapping.json: which global.ini key belongs to which Star Citizen Wiki API item.

The launcher adds item values (#21) to the global.ini of any language by key, and the keys are the same in every
language. To find them, the English names and descriptions from the Wiki API are matched against the English
global.ini. That file is only read here and must never be committed, cached or published (see LEGAL.md); the output
contains global.ini keys and Wiki item IDs only, no game texts.

Usage:
  build-item-mapping.py --reference <English global.ini> [--output item-mapping.json] [--min-share 0.9]

--min-share: fails when fewer keys are mapped than this share of the previous output, so a broken API answer never
replaces a good mapping. Standard library only, so it runs on any CI image.
"""

import argparse
import collections
import json
import os
import re
import sys
import time
import urllib.error
import urllib.parse
import urllib.request

API = "https://api.star-citizen.wiki/api/v2/"
USER_AGENT = "SC-Localization-Launcher item mapping (https://github.com/xXCrusaderXx/SC-Localization-Launcher)"
PAGE_SIZE = 200
# Without a previous file there is nothing to compare with; fewer keys than this means something went wrong
MINIMUM_KEYS = 3000


def fetch_json(url, attempts=4):
    """GET with retries; waits as long as a 429/503 answer asks (at most a minute)."""
    for attempt in range(1, attempts + 1):
        request = urllib.request.Request(url, headers={"User-Agent": USER_AGENT, "Accept": "application/json"})
        try:
            with urllib.request.urlopen(request, timeout=120) as response:
                return json.load(response)
        except urllib.error.HTTPError as error:
            if error.code not in (429, 502, 503, 504) or attempt == attempts:
                raise
            wait = error.headers.get("Retry-After", "")
            delay = min(int(wait), 60) if wait.isdigit() else 2**attempt
        except (urllib.error.URLError, TimeoutError):
            if attempt == attempts:
                raise
            delay = 2**attempt
        print(f"  retry in {delay} s: {url}", file=sys.stderr)
        time.sleep(delay)
    raise RuntimeError("unreachable")


def fetch_items(api):
    """All items, one page after another with a pause, to go easy on the Wiki."""
    items = []
    page, last = 1, 1
    while page <= last:
        query = urllib.parse.urlencode({"page[number]": page, "page[size]": PAGE_SIZE})
        data = fetch_json(f"{api}items?{query}")
        items.extend(data["data"])
        last = data["meta"]["last_page"]
        print(f"  items page {page}/{last}", file=sys.stderr)
        page += 1
        if page <= last:
            time.sleep(1)
    return items


def load_ini(path):
    keys = {}
    with open(path, encoding="utf-8-sig", errors="replace") as file:
        for line in file:
            if "=" in line:
                key, value = line.rstrip("\r\n").split("=", 1)
                keys[key] = value
    return keys


def normalise(text):
    """Same text, different notation: line breaks, markup, whitespace, case."""
    text = (text or "").replace("\\n", "\n")
    text = re.sub(r"<[^>]+>", "", text)
    return re.sub(r"\s+", " ", text).strip().lower()


def strip_header(text):
    """Game descriptions start with "Manufacturer: …\\nItem Type: …\\n\\n"; the API keeps that block separately."""
    text = text.replace("\\n", "\n")
    parts = text.split("\n\n", 1)
    if len(parts) == 2 and all(re.match(r"^[^:\n]{1,40}: ", line) for line in parts[0].split("\n")):
        return parts[1]
    return text


def tokens(text):
    return {t for t in re.split(r"[^a-z0-9]+", text.lower()) if t and t not in ("item", "name", "desc")}


def values(item):
    """What the launcher adds to names: one answer per key needs one set of these."""
    return (item.get("type"), item.get("sub_type"), item.get("size"), item.get("grade"), item.get("class"))


# Name keys without "Name" in them; only these, since other such keys hold short names and labels
NAME_KEY_PREFIXES = ("item_mining_mininglaser_",)


def is_name_key(key):
    lower = key.lower()
    return lower.startswith("item_") and ("name" in lower or lower.startswith(NAME_KEY_PREFIXES))


def desc_key_for(key, lower):
    """item_NameX -> item_DescX, item_x_Name -> item_x_Desc, if the file has it; lower: lower-case key -> key."""
    for candidate in (re.sub(r"(?i)^item_name", "item_Desc", key), re.sub(r"(?i)_name$", "_Desc", key)):
        if candidate != key and candidate.lower() in lower:
            return lower[candidate.lower()]
    return None


def pick(key, matched):
    """One item for this key: all matches must agree on the values, closest class name first."""
    best = max(len(tokens(key) & tokens(item.get("class_name") or "")) for item in matched)
    closest = [item for item in matched if len(tokens(key) & tokens(item.get("class_name") or "")) == best]
    for candidates in (matched, closest):
        if len({values(item) for item in candidates}) == 1:
            # Several variants with the same values: the base variant, else the one with the lowest ID, so runs agree
            return sorted(candidates, key=lambda item: (not item.get("is_base_variant"), item["uuid"]))[0]
    return None


def build(ini, items):
    by_name = collections.defaultdict(list)
    for item in items:
        name = (item.get("name") or "").strip()
        if name and "placeholder" not in name.lower():
            by_name[normalise(name)].append(item)

    lower = {k.lower(): k for k in ini}
    mapping = {}
    for key, text in ini.items():
        if not is_name_key(key) or not text.strip():
            continue
        matched = by_name.get(normalise(text))
        if not matched:
            continue
        item = pick(key, matched)
        if item:
            mapping[key] = item["uuid"]
            desc = desc_key_for(key, lower)
            if desc:
                mapping[desc] = item["uuid"]

    # Descriptions without a matching name key: by text, only when exactly one key has it
    desc_by_text = collections.defaultdict(list)
    for key, text in ini.items():
        lower_key = key.lower()
        if lower_key.startswith("item_") and "desc" in lower_key and key not in mapping:
            desc_by_text[normalise(strip_header(text))].append(key)
    for item in items:
        description = (item.get("description") or {}).get("en_EN")
        found = desc_by_text.get(normalise(description)) if description else None
        if found and len(found) == 1 and found[0] not in mapping:
            mapping[found[0]] = item["uuid"]
    return mapping


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--reference", required=True, help="English global.ini, read only")
    parser.add_argument("--output", default="item-mapping.json")
    parser.add_argument("--min-share", type=float, default=0.9)
    parser.add_argument("--api", default=API)
    parser.add_argument("--items", help="items JSON from an earlier run instead of the API (for development)")
    args = parser.parse_args()

    if not args.api.startswith("https://"):
        sys.exit("The Wiki API is only used over HTTPS.")

    ini = load_ini(args.reference)
    if args.items:
        with open(args.items, encoding="utf-8") as file:
            items = json.load(file)
    else:
        items = fetch_items(args.api)
    mapping = build(ini, items)

    previous = 0
    if os.path.exists(args.output):
        with open(args.output, encoding="utf-8") as file:
            previous = len(json.load(file).get("keys", {}))
    required = max(int(previous * args.min_share), MINIMUM_KEYS)
    print(f"{len(items)} Wiki items, {len(mapping)} keys mapped (before: {previous}, required: {required})", file=sys.stderr)
    if len(mapping) < required:
        sys.exit(f"Only {len(mapping)} keys mapped, expected at least {required}; the mapping is left unchanged.")

    output = {
        "version": 1,
        "source": "Star Citizen Wiki API, https://api.star-citizen.wiki (CC BY-SA)",
        "keys": dict(sorted(mapping.items())),
    }
    with open(args.output, "w", encoding="utf-8") as file:
        json.dump(output, file, indent=1, ensure_ascii=False)
        file.write("\n")


if __name__ == "__main__":
    main()
