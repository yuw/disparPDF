# disparPDF

2つのPDFファイルのテキストまたは外観を比較するツールです。

**disparPDF**はLuca Bellondaによる[ConfrontaPDF](https://github.com/lbellonda/ConfrontaPDF)（2015年）をQt6に移植したものです。ConfrontaPDF自体はMark Summerfield による[DiffPDF](http://www.qtrac.eu/diffpdf-foss.html)（2008–2013年）のフォークです。

このQt6移植版はYuwsuke Kiedaが2026年にAIツール（Claude by Anthropic）の支援を受けて作成しました。

## 機能

- 2つのPDFをページ単位で比較（テキストモード・外観モード）
- 単語単位・文字単位の比較
- ページ範囲の指定
- バッチ・コマンドラインモード（`disparPDFc`）
- マージン除外

## 依存関係のインストール（macOS / Homebrew）

Homebrewの`poppler`はQt6バインディングを含まないため、
このリポジトリの`packaging/homebrew/poppler-qt6.rb`を使って個人tapからインストールします。

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

## ビルド

```sh
export PKG_CONFIG_PATH="/opt/homebrew/opt/poppler-qt6/lib/pkgconfig"
cmake -B build \
  -DCMAKE_PREFIX_PATH="/opt/homebrew/opt/qt@6" \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(sysctl -n hw.logicalcpu)
open build/disparPDF.app
```

## コマンドライン使用例

```sh
# 同一なら0、差異があれば非0を返す
disparPDFc -b a.pdf b.pdf

# 詳細出力
disparPDFc -b --outType=1 a.pdf b.pdf

# XML出力
disparPDFc -b --xmlResult=result.xml a.pdf b.pdf
```

## ライセンス

GPL-2.0-or-later

Copyright © 2026 Yuwsuke Kieda  
Based on ConfrontaPDF © 2015 Luca Bellonda  
Based on DiffPDF © 2008–2013 Qtrac Ltd. (Mark Summerfield)
