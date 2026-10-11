class Disparpdf < Formula
  desc "PDF comparison tool — compares text or visual appearance of two PDF files"
  homepage "https://github.com/yuw/disparPDF"

  # リリースタグを打った後は以下のurl/sha256をタグのものに更新する:
  #   url "https://github.com/yuw/disparPDF/archive/refs/tags/v1.0.tar.gz"
  #   sha256 "<brew fetchでのhash>"
  url "https://github.com/yuw/disparPDF/archive/refs/heads/master.tar.gz"
  version "1.0"
  sha256 :no_check

  license any_of: ["GPL-2.0-or-later"]

  depends_on "cmake"     => :build
  depends_on "help2man"  => :build
  depends_on "pkgconf"   => :build
  depends_on "qpdf"
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

    # .app本体，disparPDFc（.app内の本体への相対シンボリックリンク），
    # manページ，シェル補完，docをまとめて配置する．disparPDFcという名前で
    # 起動すると常にバッチモードになる（main.cpp）．openを経由しないので
    # 標準出力と終了ステータスがそのまま返る
    system "cmake", "--install", "build"

    # GUIをbinからも呼び出せるようにラッパースクリプトを作成する．
    # cmake --installはmacOSでは.appバンドルとCLIのリンクだけを置き，
    # bin/disparPDFは作らないのでここで補う．
    # 引数を絶対パスに変換してから渡す（相対パスだとcannot loadエラーになる）
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
    # install_name_toolによる変更後に再署名（macOS 26以降で必須）
    system "codesign", "--force", "--sign", "-",
           "#{prefix}/disparPDF.app/Contents/MacOS/disparPDF"
  end

  # 以前はここで/Applicationsへコピーしていたが，macOS 13以降のTCC
  # (App Management)により，自分がインストールしたのではない/Applications内の
  # .appバンドルはbrewから書き換えられない（"Operation not permitted"）．
  # 無言でスキップされGUIだけ旧バージョンのまま残るため，手順をcaveatsに移した．
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

      "man disparPDF" covers both, and shell completions for bash and zsh
      are installed.
    EOS
  end

  test do
    assert_predicate prefix/"disparPDF.app", :exist?
    assert_predicate bin/"disparPDFc", :exist?
    assert_match "disparPDFc", shell_output("#{bin}/disparPDFc --version")
    # cmake --installが置くもの．補完はCMAKE_INSTALL_DATADIR配下に入るため，
    # etc/bash_completion.dを指すbash_completionヘルパーでは見つからない
    assert_predicate man1/"disparPDF.1", :exist?
    assert_predicate share/"bash-completion/completions/disparPDFc", :exist?
    assert_predicate share/"zsh/site-functions/_disparPDF", :exist?
    # --helpはウィンドウを開かずに終了コード0で返る
    assert_match "Usage: disparPDFc", shell_output("#{bin}/disparPDFc --help")
  end
end
