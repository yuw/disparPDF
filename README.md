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

```sh
brew install qt@6
brew tap yuw/disparPDF ~/src/homebrew-confrontapdf
brew install yuw/disparPDF/poppler-qt6
```

## Build

```sh
export PKG_CONFIG_PATH="/opt/homebrew/opt/poppler-qt6/lib/pkgconfig"
cmake -B build \
  -DCMAKE_PREFIX_PATH="/opt/homebrew/opt/qt@6" \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(sysctl -n hw.logicalcpu)
open build/disparPDF.app
```

## Command line usage

```sh
# 同一なら 0、差異があれば非0 を返す
disparPDFc -b a.pdf b.pdf

# 詳細出力
disparPDFc -b --outType=1 a.pdf b.pdf

# XML 出力
disparPDFc -b --xmlResult=result.xml a.pdf b.pdf
```

## License

GPL-2.0-or-later

Copyright © 2026 Yuwsuke Kieda  
Based on ConfrontaPDF © 2015 Luca Bellonda  
Based on DiffPDF © 2008–2013 Qtrac Ltd. (Mark Summerfield)
