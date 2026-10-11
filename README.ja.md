# disparPDF

2つのPDFファイルのテキストまたは外観を比較するツールです．

**disparPDF**はLuca Bellondaによる[ConfrontaPDF](https://github.com/lbellonda/ConfrontaPDF)（2015年）をQt6に移植したものです．ConfrontaPDF自体はMark Summerfieldによる[DiffPDF](http://www.qtrac.eu/diffpdf-foss.html)（2008–2013年）のフォークです．

このQt6移植版はYuwsuke Kiedaが2026年にAIツール（Claude by Anthropic）の支援を受けて作成しました．

## 機能

- 2つのPDFをページ単位で比較（テキストモード・外観モード）
- 単語単位・文字単位の比較
- ページ範囲の指定と，一方の文書にだけページがある場合のページオフセット
- マージン除外
- ページボックス（MediaBox，CropBox，BleedBox，TrimBox，ArtBox）の中だけの
  比較（`--box`）．ページはボックスの角を基準に重ねる
- フォントの比較（`--compareFonts`）
- コアごとに1スレッドでのページの並列比較
- バッチ・コマンドラインモード（`disparPDFc`）．`--interactive`でウィンドウも表示できる
- manページ（`man disparPDF`）とbash／zshの補完

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
ページボックスの読み取りと書き換えのために，[qpdf](https://qpdf.sourceforge.io/)も必要です．

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

### テスト

テストは小さなPDFに対してバッチモードを実行するもので，ディスプレイを必要としません:

```sh
ctest --test-dir build
```

内容は`test/CMakeLists.txt`に記述されています．テストを含めない場合は
`-DBUILD_TESTING=OFF`を指定して構成します．

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

LinuxとBSDでは`cmake --install`がmanページ（`man disparPDF`または
`man disparPDFc`），bashとzshの補完（置き場所は`BASH_COMPLETION_DIR`と
`ZSH_COMPLETION_DIR`で指定できます），アプリケーションメニュー用のdesktop
ファイルとアイコン，READMEもインストールします．パッケージ作成時は`DESTDIR`と
通常の`CMAKE_INSTALL_*`ディレクトリが使えます．manページは`--help`の出力から
`help2man`が生成します．`help2man`がない場合やクロスコンパイル時は`doc_man/`に
あるコピーがインストールされます（このコピーは
`cmake --build build --target update-manpage`で更新します）．

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

```sh
# TrimBoxの中だけを比較する．トンボ，カラーバー，塗り足しは無視され，
# 塗り足しのあるページは，塗り足しのない同じページと一致する
disparPDFc -b --box=trim a.pdf b.pdf
```

`disparPDFc`は別のプログラムではなく，`disparPDF`へのシンボリックリンク
（Windowsではそのコピー）です．この名前で起動するとバッチモードになるので
`-b`は省略でき，ディスプレイも不要です．`disparPDF -b`でも同じです．
`disparPDFc --interactive`では`disparPDF`と同様にウィンドウを表示します．

### ページボックスの中だけを比較する

PDFのページには，最大で5つの境界があります（ISO 32000-2:2020，14.11.2）．

- **MediaBox**：物理的な媒体の範囲
- **CropBox**：表示・印刷のときに内容を切り抜く範囲
- **BleedBox**：製版・印刷の工程で内容を切り抜く範囲
- **TrimBox**：断裁後の仕上がりの大きさ
- **ArtBox**：作成者が意図したページの意味のある内容の範囲

`--box=BOX`（GUIでは**Page Box**のドック）で，このうち1つの中だけを比較します．
`none`はページ全体を比べ，オプションを付けないときと同じです．

- **ページにそのボックスがないとき**は，規格の表31の既定値を使い，警告を出します．
  既定値は，BleedBox・TrimBox・ArtBoxならCropBox，CropBoxならMediaBoxです．
- **切り抜き方**は`--box-clip=CLIP`で選びます．

  | `CLIP` | 比較する範囲 |
  |---|---|
  | `media`（既定） | ボックスをMediaBoxで切り抜いた範囲．14.11.2.1が処理系に求めるとおり |
  | `crop` | ボックスをMediaBoxとCropBoxで切り抜いた範囲．ビューアに見える範囲だけ |
  | `none` | 書かれたとおりのボックス．MediaBoxやCropBoxの外に出ていてもそのまま |

- **テキスト**は文字ごとに判定します．
  - ボックスの辺にまたがる語は，ボックスの中にある文字だけを対象にします．
  - 辺の上にある文字や，辺にかかる文字は，中にあるとみなします．
  - 判定に使うのはpopplerが返す字の枠で，字面ではありません．
- **外観**は，ボックスの中の画素で比較します．
- **ページは，ボックスの左上の角を基準に重ねます**（CropBoxの角ではありません．
  角は`/Rotate`を適用した後のものです）．そのため，塗り足しのあるページは，
  断裁した同じページと一致します．
- **除外するマージンは，ボックスの辺から測ります．**
- **画面と保存した差分には，ページ全体を表示します**（CropBoxとボックスを合わせた範囲）．
  ボックスの外は淡く表示して青い枠線を引き，ボックスの外にはハイライトを付けません．
- **警告**は，次の場合にファイルごとに1回まとめ，該当するページを添えて出します．
  - ボックスがない
  - 2つのファイルでボックスの大きさか位置が0.01ptを超えて違う
  - `crop`でボックスがCropBoxの外に出る
  - `none`でボックスがMediaBoxの外に出る
  - 切り抜くと何も残らない

  出し先は次のとおりです．
  - `disparPDFc`：標準エラーと，`--xmlResult`のファイルの`<warnings>`．
    終了コードは変えません．
  - GUI：ログ．状態表示にも件数を出します．
- ボックスの読み取りと，ボックスに合わせたページの書き換えには，
  [qpdf](https://qpdf.sourceforge.io/)を使います．qpdfで読めないファイルは，
  ページ全体で比較するのではなく，読めないファイル（`-3`か`-4`）として報告します．

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
手で書けるように一覧にしてあります．最後の表の3つはGUIに操作個所がありません．

### 比較

| キー | 既定値 | GUI | 意味 |
|---|---|---|---|
| `InitialComparisonMode` | `2` | 比較モードの選択 | GUIの起動時モード．0=外観，1=文字，2=単語．バッチモードはこれを見ず，`-a`・`-c`・`-w`のいずれも指定しなければ外観比較になる |
| `Margins/Exclude` | `false` | マージンを除外 | 下記マージンの外側を比較対象から外す |
| `Margins/Top`・`/Bottom`・`/Left`・`/Right` | `0` | マージンのドック | マージンの大きさ（ポイント）．ページボックスの中で比べるときは，ボックスの辺から測る |
| `PageBox/Box` | `none` | Page Boxのドック ▸ Page boundary | このページボックスの中だけを比較する．`none`・`media`・`crop`・`bleed`・`trim`・`art`のいずれか（[ページボックスの中だけを比較する](#ページボックスの中だけを比較する)を参照）．`--box`で上書きできる |
| `PageBox/Clip` | `media` | Page Boxのドック ▸ Clipped to | そのボックスを何で切り抜くか．`media`・`crop`・`none`のいずれか．`--box-clip`で上書きできる |
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
| `ShowToolTips` | `true` | オプション ▸ ツールチップを表示 | メインウィンドウでツールチップを表示する |
| `CacheSizeMB` | `25` | オプション ▸ キャッシュサイズ | レンダリング済みページのキャッシュ上限（MB） |

### GUIに操作個所がない設定

| キー | 既定値 | 意味 |
|---|---|---|
| `Overlap` | `5` | ハイライト矩形を結合する重なりの許容量 |
| `CompareThreads` | `0` | 比較に使うワーカースレッド数の上限．`0`はコア数分．各ワーカーが2つの文書を個別に開くため，値を下げるとピークメモリも下がる．1536ページの外観比較を8コアで実行した場合，最大RSSは`0`で223MB，`4`で152MB，`1`で89MB（並列化前は74MB）．所要時間は1.4秒に対し2.8秒 |
| `compositionMode` | `-1` | 外観比較の差異を描画する`QPainter::CompositionMode`の値．`-1`で通常のハイライトになる．GUIのハイライトモード選択に相当する．22=Difference，23=Exclusion，26=Src Xor Dest，29=Not Src Xor Dest |

## ライセンス

GPL-2.0-or-later

Copyright © 2026 Yuwsuke Kieda  
Based on ConfrontaPDF © 2015 Luca Bellonda  
Based on DiffPDF © 2008–2013 Qtrac Ltd. (Mark Summerfield)
