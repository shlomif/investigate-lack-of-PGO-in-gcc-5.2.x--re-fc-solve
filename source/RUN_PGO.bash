preproc()
{
    local in_fn="$1"
    shift
    local out_fn="$1"
    shift
    if ! test -e "$out_fn"; then
        perl -lape 'use Cwd qw(getcwd); my $x = getcwd(); my $dat = "$x/share/freecell-solver"; my $p = "$dat/presets/"; s#\@CMAKE_INSTALL_PREFIX\@#$x#g; s#\@PKGDATADIR\@#$dat#g; s#\@PRESETS_DATA_DIR\@#$p/#g' < "$in_fn" > "$out_fn"
    fi
}

preproc "prefix.h.in" "prefix.h"
preproc "share/freecell-solver/presetrc.proto.in" "share/freecell-solver/presetrc"
bash scripts/pgo.bash gcc total
