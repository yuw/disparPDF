class PopplerQt6 < Formula
  desc "Qt6 bindings for the Poppler PDF rendering library"
  homepage "https://poppler.freedesktop.org/"

  url "https://poppler.freedesktop.org/poppler-26.06.0.tar.xz"
  sha256 "4cb4e5a3dc8cb5eec751c8a23c8ba19f61f96dedc0cd07d2aee6b0c8e2cf6ba4"
  license any_of: ["GPL-2.0-only", "GPL-3.0-only"]

  patch do
    url "https://gitlab.freedesktop.org/poppler/poppler/-/commit/e263f50b8ecac8aaad458a4c45d8ca9761dd8878.diff"
    sha256 "b61ff6d4a474503f00bdd96a0bf60ee245adc9e23b77bba2096da47da182513a"
  end

  depends_on "cmake"   => :build
  depends_on "pkgconf" => :build
  depends_on "qt@6"
  depends_on "poppler"
  depends_on "cairo"
  depends_on "fontconfig"
  depends_on "freetype"
  depends_on "glib"
  depends_on "gpgmepp"
  depends_on "jpeg-turbo"
  depends_on "libpng"
  depends_on "libtiff"
  depends_on "little-cms2"
  depends_on "nspr"
  depends_on "nss"
  depends_on "openjpeg"
  on_macos do
    depends_on "gettext"
    depends_on "gpgme"
  end
  on_linux do
    depends_on "zlib-ng-compat"
  end

  keg_only "installs Qt6 Poppler headers that conflict with the poppler formula"

  def install
    poppler_prefix = Formula["poppler"].opt_prefix
    qt6_prefix     = Formula["qt@6"].opt_prefix

    args = std_cmake_args + %W[
      -DCMAKE_PREFIX_PATH=#{qt6_prefix};#{poppler_prefix}
      -DBUILD_GTK_TESTS=OFF
      -DBUILD_QT6_TESTS=OFF
      -DENABLE_BOOST=OFF
      -DENABLE_CMS=lcms2
      -DENABLE_GLIB=OFF
      -DENABLE_QT5=OFF
      -DENABLE_QT6=ON
      -DENABLE_UNSTABLE_API_ABI_HEADERS=ON
      -DWITH_GObjectIntrospection=OFF
      -DCMAKE_INSTALL_RPATH=#{rpath}
    ]

    system "cmake", "-S", ".", "-B", "build", *args
    system "cmake", "--build", "build", "--target", "poppler-qt6"

    # ── ヘッダ ──────────────────────────────────────────────────────────
    qt6_inc = include/"poppler/qt6"
    qt6_src = buildpath/"qt6/src"
    qt6_inc.install qt6_src/"poppler-qt6.h",
                    qt6_src/"poppler-annotation.h",
                    qt6_src/"poppler-converter.h",
                    qt6_src/"poppler-form.h",
                    qt6_src/"poppler-link.h",
                    qt6_src/"poppler-media.h",
                    qt6_src/"poppler-optcontent.h",
                    qt6_src/"poppler-page-transition.h"

    %w[poppler-export.h poppler-version.h].each do |hdr|
      found = Dir["#{buildpath}/build/**/#{hdr}"].first
      qt6_inc.install found if found
    end

    # ── ライブラリ ───────────────────────────────────────────────────────
    lib.install Dir["build/qt6/src/libpoppler-qt6*"]

    # ── libpoppler-qt6 が @rpath で参照する libpoppler を
    #    Homebrew の poppler 本体の絶対パスに書き換える。
    #    同時に poppler 本体の dylib へのシンボリックリンクを lib/ に作成し、
    #    macOS の dyld が rpath 解決時にこのフォーミュラの lib/ を見つけられるようにする。
    poppler_lib = poppler_prefix/"lib"
    Dir["#{poppler_lib}/libpoppler.*.dylib"].each do |src|
      ln_sf src, lib/File.basename(src)
    end

    Dir["#{lib}/libpoppler-qt6.*.*.*.dylib"].each do |qt6_dylib|
      MachO::Tools.change_install_name(
        qt6_dylib,
        "@rpath/libpoppler.161.dylib",
        "#{poppler_lib}/libpoppler.161.dylib"
      )
      # 変更後に再署名（macOS 26以降はコード署名の変更を検出するため必須）
      system "codesign", "--force", "--sign", "-", qt6_dylib
    end

    # ── pkg-config ───────────────────────────────────────────────────────
    (lib/"pkgconfig/poppler-qt6.pc").write <<~PC
      prefix=#{prefix}
      exec_prefix=${prefix}
      libdir=${prefix}/lib
      includedir=${prefix}/include

      Name: poppler-qt6
      Description: Qt6 bindings for poppler
      Version: 26.06.0
      Requires: poppler
      Libs: -L${libdir} -lpoppler-qt6
      Cflags: -I${includedir}/poppler/qt6 -I#{poppler_prefix}/include/poppler
    PC
  end

  test do
    system Formula["pkgconf"].opt_bin/"pkg-config", "--exists", "poppler-qt6"
  end
end
