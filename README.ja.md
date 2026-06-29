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

## Homebrewによるインストール（推奨）

```sh
brew tap yuw/disparPDF
brew trust yuw/disparPDF
brew install yuw/disparPDF/disparPDF
```

インストール後、`/Applications` へのコピーを手動で行います：

```sh
cp -r /opt/homebrew/opt/disparpdf/disparPDF.app /Applications/
codesign --force --sign - /Applications/disparPDF.app/Contents/MacOS/disparPDF
```

インストール後の配置：

| 場所 | 説明 |
|---|---|
| `/Applications/disparPDF.app` | GUIアプリ（Finder用） |
| `/opt/homebrew/opt/disparPDF/disparPDF.app` | Homebrew管理下のコピー |
| `/opt/homebrew/bin/disparPDF` | CLIラッパー（GUIを起動） |
| `/opt/homebrew/bin/disparPDFc` | CLIバッチモード |

## 手動インストールからHomebrewへの移行

手動でビルド・インストールした環境からHomebrewに移行する手順です。

```sh
# 1. Homebrew tapでインストール
brew tap yuw/disparPDF
brew trust yuw/disparPDF
brew install yuw/disparPDF/disparPDF

# 2. インストールの確認
brew info yuw/disparPDF/disparPDF
ls /opt/homebrew/bin/disparPDF
ls /opt/homebrew/bin/disparPDFc

# 3. 手動インストール分を削除
sudo rm -f /usr/local/bin/disparPDF
sudo rm -f /usr/local/bin/disparPDFc
sudo rm -rf /usr/local/disparPDF.app
sudo rm -rf /Applications/disparPDF.app

# 4. /Applications にコピー
cp -r /opt/homebrew/opt/disparpdf/disparPDF.app /Applications/
codesign --force --sign - /Applications/disparPDF.app/Contents/MacOS/disparPDF

# 5. 動作確認
open /Applications/disparPDF.app
disparPDFc -b 2>&1 | head -1
```

## ソースからのビルド

### 依存関係のインストール（macOS / Homebrew）

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

### ビルド

```sh
export PKG_CONFIG_PATH="/opt/homebrew/opt/poppler-qt6/lib/pkgconfig"
cmake -B build \
  -DCMAKE_PREFIX_PATH="/opt/homebrew/opt/qt@6" \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(sysctl -n hw.logicalcpu)
```

### インストール

```sh
# /usr/local にインストール
sudo cmake --install build --prefix /usr/local

# インストール後に再署名（macOS 26以降で必要）
codesign --force --sign - /usr/local/disparPDF.app/Contents/MacOS/disparPDF

# /Applications にコピー（任意）
cp -r /usr/local/disparPDF.app /Applications/
codesign --force --sign - /Applications/disparPDF.app/Contents/MacOS/disparPDF

# CLIから呼び出せるようにシンボリックリンクを作成（任意）
sudo ln -sf /usr/local/disparPDF.app/Contents/MacOS/disparPDF /usr/local/bin/disparPDF
```

## 使い方

### GUI

```sh
# Finderから起動
open /Applications/disparPDF.app

# ターミナルからファイルを指定して起動
disparPDF a.pdf b.pdf
```

### コマンドライン（バッチモード）

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
