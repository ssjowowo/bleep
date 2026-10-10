#!/usr/bin/env python3
"""Build app/ir_library.c, the IR code library, from Flipper-IRDB.

    git clone --depth 1 https://github.com/Lucaslhm/Flipper-IRDB /tmp/irdb
    python3 -I tools/make_ir_library.py /tmp/irdb

Flipper-IRDB is CC0 (public domain), so the codes can ship in the firmware.

For each brand below, every remote file in its folders is read; only parsed
signals in a protocol the remote sends are kept (raw recordings and RCA, RC5X
and Pioneer frames are skipped; so Thomson TVs (RCA), Xiaomi TVs and Pioneer
receivers (raw only) aren't in the list: their keys can be learned instead). Files are grouped by their Power code: one
group is one code set, and the groups most files share come first, so the
"Did the TV react?" test tries the likeliest set first. A set is the group's
most complete file, with functions it lacks filled in from the others in the
group (only codes on the same protocol and address as its Power).

Codes are stored as Flipper writes them: address and command are its
little-endian bytes read as one number (NECext address EA C7 = 0xC7EA,
Kaseikyo address 80 02 20 00 = 0x200280). The HAL's encoders take them so.
"""
import collections
import os
import re
import subprocess
import sys

# (brand as shown, kind, [folders under the IRDB root], at most this many code sets)
BRANDS = [
    # TVs
    ("Samsung", "DEV_TV", ["TVs/Samsung"], 6),
    ("LG", "DEV_TV", ["TVs/LG"], 6),
    ("Sony", "DEV_TV", ["TVs/Sony"], 5),
    ("TCL", "DEV_TV", ["TVs/TCL"], 4),
    ("Hisense", "DEV_TV", ["TVs/Hisense"], 4),
    ("Philips", "DEV_TV", ["TVs/Philips"], 5),
    ("Panasonic", "DEV_TV", ["TVs/Panasonic"], 4),
    ("Sharp", "DEV_TV", ["TVs/Sharp"], 4),
    ("Toshiba", "DEV_TV", ["TVs/Toshiba"], 4),
    ("Vizio", "DEV_TV", ["TVs/Vizio"], 4),
    ("Grundig", "DEV_TV", ["TVs/Grundig"], 3),
    ("Hitachi", "DEV_TV", ["TVs/Hitachi"], 3),
    ("Insignia", "DEV_TV", ["TVs/Insignia"], 3),
    ("JVC", "DEV_TV", ["TVs/JVC"], 3),
    ("Telefunken", "DEV_TV", ["TVs/Telefunken"], 3),
    ("Medion", "DEV_TV", ["TVs/Medion"], 2),
    ("Magnavox", "DEV_TV", ["TVs/Magnavox"], 2),
    ("Element", "DEV_TV", ["TVs/Element"], 2),
    ("Sanyo", "DEV_TV", ["TVs/Sanyo"], 2),
    ("Haier", "DEV_TV", ["TVs/Haier"], 1),
    ("Loewe", "DEV_TV", ["TVs/Loewe"], 1),
    ("Westinghouse", "DEV_TV", ["TVs/Westinghouse"], 2),
    ("Sceptre", "DEV_TV", ["TVs/Sceptre"], 2),
    ("Blaupunkt", "DEV_TV", ["TVs/Blaupunkt"], 1),
    # Soundbars
    ("Samsung", "DEV_SPEAKER", ["SoundBars/Samsung"], 4),
    ("LG", "DEV_SPEAKER", ["SoundBars/LG"], 4),
    ("Sony", "DEV_SPEAKER", ["SoundBars/Sony"], 4),
    ("Bose", "DEV_SPEAKER", ["SoundBars/Bose"], 4),
    ("Sonos", "DEV_SPEAKER", ["SoundBars/Sonos"], 1),
    ("Yamaha", "DEV_SPEAKER", ["SoundBars/Yamaha"], 3),
    ("JBL", "DEV_SPEAKER", ["SoundBars/JBL"], 3),
    ("Vizio", "DEV_SPEAKER", ["SoundBars/Vizio"], 4),
    ("Denon", "DEV_SPEAKER", ["SoundBars/Denon"], 3),
    ("Polk Audio", "DEV_SPEAKER", ["SoundBars/PolkAudio"], 2),
    ("Philips", "DEV_SPEAKER", ["SoundBars/Philips"], 2),
    ("TCL", "DEV_SPEAKER", ["SoundBars/TCL"], 2),
    ("Hisense", "DEV_SPEAKER", ["SoundBars/Hisense"], 1),
    ("Xiaomi", "DEV_SPEAKER", ["SoundBars/Xiaomi"], 2),
    ("Klipsch", "DEV_SPEAKER", ["SoundBars/Klipsch"], 1),
    ("Sennheiser", "DEV_SPEAKER", ["SoundBars/Sennheiser"], 1),
    ("Creative", "DEV_SPEAKER", ["SoundBars/Creative"], 2),
    ("Panasonic", "DEV_SPEAKER", ["SoundBars/Panasonic"], 1),
    ("Teufel", "DEV_SPEAKER", ["SoundBars/Teufel"], 1),
    ("Canton", "DEV_SPEAKER", ["SoundBars/Canton"], 1),
    ("JVC", "DEV_SPEAKER", ["SoundBars/JVC"], 1),
    ("Toshiba", "DEV_SPEAKER", ["SoundBars/Toshiba"], 1),
    ("Grundig", "DEV_SPEAKER", ["SoundBars/Grundig"], 1),
    # AV receivers
    ("Denon", "DEV_AVR", ["Audio_and_Video_Receivers/Denon"], 3),
    ("Marantz", "DEV_AVR", ["Audio_and_Video_Receivers/Marantz"], 2),
    ("Onkyo", "DEV_AVR", ["Audio_and_Video_Receivers/Onkyo"], 3),
    ("Yamaha", "DEV_AVR", ["Audio_and_Video_Receivers/Yamaha"], 4),
    ("Sony", "DEV_AVR", ["Audio_and_Video_Receivers/Sony"], 4),
    ("Harman Kardon", "DEV_AVR", ["Audio_and_Video_Receivers/Harman_Kardon"], 2),
]

PROTOS = {"NEC": "IR_NEC", "NECext": "IR_NECEXT", "Samsung32": "IR_SAMSUNG32", "SIRC": "IR_SIRC",
          "SIRC15": "IR_SIRC15", "SIRC20": "IR_SIRC20", "RC5": "IR_RC5", "RC6": "IR_RC6",
          "Kaseikyo": "IR_KASEIKYO"}

# Flipper button names (lower case, spaces and dashes as _) -> the remote's functions, best name first
NAMES = {
    "FN_POWER": ["power", "power_toggle", "on_off", "standby", "pwr", "an_aus"],
    "FN_POWER_ON": ["power_on", "on", "poweron", "pwr_on"],
    "FN_POWER_OFF": ["power_off", "off", "poweroff", "pwr_off"],
    "FN_UP": ["up", "cursor_up", "arrow_up"],
    "FN_DOWN": ["down", "cursor_down", "arrow_down"],
    "FN_LEFT": ["left", "cursor_left", "arrow_left"],
    "FN_RIGHT": ["right", "cursor_right", "arrow_right"],
    "FN_OK": ["ok", "select", "enter_ok", "center"],
    "FN_BACK": ["back", "return", "exit"],
    "FN_HOME": ["home", "smart_hub", "smart"],
    "FN_MENU": ["menu", "settings", "setup"],
    "FN_GUIDE": ["guide", "epg"],
    "FN_VOL_UP": ["vol_up", "volume_up", "vol+", "vol_plus", "voll+"],
    "FN_VOL_DOWN": ["vol_dn", "vol_down", "volume_down", "vol-", "vol_minus", "voll-"],
    "FN_MUTE": ["mute"],
    "FN_PLAY_PAUSE": ["play_pause", "play/pause", "play_pa", "playpause", "play"],
    "FN_REW": ["rewind", "rew", "fast_ba", "fast_back", "reverse"],
    "FN_FWD": ["fast_forward", "ff", "fwd", "fastforward", "fast_fo", "fast_fwd", "forward"],
    "FN_NETFLIX": ["netflix"],
    "FN_YOUTUBE": ["youtube"],
    "FN_INPUT_1": ["hdmi1", "hdmi_1"],
    "FN_INPUT_2": ["hdmi2", "hdmi_2"],
    "FN_INPUT_3": ["hdmi3", "hdmi_3"],
    "FN_INPUT_4": ["hdmi4", "hdmi_4"],
    "FN_SUBTITLES": ["subtitle", "subtitles", "cc"],
    "FN_AUDIO": ["audio"],
    "FN_INFO": ["info", "display"],
    "FN_SOURCE": ["source", "input", "input_select", "source_select", "src", "tv/av", "av/tv"],
    "FN_CH_UP": ["ch_next", "ch_up", "channel_up", "ch+", "p+", "prog+", "page_up_ch"],
    "FN_CH_DOWN": ["ch_prev", "ch_down", "channel_down", "ch-", "p-", "prog-"],
}
for d in range(10):
    NAMES[f"FN_DIGIT_{d}"] = [str(d), f"num_{d}", f"digit_{d}", f"key_{d}"]
ALIAS = {n: (fn, rank) for fn, ns in NAMES.items() for rank, n in enumerate(ns)}
POWERS = ("FN_POWER", "FN_POWER_ON", "FN_POWER_OFF")


def le(field):
    return sum(int(b, 16) << (8 * i) for i, b in enumerate(field.split()))


def read(path):
    """{fn: (proto, address, command)} from one .ir file"""
    codes, ranks = {}, {}
    text = open(path, encoding="utf-8", errors="replace").read()
    for block in re.split(r"^#.*$", text, flags=re.M):
        f = dict(re.findall(r"^(\w+):\s*(.*?)\s*$", block, flags=re.M))
        if f.get("type") != "parsed" or f.get("protocol") not in PROTOS or "name" not in f:
            continue
        name = re.sub(r"[\s\-]+", "_", f["name"].strip().lower())
        if name not in ALIAS:
            continue
        fn, rank = ALIAS[name]
        try:
            code = (PROTOS[f["protocol"]], le(f["address"]), le(f["command"]))
        except (KeyError, ValueError):
            continue
        if fn not in codes or rank < ranks[fn]:   # "power" beats "standby"
            codes[fn], ranks[fn] = code, rank
    return codes


def useful(kind, codes):
    if kind == "DEV_SPEAKER" and "FN_VOL_UP" in codes and "FN_VOL_DOWN" in codes:
        return True   # a soundbar may have no Power of its own (Sonos follows the TV)
    if not any(p in codes for p in POWERS):
        return False
    if kind == "DEV_TV":
        return "FN_VOL_UP" in codes or "FN_UP" in codes
    return "FN_VOL_UP" in codes and "FN_VOL_DOWN" in codes


def code_sets(root, kind, folders, most):
    files = []
    for folder in folders:
        for dirpath, _, names in os.walk(os.path.join(root, folder)):
            for n in sorted(names):
                if n.endswith(".ir"):
                    codes = read(os.path.join(dirpath, n))
                    if useful(kind, codes):
                        files.append(codes)
    groups = collections.defaultdict(list)
    for codes in files:
        key = next(codes[p] for p in POWERS + ("FN_VOL_UP",) if p in codes)
        groups[key].append(codes)
    # most files first; then the most complete; then by code, so the order never wobbles
    ordered = sorted(groups.items(), key=lambda g: (-len(g[1]), -max(len(c) for c in g[1]), g[0]))
    sets = []
    for key, members in ordered[:most]:
        members.sort(key=lambda c: -len(c))
        merged = dict(members[0])
        for fn in NAMES:
            if fn in merged:
                continue
            votes = collections.Counter(c[fn] for c in members[1:]
                                        if fn in c and c[fn][0] == key[0] and c[fn][1] == key[1])
            if votes:
                merged[fn] = votes.most_common(1)[0][0]
        sets.append(merged)
    return sets


def main():
    root = sys.argv[1]
    try:
        commit = subprocess.run(["git", "-C", root, "rev-parse", "--short", "HEAD"], capture_output=True,
                                text=True).stdout.strip() or "unknown"
    except OSError:
        commit = "unknown"
    order = list(NAMES)
    out = [
        "/*",
        " * The IR code library: TVs, soundbars and AV receivers by brand.",
        f" * Generated by tools/make_ir_library.py from Flipper-IRDB (CC0), commit {commit}.",
        " * Don't edit: change the brand list in the script and run it again.",
        " */",
        '#include "model.h"',
        "",
        "#define C(fn, proto, a, b) {fn, proto, a, b}",
        "",
    ]
    brands, table, total = [], [], 0
    for name, kind, folders, most in BRANDS:
        sets = code_sets(root, kind, folders, most)
        if not sets:
            print(f"  {name} ({kind}): no usable code sets, left out", file=sys.stderr)
            continue
        brands.append((name, kind, len(sets), len(table)))
        for s in sets:
            table.append((total, s))
            total += len(s)
    out.append("static const ir_lib_code_t codes[] = {")
    for i, (first, s) in enumerate(table):
        bname = next(b for b in brands if b[3] <= i < b[3] + b[2])
        kind = {"DEV_TV": "TV", "DEV_SPEAKER": "soundbar", "DEV_AVR": "AV receiver"}[bname[1]]
        out.append(f"    /* {bname[0]} {kind}, set {i - bname[3] + 1} */")
        for fn in order:
            if fn in s:
                p, a, b = s[fn]
                out.append(f"    C({fn}, {p}, 0x{a:X}, 0x{b:X}),")
    out.append("};")
    out.append("")
    out.append("const ir_lib_set_t ir_lib_sets[] = {")
    for first, s in table:
        out.append(f"    {{{first}, {len(s)}}},")
    out.append("};")
    out.append("")
    out.append("const ir_brand_t ir_brands[] = {")
    for name, kind, n, first in brands:
        out.append(f'    {{"{name}", {kind}, {n}, {first}}},')
    out.append("};")
    out.append("const int ir_brand_count = sizeof(ir_brands) / sizeof(ir_brands[0]);")
    out.append("const ir_lib_code_t *const ir_lib_codes = codes;")
    out.append("")
    dest = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "app", "ir_library.c")
    with open(dest, "w") as f:
        f.write("\n".join(out))
    print(f"{len(brands)} brands, {len(table)} code sets, {total} codes -> app/ir_library.c", file=sys.stderr)


main()
