#!/usr/bin/perl

use strict;
use warnings;

use IO::All;
use File::Path;

use Getopt::Long;

my $to_upload = 0;
GetOptions(
    '--upload!' => \$to_upload,
);

my $dest_dir = 'fcc_solver_for_amadiro';
mkpath("$dest_dir");
mkpath("$dest_dir/pthread");
mkpath("$dest_dir/libavl");

system(qq{./Tatzer -l x64b --nfc=2 --states-type=COMPACT_STATES --dbm=kaztree});
my @modules = ('app_str.o', 'card.o', 'fcc_solver.o', 'state.o', 'kaz_tree.o', 'rwlock.o', 'queue.o', 'meta_alloc.o', 'libavl/avl.o', );

foreach my $fn ('alloc.c', 'app_str.c', 'card.c', 'dbm_solver.c', 'state.c',
    'dbm_kaztree.c', 'alloc.h', 'config.h', 'state.h',
    'dbm_solver.h', 'kaz_tree.c', 'kaz_tree.h', 'dbm_solver_key.h',
    'fcs_move.h', 'inline.h', 'bool.h', 'internal_move_struct.h', 'app_str.h',
    'delta_states.c', 'fcs_dllexport.h', 'bit_rw.h', 'fcs_enums.h', 'unused.h',
    'portable_time.h', 'dbm_calc_derived.h', 'dbm_calc_derived_iface.h',
    'dbm_common.h', 'fcc_solver.c', 'indirect_buffer.h', 'fcc_brfs_test.h',
    'fcc_brfs.h', 'dbm_lru_cache.h', 'dbm_cache.h', 'meta_alloc.h',
    'meta_alloc.c', 'libavl/avl.c',, 'libavl/avl.h', 'generic_tree.h',
    'delta_states.h',
)
{
    io($fn) > io("$dest_dir/$fn");
}

foreach my $fn ('rwlock.c', 'queue.c', 'pthread/rwlock_fcfs.h', 'pthread/rwlock_fcfs_queue.h')
{
    io("/home/shlomif/progs/C/pthreads/rwlock/fcfs-rwlock/pthreads/$fn") > io("$dest_dir/$fn")
}

my $cache_size = 64_000_000;
my @deals = (
    982,
);

# my $deal_idx = 982;
foreach my $deal_idx (@deals)
{
    system(qq{python board_gen/make_pysol_freecell_board.py -t --ms $deal_idx > $dest_dir/$deal_idx.board});
}

@modules = sort { $a cmp $b } @modules;

io("$dest_dir/Makefile")->print(<<"EOF");
TARGET = fcc_fc_solver
DEALS = @deals

DEALS_DUMPS = \$(patsubst %,%.dump,\$(DEALS))
THREADS = 12
CACHE_SIZE = $cache_size

CFLAGS = -O3 -march=native -fomit-frame-pointer -DFCS_DBM_WITHOUT_CACHES=1 -DFCS_DBM_USE_LIBAVL=1 -I. -I./libavl/
MODULES = @modules

all: \$(TARGET)

\$(TARGET): \$(MODULES)
\tgcc \$(CFLAGS) -fwhole-program -o \$\@ \$(MODULES) -lm -lpthread

\$(MODULES): %.o: %.c
\tgcc -c \$(CFLAGS) -o \$\@ \$<

run: \$(DEALS_DUMPS)

\$(DEALS_DUMPS): %.dump: all
\t./\$(TARGET) --caches-delta \$(CACHE_SIZE) -o \$\@ \$(patsubst %.dump,%.board,\$\@)

%.show:
\t\@echo "\$* = \$(\$*)"
EOF

if ($to_upload)
{
    my $arc_name = "$dest_dir-trunk-r4442.tar.bz2";
    if (system('tar', '-cjvf', $arc_name, $dest_dir))
    {
        die "tar failed!";
    }
    system("rsync", "-a", "-v", "--progress", "--inplace", $arc_name,
        "hostgator:public_html/Files/files/code/"
    );
}
