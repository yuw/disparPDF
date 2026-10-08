class Disparpdf < Formula
  desc "PDF comparison tool — compares text or visual appearance of two PDF files"
  homepage "https://github.com/yuw/disparPDF"

  # リリースタグを打った後は以下の url/sha256 をタグのものに更新する:
  #   url "https://github.com/yuw/disparPDF/archive/refs/tags/v1.0.tar.gz"
  #   sha256 "<brew fetch でのhash>"
  url "https://github.com/yuw/disparPDF/archive/refs/heads/master.tar.gz"
  version "1.0"
  sha256 :no_check

  license any_of: ["GPL-2.0-or-later"]

  depends_on "cmake"   => :build
  depends_on "pkgconf" => :build
  depends_on "qt@6"
  depends_on "yuw/disparPDF/poppler-qt6"

  def install
    poppler_qt6_prefix = Formula["yuw/disparPDF/poppler-qt6"].opt_prefix
    qt6_prefix         = Formula["qt@6"].opt_prefix

    system "cmake", "-S", ".", "-B", "build",
      *std_cmake_args,
      "-DCMAKE_PREFIX_PATH=#{qt6_prefix};#{poppler_qt6_prefix}",
      "-DCMAKE_BUILD_TYPE=Release"

    system "cmake", "--build", "build", "-j#{ENV.make_jobs}"

    # GUI アプリ
    prefix.install "build/disparPDF.app"

    # CLI: .app 内の本体へのシンボリックリンク。disparPDFc という名前で
    # 起動すると常にバッチモードになる（main.cpp）。open を経由しないので
    # 標準出力と終了ステータスがそのまま返る
    bin.install_symlink prefix/"disparPDF.app/Contents/MacOS/disparPDF" => "disparPDFc"

    # GUI を bin からも呼び出せるようにラッパースクリプトを作成
    # 引数を絶対パスに変換してから渡す（相対パスだと cannot load エラーになる）
    (bin/"disparPDF").write <<~SHELL
      #!/bin/sh
      args=""
      for f in "$@"; do
        case "$f" in
          -*) args="$args $f" ;;
          *)  args="$args $(cd "$(dirname "$f")" 2>/dev/null && pwd)/$(basename "$f")" ;;
        esac
      done
      exec open "#{prefix}/disparPDF.app" --args $args
    SHELL
    chmod 0755, bin/"disparPDF"
  end

  def post_install
    # install_name_tool による変更後に再署名（macOS 26以降で必須）
    system "codesign", "--force", "--sign", "-",
           "#{prefix}/disparPDF.app/Contents/MacOS/disparPDF"
  end

  # 以前はここで /Applications へコピーしていたが、macOS 13 以降の TCC
  # (App Management) により、自分がインストールしたのではない /Applications 内の
  # .app バンドルは brew から書き換えられない ("Operation not permitted")。
  # 無言でスキップされ GUI だけ旧バージョンのまま残るため、手順を caveats に移した。
  def caveats
    <<~EOS
      disparPDF.app has been installed to:
        #{opt_prefix}/disparPDF.app

      The `disparPDF` command always launches the copy above, so it is
      up to date immediately after every `brew upgrade`.

      To also have it in /Applications (for Finder, Dock and Spotlight),
      copy it there yourself. macOS does not let Homebrew do this, so the
      command has to be repeated after each upgrade:

        ditto #{opt_prefix}/disparPDF.app /Applications/disparPDF.app

      ditto preserves the code signature, so no re-signing is needed.

      CLI commands available:
        disparPDF   — launch GUI with optional file arguments
        disparPDFc  — batch/command line mode
    EOS
  end

  test do
    assert_predicate prefix/"disparPDF.app", :exist?
    assert_predicate bin/"disparPDFc", :exist?
    assert_match "disparPDFc", shell_output("#{bin}/disparPDFc --version")
  end
end
