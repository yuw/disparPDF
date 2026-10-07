# bash completion for disparPDF and disparPDFc            -*- shell-script -*-
# Installed as .../bash-completion/completions/disparPDF, with a symbolic
# link to it named disparPDFc

_disparPDF()
{
    local cur prev words cword
    _init_completion -n = || return

    # Files must be named *.pdf unless --any-extension is given, and
    # after "--" everything is a file
    local i pattern='@(pdf|PDF)' options=true
    for ((i = 1; i < cword; i++)); do
        case ${words[i]} in
            --any-extension) pattern= ;;
            --) options=false ;;
        esac
    done

    if $options; then
        case $cur in
            --language=*)
                COMPREPLY=($(compgen -W 'cz de en fr' -- "${cur#*=}"))
                return
                ;;
            --debug=*)
                COMPREPLY=($(compgen -W '2 3' -- "${cur#*=}"))
                return
                ;;
            --outType=*)
                COMPREPLY=($(compgen -W '0 1' -- "${cur#*=}"))
                return
                ;;
            --pages=* | --startPage1=* | --startPage2=* | --key=*)
                # A number, or for --key any text
                return
                ;;
            --pdfdiff=*)
                cur=${cur#*=}
                _filedir '@(pdf|PDF)'
                return
                ;;
            --xmlResult=*)
                cur=${cur#*=}
                _filedir xml
                return
                ;;
            --settings=*)
                cur=${cur#*=}
                _filedir
                return
                ;;
        esac

        if [[ $cur == -* ]]; then
            local opts='-a --appearance -c --characters -w --words
                --any-extension --language= --debug=2 --debug=3
                --outType=0 --outType=1 --pages= --startPage1= --startPage2=
                --pdfdiff= --xmlResult= --key= --settings= --compareFonts
                -h --help --version --'
            # disparPDFc is always in batch mode
            [[ ${words[0]##*/} == disparPDFc ]] || opts+=' -b --batch'
            COMPREPLY=($(compgen -W "$opts" -- "$cur"))
            [[ ${COMPREPLY-} == *= ]] && compopt -o nospace
            return
        fi
    fi

    if [[ $pattern ]]; then
        _filedir "$pattern"
    else
        _filedir
    fi
} &&
    complete -F _disparPDF disparPDF disparPDFc

# ex: filetype=sh
