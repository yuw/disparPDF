#!/bin/sh
# Tests the zsh completion: completion-zsh.sh COMPLETION_DIR
# Runs an interactive zsh in a pseudo-terminal, presses Tab after partial
# command lines, and records what completion offers.  Skipped (exit
# status 77) without zsh.
export LC_ALL=C
command -v zsh >/dev/null || exit 77
COMPDIR=$(cd "$1" && pwd) || exit 1
dir=$(mktemp -d) || exit 1
trap 'rm -rf "$dir"' EXIT
mkdir "$dir/files" "$dir/files/sub"
touch "$dir/files/a.pdf" "$dir/files/B.PDF" "$dir/files/notes.txt" \
      "$dir/files/r.xml"
export COMPDIR WORKDIR="$dir/files" OUTFILE="$dir/out" RCFILE="$dir/rc.zsh"

# Loaded by the interactive zsh: Tab completes, writing what is offered
# to $OUTFILE
cat > "$RCFILE" <<'EOF'
fpath=($COMPDIR $fpath)
autoload -Uz compinit && compinit -u -D
cd $WORKDIR
_capture_complete() {
  compadd() {
    local -a __hits
    builtin compadd -O __hits "$@"
    (( $#__hits )) && print -rl -- $__hits >> $OUTFILE
    builtin compadd "$@"
  }
  _main_complete
  unfunction compadd
}
zle -C capture-complete complete-word _capture_complete
bindkey '^I' capture-complete
PS1='READY> '
EOF

# complete LINE: what zsh offers for LINE, sorted, on one line
complete_line() {
    zsh -f -c '
        zmodload zsh/zpty
        : > $OUTFILE
        zpty z zsh -f -i
        zpty -w z "source $RCFILE"
        zpty -r z junk "*READY> "
        zpty -n -w z "$1"$'\''\t'\''
        for i in {1..50}; do [[ -s $OUTFILE ]] && break; sleep 0.1; done
        sleep 0.3
        zpty -d z
        sort -u $OUTFILE | tr "\n" " "
    ' sh "$1"
}

status=0
check() {
    actual=$(complete_line "$1")
    if [ "$actual" != "$2" ]; then
        echo "FAIL: [$1] gave [$actual], expected [$2]"
        status=1
    fi
}
check 'disparPDF ' 'B.PDF a.pdf sub '
check 'disparPDF --lang' '--language '
check 'disparPDF --language=' 'cz de en fr '
check 'disparPDF --debug=' '2 3 '
check 'disparPDF --outType=' '0 1 '
check 'disparPDF --start' '--startPage1 --startPage2 '
check 'disparPDF --b' '--batch '
check 'disparPDFc --b' ''
check 'disparPDF --pdfdiff=' 'B.PDF a.pdf sub '
check 'disparPDF --xmlResult=' 'r.xml sub '
check 'disparPDF --any-extension ' 'B.PDF a.pdf notes.txt r.xml sub '
check 'disparPDF -a --comp' '--compareFonts '
exit $status
