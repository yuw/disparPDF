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

    # CLI バイナリ
    bin.install "build/disparPDFc"

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

    # /Applications にもコピー
    apps_dir = Pathname.new("/Applications")
    if apps_dir.exist? && apps_dir.writable?
      system "cp", "-r", "#{prefix}/disparPDF.app", "/Applications/disparPDF.app"
      system "codesign", "--force", "--sign", "-",
             "/Applications/disparPDF.app/Contents/MacOS/disparPDF"
    end
  end

  def caveats
    <<~EOS
      disparPDF.app has been installed to:
        #{opt_prefix}/disparPDF.app
        /Applications/disparPDF.app (if /Applications is writable)

      CLI commands available:
        disparPDF   — launch GUI with optional file arguments
        disparPDFc  — batch/command line mode
    EOS
  end

  test do
    assert_predicate prefix/"disparPDF.app", :exist?
    assert_predicate bin/"disparPDFc", :exist?
  end
end
