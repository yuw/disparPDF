#!/bin/sh
# Tests the bash completion: completion-bash.sh COMPLETION_FILE
# Skipped (exit status 77) without bash and bash-completion.
export LC_ALL=C
completion=$1
bash_completion=/usr/share/bash-completion/bash_completion
command -v bash >/dev/null && [ -r "$bash_completion" ] || exit 77

dir=$(mktemp -d) || exit 1
trap 'rm -rf "$dir"' EXIT
mkdir "$dir/sub"
touch "$dir/a.pdf" "$dir/B.PDF" "$dir/notes.txt" "$dir/r.xml"
cd "$dir" || exit 1

# complete LINE: what bash offers for LINE, sorted, on one line
complete_line() {
    bash -c '
        source "$1" 2>/dev/null; source "$2"
        compopt() { :; }  # only valid during real completion
        line=$3
        read -ra COMP_WORDS <<< "$line"
        [[ $line == *" " ]] && COMP_WORDS+=("")
        COMP_LINE=$line COMP_POINT=${#line} COMP_CWORD=$((${#COMP_WORDS[@]} - 1))
        _disparPDF "${COMP_WORDS[0]}" "${COMP_WORDS[COMP_CWORD]}" "${COMP_WORDS[COMP_CWORD-1]}"
        printf "%s\n" "${COMPREPLY[@]}" | sort | tr "\n" " "
    ' sh "$bash_completion" "$completion" "$1"
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
check 'disparPDF --lang' '--language= '
check 'disparPDF --language=' 'cz de en fr '
check 'disparPDF --debug=' '2 3 '
check 'disparPDF --outType=' '0 1 '
check 'disparPDF --start' '--startPage1= --startPage2= '
check 'disparPDF --b' '--batch '
check 'disparPDFc --b' ' '  # nothing
check 'disparPDF --int' '--interactive '
check 'disparPDFc --int' '--interactive '
check 'disparPDF --pdfdiff=' 'B.PDF a.pdf sub '
check 'disparPDF --xmlResult=' 'r.xml sub '
check 'disparPDF --settings=' 'B.PDF a.pdf notes.txt r.xml sub '
check 'disparPDF --vers' '--version '
check 'disparPDF -a a.pdf ' 'B.PDF a.pdf sub '
check 'disparPDF --any-extension ' 'B.PDF a.pdf notes.txt r.xml sub '
check 'disparPDF -- --' ' '  # nothing
exit $status
