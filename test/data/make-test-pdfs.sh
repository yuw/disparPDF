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
