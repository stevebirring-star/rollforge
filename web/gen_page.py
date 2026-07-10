#!/usr/bin/env python3
"""Generate the RollForge web page.

The user manual is generated from the app's OWN in-app Help text (src/ui/AboutView.cpp),
so the page and the app cannot drift apart. Everything else is hand-written.
"""
import html
import json
import re
import sys
from pathlib import Path

REPO = Path("/home/birring/rollforge")
OUT = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("/tmp/rollforge-index.html")
DOWNLOADS = json.loads(sys.argv[2]) if len(sys.argv) > 2 else {}
VERSION = sys.argv[3] if len(sys.argv) > 3 else "0.2.0"


def help_entries():
    src = (REPO / "src/ui/AboutView.cpp").read_text(encoding="utf-8")
    i = src.index("body.setText (")
    j = src.index("        false);", i)
    lits = re.findall(r'"((?:[^"\\]|\\.)*)"', src[i:j])
    text = "".join(l.replace("\\n", "\n").replace('\\"', '"').replace("\\'", "'") for l in lits)

    entries = []
    for para in [p for p in text.split("\n\n") if p.strip()]:
        raw = para.rstrip()
        indented = raw.startswith("    ")
        flat = " ".join(raw.split())
        m = re.match(r"^([A-Za-z0-9 /&()+.\-]{2,40}?)\s+-\s+(.*)$", flat, re.S)
        if flat == "Getting started":
            continue
        if m:
            # The Help reads "Make a Beat  -  one tap generates ...". Split apart, the body is a
            # sentence fragment starting lower-case; on its own line it needs a capital.
            body = m.group(2).strip()
            body = body[:1].upper() + body[1:] if body else body
            entries.append((m.group(1).strip(), body, indented))
        else:
            entries.append((None, flat, indented))
    return entries


# The app draws this in BrandMark::paint(): the anvil path from anvilGlyph(), filled in
# textDim over a 1px shadow, with two accentHot sparks off the horn. Coordinates below are
# that path's unit-square points multiplied by 100, so the page mark IS the app's mark.
ANVIL_PATH = ("M6,44 L78,44 L94,52 L78,56 L60,56 L54,74 L74,86 L74,94 "
              "L16,94 L16,86 L36,74 L30,56 L6,56 Z")

def anvil_svg(px=40, ids="m"):
    return (
      f'<svg class="anvil" viewBox="0 0 108 100" width="{px}" height="{int(px*100/108)}" '
      f'aria-hidden="true" focusable="false">'
      f'<path d="{ANVIL_PATH}" transform="translate(0,3)" fill="rgba(0,0,0,.55)"/>'
      f'<path d="{ANVIL_PATH}" fill="#9a9aa4"/>'
      f'<circle cx="90.2" cy="24.2" r="4.2" fill="#ff7a1a"/>'
      f'<circle cx="103.1" cy="5.1" r="3.1" fill="#ff7a1a"/>'
      f'</svg>')

FAVICON = ("data:image/svg+xml;utf8,"
           "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 108 108'>"
           "<rect width='108' height='108' rx='20' fill='%2312141a'/>"
           "<path d='" + ANVIL_PATH.replace(' ', '%20') + "' fill='%239a9aa4'/>"
           "<circle cx='90.2' cy='24.2' r='4.2' fill='%23ff7a1a'/>"
           "<circle cx='103.1' cy='5.1' r='3.1' fill='%23ff7a1a'/>"
           "</svg>")


# --- the differentiators, hand-written: this is a pitch, not a reference ------------
FEATURES = [
    ("Make a Beat",
     "One tap writes a full groove in the style you pick -- Trap, Boom-bap, House and more. "
     "Reroll for another take, Vary to nudge the one you have. Lock any lane to keep it while "
     "the rest gamble."),
    ("Beatbox it in",
     "Press REC and beatbox into your microphone. RollForge finds the hits, decides which are "
     "kicks, snares and hats, and writes them to the grid in time. Or set it to Pads and tap "
     "the beat in with your keyboard, mouse or a MIDI controller."),
    ("EVOLVE -- never the same bar twice",
     "Every bar becomes a fresh variation of the pattern you wrote. Crucially it varies what "
     "you wrote, not the bar before it: a variation of a variation is a random walk, and a "
     "random walk turns a groove into mush. Drift sets how far each bar strays."),
    ("Paint a roll",
     "Turn on Roll Brush and drag across a lane. You get an accelerating roll with ratchets, "
     "velocity ramps and a shape you choose -- not a row of identical hits you programmed by hand."),
    ("Eight patterns, one kit",
     "A to H hold eight patterns over one kit, and a switch lands on the next bar line, so a "
     "verse becomes a chorus in time. Chain them into a song: A x2, B, A x2, C."),
    ("A library that knows what things sound like",
     "Point it at your samples once and it watches that folder for ever, analysing anything you "
     "drop in. Every pad gets a Similar button that swaps it for the nearest sound of the same "
     "kind. See the whole library as a constellation, placed by how things sound."),
    ("Resample onto a pad",
     "Bounce the whole pattern -- kit, rolls, macros, EQ, the lot -- onto any pad as one "
     "seamless loop. The decay that runs past the bar is folded back over the start, where the "
     "loop's next pass would have put it."),
    ("Hats that choke themselves",
     "Build a kit and the closed hat cuts the open one, automatically, from the first launch. "
     "No choke groups to set up and nothing to route. Drop any sample on a hat pad and it joins "
     "the group -- something the tools people compare this to cannot do at all."),
    ("Export that matches what you heard",
     "MIDI, a WAV mix, or per-pad stems. Stems are pre-master, so they add back up to the mix "
     "exactly. Or drag the loop straight into your DAW."),
]

MANUAL_GROUPS = [
    ("First sounds", ["Make a Beat", "Play", "Capture (REC)", "PADS", "MIC", "Tempo & feel", "Vary",
                      "New Sounds", "Lock a lane"]),
    ("Pads and sounds", ["Pads", "Hats choke each other", "Layers", "Similar", "Resample", "Slice"]),
    ("The sequencer", ["Sequencer", "Triplets", "Roll Brush", "Note Repeat", "Evolve"]),
    ("Arranging", ["Patterns A-H", "Song"]),
    ("Mixing", ["Master", "Meters"]),
    ("Your samples", ["Library", "Map"]),
    ("Files and setup", ["Save / Open", "Export", "Settings", "Keys", "The tour"]),
]

CSS = """
:root{--bg:#12141a;--bg2:#0e1015;--panel:#191c23;--raised:#232730;--line:#2a2e38;
--text:#e8e8ec;--dim:#9a9aa4;--faint:#6c7383;--hot:#ff7a1a;--ember:#ffb43a;--cool:#3f8cff}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--text);
font:16px/1.65 -apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,Helvetica,Arial,sans-serif}
a{color:var(--hot)}
.wrap{max-width:960px;margin:0 auto;padding:0 20px}
header{border-bottom:1px solid var(--line);background:linear-gradient(180deg,#161922,#12141a)}
.hero{padding:64px 0 56px}
.brand{font-size:44px;font-weight:800;letter-spacing:-.5px;margin:0;
display:flex;align-items:center;gap:14px}
.brand .word{display:inline-block}
.brand .hot{color:var(--hot)}
.brand .anvil{flex:0 0 auto;display:block}
.tag{color:var(--dim);font-size:19px;margin:10px 0 0}
.ver{display:inline-block;margin-top:18px;padding:4px 10px;border:1px solid var(--line);
border-radius:99px;color:var(--faint);font-size:13px}
.cta{display:inline-block;margin-top:26px;background:var(--hot);color:#12141a;font-weight:700;
padding:13px 24px;border-radius:6px;text-decoration:none}
.cta:hover{background:var(--ember)}
h2{font-size:26px;margin:56px 0 6px;letter-spacing:-.2px}
h2+.sub{color:var(--dim);margin:0 0 26px}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(280px,1fr));gap:16px}
.card{background:var(--panel);border:1px solid var(--line);border-radius:8px;padding:20px}
.card h3{margin:0 0 8px;font-size:17px;color:var(--text)}
.card p{margin:0;color:var(--dim);font-size:15px}
.dl{display:grid;grid-template-columns:repeat(auto-fit,minmax(300px,1fr));gap:16px}
.dl a{display:block;background:var(--raised);border:1px solid var(--line);border-radius:8px;
padding:18px 20px;text-decoration:none;color:var(--text)}
.dl a:hover{border-color:var(--hot)}
.dl .os{font-weight:700;font-size:17px}
.dl .file{color:var(--faint);font-size:13px;margin-top:4px;font-family:ui-monospace,Menlo,monospace}
.dl .note{color:var(--dim);font-size:14px;margin-top:8px}
.lock{background:#1d1a14;border:1px solid #4a3a1c;border-radius:8px;padding:16px 18px;margin:0 0 22px;
color:var(--ember);font-size:15px}
.lock b{color:var(--text)}
.man{margin-top:10px}
.man section{border-top:1px solid var(--line);padding-top:22px;margin-top:26px}
.man h3{font-size:20px;margin:0 0 14px}
.man dl{margin:0}
.man dt{font-weight:700;color:var(--text);margin-top:18px}
.man dt:first-child{margin-top:0}
.man dd{margin:5px 0 0;color:var(--dim)}
.man dd.sub{margin-left:18px;border-left:2px solid var(--line);padding-left:14px}
.man .kicker{color:var(--dim)}
footer{border-top:1px solid var(--line);margin-top:70px;padding:30px 0 60px;color:var(--faint);font-size:14px}
footer p{margin:0 0 8px}
footer .lic{font-size:12.5px;color:var(--faint);max-width:70ch;line-height:1.6}
code{background:var(--raised);padding:1px 6px;border-radius:4px;font-size:.9em;
font-family:ui-monospace,Menlo,monospace;color:var(--ember)}
@media (max-width:640px){.brand{font-size:34px}.hero{padding:44px 0 40px}}
"""


def esc(t):
    return html.escape(t, quote=False)


def render_manual(entries):
    by_title = {t: (b, ind) for (t, b, ind) in entries if t}
    used = set()
    out = []
    for group, titles in MANUAL_GROUPS:
        rows = []
        for t in titles:
            if t not in by_title:
                continue
            body, indented = by_title[t]
            used.add(t)
            cls = ' class="sub"' if indented else ""
            rows.append(f"<dt>{esc(t)}</dt><dd{cls}>{esc(body)}</dd>")
        if rows:
            out.append(f'<section><h3>{esc(group)}</h3><dl>{"".join(rows)}</dl></section>')

    leftover = [(t, b) for (t, b, _) in entries if t and t not in used]
    if leftover:
        rows = "".join(f"<dt>{esc(t)}</dt><dd>{esc(b)}</dd>" for t, b in leftover)
        out.append(f'<section><h3>Everything else</h3><dl>{rows}</dl></section>')

    tail = [b for (t, b, _) in entries if not t]
    if tail:
        out.append('<section><h3>Notes</h3>'
                   + "".join(f'<p class="kicker">{esc(b)}</p>' for b in tail) + "</section>")
    return "".join(out)


def render_downloads():
    order = [
        ("windows_setup", "Windows", "Installer -- the normal choice"),
        ("windows_zip", "Windows", "Portable zip -- no install, unzip and run"),
        ("linux_appimage", "Linux", "AppImage -- chmod +x, then run it"),
        ("linux_targz", "Linux", "Portable tar.gz -- extract and run"),
    ]
    cards = []
    for key, os_name, note in order:
        f = DOWNLOADS.get(key)
        if not f:
            continue
        cards.append(
            f'<a href="downloads/{esc(f["name"])}">'
            f'<div class="os">{esc(os_name)}</div>'
            f'<div class="file">{esc(f["name"])} &middot; {esc(f["size"])}</div>'
            f'<div class="note">{esc(note)}</div></a>')
    return "".join(cards)


def main():
    entries = help_entries()
    feature_cards = "".join(
        f'<div class="card"><h3>{esc(t)}</h3><p>{esc(b)}</p></div>' for t, b in FEATURES)

    page = f"""<!doctype html>
<html lang="en"><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>RollForge -- the fast, non-technical beat sketchpad</title>
<meta name="description" content="RollForge is a drum machine you can beatbox into. One tap writes a groove; EVOLVE never plays the same bar twice. Free, open source, Windows and Linux.">
<meta name="robots" content="noindex">
<link rel="icon" href="{FAVICON}">
<style>{CSS}</style>
</head><body>
<header><div class="wrap hero">
  <h1 class="brand">{anvil_svg(46)}<span class="word">ROLL<span class="hot">FORGE</span></span></h1>
  <p class="tag">The fast, non-technical beat sketchpad. Beatbox a rhythm, and it becomes a pattern.</p>
  <div class="ver">version {esc(VERSION)} &middot; Windows &amp; Linux &middot; GPLv3</div><br>
  <a class="cta" href="#downloads">Download</a>
</div></header>

<main class="wrap">
  <h2>What it does</h2>
  <p class="sub">Eight things you cannot do, or cannot do this fast, in a normal drum machine.</p>
  <div class="grid">{feature_cards}</div>

  <h2 id="downloads">Downloads</h2>
  <p class="sub">Version {esc(VERSION)}. Built and tested on Linux and Windows.</p>
  <div class="lock">
    <b>The downloads are password protected.</b> Your browser will ask for a username and a
    password when you click one. The username is <code>rollforge</code>; use the password you
    were given.
  </div>
  <div class="dl">{render_downloads()}</div>

  <h2>User manual</h2>
  <p class="sub">This is the app's own Help text, generated from the source, so it cannot fall
  out of date. The same guide lives under Help inside RollForge, along with a guided tour.</p>
  <div class="man">{render_manual(entries)}</div>
</main>

<footer><div class="wrap">
  <p>&copy; 2026 <a href="https://getstackbase.com">StackBase.com</a>. All rights reserved.</p>
  <p class="lic">RollForge {esc(VERSION)} is built against <a href="https://juce.com">JUCE</a> under its
  GPLv3 option and is distributed under the GPLv3. The complete corresponding source code is
  available on request from <a href="mailto:hello@getstackbase.com">hello@getstackbase.com</a>.</p>
</div></footer>
</body></html>
"""
    OUT.write_text(page, encoding="utf-8")
    print(f"wrote {OUT} ({len(page)} bytes)")


if __name__ == "__main__":
    main()
