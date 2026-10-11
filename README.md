# disparPDF

PDF comparison tool — compares text or visual appearance of two PDF files.

**disparPDF** is a Qt6 port of [ConfrontaPDF](https://github.com/lbellonda/ConfrontaPDF)
by Luca Bellonda (2015), which is itself a fork of
[DiffPDF](http://www.qtrac.eu/diffpdf-foss.html) by Mark Summerfield (2008–2013).

This Qt6 port was created by Yuwsuke Kieda in 2026 with the assistance of AI tools
(Claude by Anthropic).

## Features

- Compare two PDF files page by page (text or visual mode)
- Word or character comparison
- Page range specification, and a page offset for when one document has
  pages the other does not
- Margin exclusion
- Comparing only inside a page boundary — the MediaBox, CropBox, BleedBox,
  TrimBox or ArtBox (`--box`) — with the pages lined up on its corner
- Font comparison (`--compareFonts`)
- Pages compared in parallel, one worker thread per core
- Batch/command line mode (`disparPDFc`), which can also show the window
  with `--interactive`
- Manual page (`man disparPDF`) and bash/zsh completion

## Install via Homebrew tap (recommended)

```sh
brew tap yuw/disparPDF
brew trust yuw/disparPDF
brew install yuw/disparPDF/disparPDF
```

The `disparPDF` and `disparPDFc` commands work right away. To also have the
app in `/Applications` (for Finder, Dock and Spotlight), copy it there yourself:

```sh
ditto /opt/homebrew/opt/disparpdf/disparPDF.app /Applications/disparPDF.app
```

macOS does not allow Homebrew to write into an existing app bundle in
`/Applications` (App Management protection), so **repeat this command after
every `brew upgrade`** — otherwise the Finder copy stays on the old version.
Use `ditto`, not `cp -r`: with an existing bundle `cp -r` nests the new copy
inside the old one instead of replacing it. `ditto` preserves the code
signature, so no re-signing is needed.

After installation:

| Location | Description |
|---|---|
| `/Applications/disparPDF.app` | GUI app (Finder) |
| `/opt/homebrew/opt/disparPDF/disparPDF.app` | Homebrew-managed copy |
| `/opt/homebrew/bin/disparPDF` | CLI wrapper (launches GUI) |
| `/opt/homebrew/bin/disparPDFc` | CLI batch mode |

## Upgrading

```sh
brew update
brew upgrade yuw/disparPDF/poppler-qt6 yuw/disparPDF/disparPDF
```

The `disparPDF` and `disparPDFc` commands are current as soon as this finishes.

**If only `poppler-qt6` was upgraded**, rebuild disparPDF against the new
bindings. Homebrew does not rebuild a formula when one of its dependencies is
updated, so the binary would keep running against the previous Poppler:

```sh
brew reinstall yuw/disparPDF/disparPDF
```

**If you keep a copy in `/Applications`**, refresh it after every upgrade.
macOS does not allow Homebrew to write into an existing app bundle there, so
this step cannot be automated:

```sh
ditto /opt/homebrew/opt/disparpdf/disparPDF.app /Applications/disparPDF.app
```

To check what is installed, and to reclaim the disk space held by the
superseded versions:

```sh
brew list --versions disparPDF poppler-qt6
brew cleanup
```

## Migrating from manual install to Homebrew tap

If you have previously built and installed disparPDF manually, follow these steps:

```sh
# 1. Install via Homebrew tap
brew tap yuw/disparPDF
brew trust yuw/disparPDF
brew install yuw/disparPDF/disparPDF

# 2. Verify the tap installation
brew info yuw/disparPDF/disparPDF
ls /opt/homebrew/bin/disparPDF
ls /opt/homebrew/bin/disparPDFc

# 3. Remove the manual install
sudo rm -f /usr/local/bin/disparPDF
sudo rm -f /usr/local/bin/disparPDFc
sudo rm -rf /usr/local/disparPDF.app
sudo rm -rf /Applications/disparPDF.app

# 4. Copy to /Applications
ditto /opt/homebrew/opt/disparpdf/disparPDF.app /Applications/disparPDF.app

# 5. Verify
open /Applications/disparPDF.app
disparPDFc -b 2>&1 | head -1
```

## Build from source

### Dependencies (macOS / Homebrew)

Homebrew's `poppler` does not include Qt6 bindings.
Use the `poppler-qt6.rb` formula in `packaging/homebrew/` to install via a local tap.

disparPDF also needs [qpdf](https://qpdf.sourceforge.io/), which reads and
rewrites the page boundaries.

```sh
brew install qt@6 qpdf

mkdir -p ~/homebrew-disparPDF/Formula
cp packaging/homebrew/poppler-qt6.rb ~/homebrew-disparPDF/Formula/
cd ~/homebrew-disparPDF
git init
git add Formula/poppler-qt6.rb
git commit -m "Add poppler-qt6 formula"
cd -

brew tap yuw/disparPDF ~/homebrew-disparPDF
brew install yuw/disparPDF/poppler-qt6
```

### Build

On macOS, CMake locates Homebrew's keg-only `qt@6` and `poppler-qt6`, and
`qpdf`, automatically, so no environment variables are needed. Elsewhere it
finds qpdf with pkg-config (`libqpdf`; e.g. the `libqpdf-dev` package on
Debian and Ubuntu):

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(sysctl -n hw.logicalcpu)
```

To use a Qt or Poppler installation elsewhere, pass it explicitly — an
explicit `CMAKE_PREFIX_PATH` takes precedence over the auto-detected paths:

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="/path/to/qt6;/path/to/poppler-qt6"
```

### Test

The tests run the batch mode on small PDFs and need no display:

```sh
ctest --test-dir build
```

They are described in `test/CMakeLists.txt`; configure with
`-DBUILD_TESTING=OFF` to leave them out.

### Install

```sh
# Install to /usr/local
sudo cmake --install build --prefix /usr/local

# Re-sign after install (required on macOS 26+)
codesign --force --sign - /usr/local/disparPDF.app/Contents/MacOS/disparPDF

# Optional: copy to /Applications (ditto replaces an existing bundle and
# keeps the signature; cp -r would nest the new copy inside the old one)
ditto /usr/local/disparPDF.app /Applications/disparPDF.app

# Optional: add symlink for CLI use
sudo ln -sf /usr/local/disparPDF.app/Contents/MacOS/disparPDF /usr/local/bin/disparPDF
```

On Linux and the BSDs, `cmake --install` also installs the manual page
(`man disparPDF`, or `man disparPDFc`), bash and zsh completion (set
`BASH_COMPLETION_DIR` and `ZSH_COMPLETION_DIR` to choose where), a desktop
file and icon for the application menus, and the README; packagers can use
`DESTDIR` and the usual `CMAKE_INSTALL_*` directories.  The manual page is
generated with `help2man` from `--help`; without it, or when cross
compiling, the copy in `doc_man/` is installed instead (refresh it with
`cmake --build build --target update-manpage`).

## Usage

### GUI

```sh
# Launch from Finder
open /Applications/disparPDF.app

# Launch with files from terminal
disparPDF a.pdf b.pdf
```

### Command line (batch mode)

```sh
# Returns 0 if identical, non-0 if differences found
disparPDFc -b a.pdf b.pdf

# With description
disparPDFc -b --outType=1 a.pdf b.pdf

# XML output
disparPDFc -b --xmlResult=result.xml a.pdf b.pdf
```

```sh
# Only what is inside the trim box: crop marks, colour bars and bleed are
# ignored, and a page with bleed matches the same page without it
disparPDFc -b --box=trim a.pdf b.pdf
```

`disparPDFc` is not a separate program but a symbolic link to `disparPDF`
(on Windows, a copy of it): run by that name, it is in batch mode, so
`-b` is optional, and it needs no display.  `disparPDF -b` does the
same, and `disparPDFc --interactive` shows the window, as `disparPDF` does.

### Comparing inside a page box

A PDF page can have up to five boundaries (ISO 32000-2:2020, 14.11.2):

- the **MediaBox**, the physical medium;
- the **CropBox**, to which it is clipped when displayed or printed;
- the **BleedBox**, to which it is clipped in production;
- the **TrimBox**, the finished page after trimming;
- the **ArtBox**, the page's meaningful content.

`--box=BOX` (or the GUI's **Page Box** dock) compares only what is inside
one of them. `none` compares whole pages, as without the option.

- **A page without the box** uses its default from the standard's Table 31:
  the CropBox for the BleedBox, TrimBox and ArtBox, and the MediaBox for the
  CropBox. A warning says so.
- **Clipping**, chosen with `--box-clip=CLIP`:

  | `CLIP` | what is compared |
  |---|---|
  | `media` (the default) | the box clipped to the MediaBox, as 14.11.2.1 requires of a processor |
  | `crop` | the box clipped to the MediaBox and the CropBox, i.e. only what a viewer shows |
  | `none` | the box as written, even where it extends past the MediaBox or the CropBox |

- **Text** is judged character by character. In a word that straddles the
  box's edge, the characters inside it count. A character on the edge, or
  overlapping it, counts as inside. The character's box is the one Poppler
  gives, not where the glyph actually puts ink. **Appearance** is compared
  on the pixels inside the box.
- **The pages are lined up on the box's top left corner**, after `/Rotate`,
  rather than the CropBox's. So a page with bleed matches the same page
  trimmed.
- **Excluded margins are measured from the box's edges.**
- **The views and saved differences show the whole page**: the CropBox and
  the box together. Everything outside the box is dimmed, the box is
  outlined in blue, and nothing outside it is highlighted.
- **Warnings** are given once per file, or for the pair, with the pages
  they apply to, when:
  - the box is absent (`absent`);
  - with `crop`, the box extends past the CropBox (`beyond-crop`);
  - with `none`, it extends past the MediaBox (`beyond-media`);
  - nothing is left once it is clipped (`empty`);
  - the two files' boxes differ in size or position on the page by more
    than 0.01pt (`differ`, for the pair);
  - `--box-clip` is given without `--box`, and so ignored
    (`clip-without-box`).

  `disparPDFc` writes them to standard error and leaves the result code
  alone. With `--xmlResult` they also go into the XML, as a `<warnings>`
  element holding one `<warning>` per kind: `kind` is the name in brackets
  above, `file` is 1 or 2 (left out for the pair), `pages` lists the pages,
  e.g. `1-3, 5` (for `differ`, FILE1/FILE2 pairs such as `3/4` when the
  page numbers differ), and `message` is the text. The GUI writes them to
  the log and counts them in the status line.
- **In the GUI**, changing the box or the clipping forgets the
  comparison, which was made inside another box, so click Compare again.
  The choice is kept for next time, but one given with `--box` or
  `--box-clip` when starting the program is used without being saved,
  unless it is changed in the window.
- Boxes, like margins, are in default user space units: `/UserUnit`
  (PDF 1.6) is not applied.
- The boxes are read, and the pages rewritten to them, with
  [qpdf](https://qpdf.sourceforge.io/). A file qpdf cannot read is reported
  as unreadable (`-3` or `-4`) rather than compared whole.

## Settings

The GUI keeps its settings per user, on macOS in
`~/Library/Preferences/com.disparpdf.disparPDF.plist`, and on Linux and
the BSDs in `~/.config/disparPDF/disparPDF.conf` (an INI file).

`disparPDFc` ignores that file and reads an INI file given with
`--settings=FILE`, which is how a scripted comparison gets non-default
options:

```sh
disparPDFc -a --settings=mysettings.ini a.pdf b.pdf
```

Most keys below also have a control in the GUI; they are listed so that an
INI file for `disparPDFc` can be written by hand. The three in the last
table have no GUI control at all.

### Comparison

| Key | Default | GUI | Meaning |
|---|---|---|---|
| `InitialComparisonMode` | `2` | Compare box | Mode the GUI starts in: 0 appearance, 1 characters, 2 words. Batch mode ignores this and compares appearance unless `-a`, `-c` or `-w` is given |
| `Margins/Exclude` | `false` | Exclude Margins | Ignore everything outside the margins below |
| `Margins/Top`, `/Bottom`, `/Left`, `/Right` | `0` | Margins dock | Margin sizes, in points; inside a page box, from its edges |
| `PageBox/Box` | `none` | Page Box dock ▸ Page boundary | Compare only inside this page box: `none`, `media`, `crop`, `bleed`, `trim` or `art` (see [Comparing inside a page box](#comparing-inside-a-page-box)). `--box` overrides it |
| `PageBox/Clip` | `media` | Page Box dock ▸ Clipped to | What that box is clipped to: `media`, `crop` or `none`. `--box-clip` overrides it |
| `Zoning/Enable` | `false` | Zoning | Group text into zones before comparing |
| `Columns` | `1` | Columns | How many columns the page has; improves zoning |
| `Tolerance/R` | `8` | Tolerance/R | Largest distance (4–144) between word rectangles for them to land in the same zone |
| `Tolerance/Y` | `10` | Tolerance/Y | Text *y* coordinates are rounded to this (0–32) when zoning |
| `RequirePdfExtension` | `true` | Options ▸ Files to compare | Accept only files named `*.pdf` on the command line and show only those in the file dialogs. Batch mode ignores this and always requires `*.pdf` unless `--any-extension` is given |

### Display and highlighting

| Key | Default | GUI | Meaning |
|---|---|---|---|
| `Zoom` | `100` | Zoom | View magnification, per cent |
| `Outline`, `Fill` | — | Options ▸ Highlighting | Pen and brush for highlights. These are serialised Qt values, so set them through the dialog rather than by hand |
| `Opacity` | `13` | Options ▸ Fill Opacity | Fill opacity, per cent |
| `RuleWidth` | `1.5` | Options ▸ Rule width | Width of the highlight outline |
| `SquareSize` | `10` | Options ▸ Square Size | Size of the highlight square, in pixels |
| `CombineTextHighlighting` | `true` | Options ▸ Combine Highlighting | Merge adjacent highlights in the text modes |
| `ShowToolTips` | `true` | Options ▸ Show Tooltips | Show tool tips in the main window |
| `CacheSizeMB` | `25` | Options ▸ Cache Size | Limit on the rendered-page cache, in MB |

### No GUI control

| Key | Default | Meaning |
|---|---|---|
| `Overlap` | `5` | How far highlight rectangles may overlap before they are merged |
| `CompareThreads` | `0` | Upper bound on comparison worker threads; `0` means one per core. Each worker opens its own copy of both documents, so lowering this lowers peak memory. On a 1536-page appearance comparison across 8 cores, peak RSS was 223 MB at `0`, 152 MB at `4` and 89 MB at `1` — against 74 MB before the comparison was threaded, and 2.8 s instead of 1.4 s |
| `compositionMode` | `-1` | `QPainter::CompositionMode` used to draw appearance differences; `-1` draws the ordinary highlight. This is the batch-mode counterpart of the GUI's highlighting-mode box: 22 Difference, 23 Exclusion, 26 Src Xor Dest, 29 Not Src Xor Dest |

## License

GPL-2.0-or-later

Copyright © 2026 Yuwsuke Kieda  
Based on ConfrontaPDF © 2015 Luca Bellonda  
Based on DiffPDF © 2008–2013 Qtrac Ltd. (Mark Summerfield)
