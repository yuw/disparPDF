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
- Page range specification
- Batch/command line mode (`disparPDFc`)
- Margin exclusion

## Dependencies (macOS / Homebrew)

Homebrew's `poppler` does not include Qt6 bindings.
Use the `poppler-qt6.rb` formula in `packaging/homebrew/` to install via a local tap.

```sh
brew install qt@6

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

## Build

```sh
export PKG_CONFIG_PATH="/opt/homebrew/opt/poppler-qt6/lib/pkgconfig"
cmake -B build \
  -DCMAKE_PREFIX_PATH="/opt/homebrew/opt/qt@6" \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(sysctl -n hw.logicalcpu)
```

## Install

```sh
# Install to /usr/local
sudo cmake --install build --prefix /usr/local

# Re-sign after install (required on macOS 26+)
codesign --force --sign - /usr/local/disparPDF.app/Contents/MacOS/disparPDF

# Optional: add symlink for CLI use
sudo ln -sf /usr/local/disparPDF.app/Contents/MacOS/disparPDF /usr/local/bin/disparPDF
```

## Usage

### GUI

```sh
# Launch from Finder
open /usr/local/disparPDF.app

# Launch with files from terminal
open /usr/local/disparPDF.app --args a.pdf b.pdf

# If symlink is set up
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

## License

GPL-2.0-or-later

Copyright © 2026 Yuwsuke Kieda  
Based on ConfrontaPDF © 2015 Luca Bellonda  
Based on DiffPDF © 2008–2013 Qtrac Ltd. (Mark Summerfield)
