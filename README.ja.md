# disparPDF

2つのPDFファイルのテキストまたは外観を比較するツールです．

**disparPDF**はLuca Bellondaによる[ConfrontaPDF](https://github.com/lbellonda/ConfrontaPDF)（2015年）をQt6に移植したものです．ConfrontaPDF自体はMark Summerfieldによる[DiffPDF](http://www.qtrac.eu/diffpdf-foss.html)（2008–2013年）のフォークです．

このQt6移植版はYuwsuke Kiedaが2026年にAIツール（Claude by Anthropic）の支援を受けて作成しました．

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

`disparPDF` / `disparPDFc`コマンドはこの時点で使えます．Finder・Dock・Spotlightから
使えるよう`/Applications`にも置く場合は，手動でコピーします：

```sh
ditto /opt/homebrew/opt/disparpdf/disparPDF.app /Applications/disparPDF.app
```

macOSのApp Management保護により，`/Applications`にある既存の.appバンドルをHomebrewから
書き換えることはできません．**そのため`brew upgrade`のたびに上のコマンドを
実行してください**（しないとFinder側だけ旧バージョンのまま残ります）．

`cp -r`ではなく`ditto`を使ってください．既存バンドルがある状態で`cp -r`を使うと，
置き換えではなく古いバンドルの中に入れ子でコピーされてしまいます．`ditto`は
コード署名を保持するため，再署名は不要です．

インストール後の配置：

| 場所 | 説明 |
|---|---|
| `/Applications/disparPDF.app` | GUIアプリ（Finder用） |
| `/opt/homebrew/opt/disparPDF/disparPDF.app` | Homebrew管理下のコピー |
| `/opt/homebrew/bin/disparPDF` | CLIラッパー（GUIを起動） |
| `/opt/homebrew/bin/disparPDFc` | CLIバッチモード |

## アップグレード

```sh
brew update
brew upgrade yuw/disparPDF/poppler-qt6 yuw/disparPDF/disparPDF
```

`disparPDF` / `disparPDFc`コマンドはこれだけで最新になります．

**`poppler-qt6`だけが更新された場合**は，新しいバインディングに対してビルドし直してください．Homebrewは
依存先が更新されただけではformulaを再ビルドしないため，そのままでは古いPopplerに対して
リンクされたバイナリが使われ続けます：

```sh
brew reinstall yuw/disparPDF/disparPDF
```

**`/Applications`にコピーを置いている場合**は，アップグレードのたびに更新してください．macOSは`/Applications`にある
既存の.appバンドルへのHomebrewからの書き込みを許可しないため，この手順は自動化できません：

```sh
ditto /opt/homebrew/opt/disparpdf/disparPDF.app /Applications/disparPDF.app
```

インストール状況の確認と，入れ替わって不要になった旧バージョンの削除：

```sh
brew list --versions disparPDF poppler-qt6
brew cleanup
```

## 手動インストールからHomebrewへの移行

手動でビルド・インストールした環境からHomebrewに移行する手順です．

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

# 4. /Applicationsにコピー
ditto /opt/homebrew/opt/disparpdf/disparPDF.app /Applications/disparPDF.app

# 5. 動作確認
open /Applications/disparPDF.app
disparPDFc -b 2>&1 | head -1
```

## ソースからのビルド

### 依存関係のインストール（macOS / Homebrew）

Homebrewの`poppler`はQt6バインディングを含まないため，
このリポジトリの`packaging/homebrew/poppler-qt6.rb`を使って個人tapからインストールします．

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

macOSではHomebrewのkeg-onlyな`qt@6` / `poppler-qt6`をCMakeが自動的に
探索するため，環境変数の設定は不要です:

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(sysctl -n hw.logicalcpu)
```

別の場所にあるQt / Popplerを使う場合は明示的に指定します．明示指定した`CMAKE_PREFIX_PATH`は
自動探索したパスより優先されます:

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="/path/to/qt6;/path/to/poppler-qt6"
```

### インストール

```sh
# /usr/localにインストール
sudo cmake --install build --prefix /usr/local

# インストール後に再署名（macOS 26以降で必要）
codesign --force --sign - /usr/local/disparPDF.app/Contents/MacOS/disparPDF

# /Applicationsにコピー（任意）
# dittoは既存バンドルを置き換え，署名も保持する
# （cp -rだと古いバンドルの中に入れ子でコピーされてしまう）
ditto /usr/local/disparPDF.app /Applications/disparPDF.app

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
# 同一なら0，差異があれば非0を返す
disparPDFc -b a.pdf b.pdf

# 詳細出力
disparPDFc -b --outType=1 a.pdf b.pdf

# XML出力
disparPDFc -b --xmlResult=result.xml a.pdf b.pdf
```

`disparPDFc`は別のプログラムではなく，`disparPDF`へのシンボリックリンク
（Windowsではそのコピー）です．この名前で起動するとバッチモードになるので
`-b`は省略でき，ディスプレイも不要です．`disparPDF -b`でも同じです．
`disparPDFc --interactive`では`disparPDF`と同様にウィンドウを表示します．

## 設定

GUIはユーザーごとに設定を保存します．macOSでは
`~/Library/Preferences/com.disparpdf.disparPDF.plist`，LinuxとBSDでは
`~/.config/disparPDF/disparPDF.conf`（INIファイル）です．

`disparPDFc`はこのファイルを読まず，`--settings=FILE`で指定したINIファイルを
読みます．既定値以外の条件でスクリプトから比較する場合はこれを使います：

```sh
disparPDFc -a --settings=mysettings.ini a.pdf b.pdf
```

以下のうち大半はGUIにも操作個所があります．`disparPDFc`向けのINIファイルを
手で書けるように一覧にしてあります．最後の2つだけはGUIに操作個所がありません．

### 比較

| キー | 既定値 | GUI | 意味 |
|---|---|---|---|
| `InitialComparisonMode` | `2` | 比較モードの選択 | GUIの起動時モード．0=外観，1=文字，2=単語．バッチモードはこれを見ず，`-a`・`-c`・`-w`のいずれも指定しなければ外観比較になる |
| `Margins/Exclude` | `false` | マージンを除外 | 下記マージンの外側を比較対象から外す |
| `Margins/Top`・`/Bottom`・`/Left`・`/Right` | `0` | マージンのドック | マージンの大きさ（ポイント） |
| `Zoning/Enable` | `false` | ゾーニング | 比較前にテキストをゾーンにまとめる |
| `Columns` | `1` | 段数 | ページの段数．ゾーニングの精度が上がる |
| `Tolerance/R` | `8` | Tolerance/R | 同一ゾーンとみなす単語矩形間の最大距離（4〜144） |
| `Tolerance/Y` | `10` | Tolerance/Y | ゾーニング時にテキストの*y*座標を丸める単位（0〜32） |
| `RequirePdfExtension` | `true` | オプション ▸ 比較するファイル | コマンドラインで名前が`*.pdf`のファイルだけを受け付け，ファイルダイアログにもそれだけを表示する．バッチモードはこれを見ず，`--any-extension`を指定しない限り常に`*.pdf`が必要 |

### 表示とハイライト

| キー | 既定値 | GUI | 意味 |
|---|---|---|---|
| `Zoom` | `100` | ズーム | 表示倍率（パーセント） |
| `Outline`・`Fill` | — | オプション ▸ ハイライト | ハイライトのペンとブラシ．Qtの値をシリアライズしたものなので，手で書かずダイアログから設定する |
| `Opacity` | `13` | オプション ▸ 塗りの不透明度 | 塗りの不透明度（パーセント） |
| `RuleWidth` | `1.5` | オプション ▸ 線幅 | ハイライトの輪郭線の太さ |
| `SquareSize` | `10` | オプション ▸ 四角のサイズ | ハイライトの四角の大きさ（ピクセル） |
| `CombineTextHighlighting` | `true` | オプション ▸ ハイライトを結合 | テキストモードで隣接するハイライトをまとめる |
| `Overlap` | `5` | — | ハイライト矩形を結合する重なりの許容量 |
| `ShowToolTips` | `true` | オプション ▸ ツールチップを表示 | メインウィンドウでツールチップを表示する |
| `CacheSizeMB` | `25` | オプション ▸ キャッシュサイズ | レンダリング済みページのキャッシュ上限（MB） |

### GUIに操作個所がない設定

| キー | 既定値 | 意味 |
|---|---|---|
| `CompareThreads` | `0` | 比較に使うワーカースレッド数の上限．`0`はコア数分．各ワーカーが2つの文書を個別に開くため，値を下げるとピークメモリも下がる．1536ページの外観比較を8コアで実行した場合，最大RSSは`0`で223MB，`4`で152MB，`1`で89MB（並列化前は74MB）．所要時間は1.4秒に対し2.8秒 |
| `compositionMode` | `-1` | 外観比較の差異を描画する`QPainter::CompositionMode`の値．`-1`で通常のハイライトになる．GUIのハイライトモード選択に相当する．22=Difference，23=Exclusion，26=Src Xor Dest，29=Not Src Xor Dest |

## ライセンス

GPL-2.0-or-later

Copyright © 2026 Yuwsuke Kieda  
Based on ConfrontaPDF © 2015 Luca Bellonda  
Based on DiffPDF © 2008–2013 Qtrac Ltd. (Mark Summerfield)
