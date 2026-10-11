#!/bin/sh
# Regenerates the test PDFs in this directory.  They are kept in the
# repository, so building and testing disparPDF does not need this script
# or Ghostscript; it records how they were made.
#
# Each file differs from base.pdf (3 pages of text) in one way:
#   same.pdf       nothing (regenerated, so not byte-identical)
#   word.pdf       one word on page 2
#   reflow.pdf     page 1's words broken into lines differently
#   reordered.pdf  page 1's lines drawn in reverse order, in the same places
#   bold.pdf       one line of page 1 drawn twice, so it looks bolder
#   box.pdf        a small box on page 1, 1.5in from the top
#   marginbox.pdf  a small box on page 1, 0.25in from the top (in a 1in margin)
#   inserted.pdf   an extra page between pages 1 and 2
#
# boxes.pdf is not compared with base.pdf: its pages have page boundaries
# (CropBox, BleedBox, TrimBox, ArtBox, /Rotate) for the unit tests of
# pagebox.cpp (see boxes_pages below).
#
# These have one page, page 1 of base.pdf, for comparing inside the trim
# box (--box=trim):
#   plain.pdf      no boxes
#   trim.pdf       a TrimBox of [36 36 576 756], with a mark outside it in
#                  the lower left
#   trim-mark.pdf  the same, but the mark in the lower right
#   trim-word.pdf  the same as trim.pdf, but with word.pdf's changed word
#   bleed.pdf      plain.pdf's page with 18pt of bleed all round: a larger
#                  MediaBox, the TrimBox where plain.pdf's page was, and a
#                  mark in the bleed
#
# (The other files here are described in README.)
#
# Requires ps2pdf (Ghostscript).
set -e
cd "$(dirname "$0")"

header() {
    echo '%!PS'
    echo '<< /PageSize [612 792] >> setpagedevice'
    echo '/Helvetica findfont 12 scalefont setfont'
}

# line PAGE K: the text of line K of page PAGE
line() {
    echo "Page $1, line $2: the quick brown fox jumps over the lazy dog."
}

# show Y TEXT
show() {
    echo "72 $1 moveto ($2) show"
}

# page PAGE [VARIANT]
page() {
    k=1
    while [ $k -le 20 ]; do
        text=$(line "$1" $k)
        if [ "$2" = word ] && [ $k -eq 10 ]; then
            text=$(echo "$text" | sed 's/fox/cat/')
        fi
        show $((700 - 15 * (k - 1))) "$text"
        if [ "$2" = bold ] && [ $k -eq 3 ]; then
            show $((700 - 15 * (k - 1))) "$text"
        fi
        k=$((k + 1))
    done
    case "$2" in
        box) echo '500 664 20 20 rectfill' ;;
        marginbox) echo '500 756 20 20 rectfill' ;;
    esac
    echo showpage
}

# Page 1's lines, last first, in the same places
reordered_page() {
    k=20
    while [ $k -ge 1 ]; do
        show $((700 - 15 * (k - 1))) "$(line 1 $k)"
        k=$((k - 1))
    done
    echo showpage
}

# Page 1's words, six to a line
reflowed_page() {
    k=1
    words=""
    while [ $k -le 20 ]; do
        words="$words $(line 1 $k)"
        k=$((k + 1))
    done
    echo $words | xargs -n 6 echo | {
        y=700
        while read -r text; do
            show $y "$text"
            y=$((y - 15))
        done
    }
    echo showpage
}

inserted_page() {
    k=1
    while [ $k -le 20 ]; do
        show $((700 - 15 * (k - 1))) "An inserted page, line $k."
        k=$((k + 1))
    done
    echo showpage
}

# boxes_page PDFMARK: a page with the boundaries PDFMARK gives it, set with
# a /PAGE pdfmark, and words at fixed places in default user space
boxes_page() {
    echo "[ $1 /PAGE pdfmark"
    show 400 MARK
    echo '45 300 moveto (ARTGAP) show'      # inside the trim box, not the art box
    echo '5 100 moveto (OUTCROP) show'      # inside the media box, not the crop box
    echo '-15 500 moveto (NOMEDIA) show'    # starts outside the media box
    echo showpage
}

# Letter pages, the media box [0 0 612 792] unless given:
#   1-4  crop [18 18 594 774], bleed [27 27 585 765], trim [36 36 576 756],
#        art [72 72 540 720], rotated by 0, 90, 180 and 270
#   5    media [-100 -50 512 742] and no other boxes
#   6    crop [36 36 576 756], bleed [18 18 594 774] (outside the crop
#        box) and trim [-20 36 576 756] (outside the media box), no art box
boxes_pages() {
    all='/CropBox [18 18 594 774] /BleedBox [27 27 585 765] /TrimBox [36 36 576 756] /ArtBox [72 72 540 720]'
    for rotate in 0 90 180 270; do
        boxes_page "$all /Rotate $rotate"
    done
    boxes_page '/MediaBox [-100 -50 512 742]'
    boxes_page '/CropBox [36 36 576 756] /BleedBox [18 18 594 774] /TrimBox [-20 36 576 756]'
}

# trimmed_page X VARIANT: page 1 with a TrimBox, and a mark at X, 10
# outside it
trimmed_page() {
    echo '[ /TrimBox [36 36 576 756] /PAGE pdfmark'
    page 1 "$2" | sed '$d'
    echo "$1 10 10 10 rectfill"
    echo showpage
}

# Page 1 on a page 36pt larger, moved into the middle of it, trimmed to
# where it was, with a mark in the bleed
bled_page() {
    echo '<< /PageSize [648 828] >> setpagedevice'
    echo '[ /TrimBox [18 18 630 810] /PAGE pdfmark'
    echo '18 18 translate'
    page 1 | sed '$d'
    echo '-15 -15 10 10 rectfill'
    echo showpage
}

generate() {
    name=$1
    shift
    { header; "$@"; } > "$name.ps"
    # Embed all fonts, so rendering does not depend on installed fonts
    ps2pdf -dPDFSETTINGS=/prepress "$name.ps" "$name.pdf"
    rm "$name.ps"
}

base_pages() { page 1; page 2; page 3; }
word_pages() { page 1; page 2 word; page 3; }
reflow_pages() { reflowed_page; page 2; page 3; }
reordered_pages() { reordered_page; page 2; page 3; }
bold_pages() { page 1 bold; page 2; page 3; }
box_pages() { page 1 box; page 2; page 3; }
marginbox_pages() { page 1 marginbox; page 2; page 3; }
inserted_pages() { page 1; inserted_page; page 2; page 3; }

generate base base_pages
generate same base_pages
generate word word_pages
generate reflow reflow_pages
generate reordered reordered_pages
generate bold bold_pages
generate box box_pages
generate marginbox marginbox_pages
generate inserted inserted_pages
generate boxes boxes_pages
generate plain page 1
generate trim trimmed_page 10
generate trim-mark trimmed_page 590
generate trim-word trimmed_page 10 word
generate bleed bled_page
