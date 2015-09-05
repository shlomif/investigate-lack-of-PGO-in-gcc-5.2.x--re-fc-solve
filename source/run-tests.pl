#!/usr/bin/perl

use strict;
use warnings;

use autodie;

# use File::Which;
# use File::Basename;
use Cwd;
use File::Spec;
use File::Copy;
use File::Path;
use Getopt::Long;
use Env::Path;
use File::Basename qw( basename dirname );
# use Time::HiRes qw(time);


my $bindir = dirname( __FILE__ );
my $abs_bindir = File::Spec->rel2abs($bindir);

# Whether to use prove instead of runprove.
my $use_prove = $ENV{FCS_USE_TEST_RUN} ? 0 : 1;
my $num_jobs = $ENV{TEST_JOBS};

sub _is_parallized
{
    return ($use_prove && $num_jobs);
}

sub _calc_prove
{
    return ['prove', (defined($num_jobs) ? sprintf("-j%d", $num_jobs) : ())];
}

sub run_tests
{
    my $tests = shift;

    exec(($use_prove ? @{_calc_prove()} : 'runprove'), @$tests);
}

my $tests_glob = "*.{t.exe,py,t}";

GetOptions(
    '--glob=s' => \$tests_glob,
    '--prove!' => \$use_prove,
    '--jobs|j=n' => \$num_jobs,
) or die "--glob='tests_glob'";

{
    my $fcs_path = Cwd::getcwd();
    local $ENV{FCS_PATH} = $fcs_path;
    local $ENV{FCS_SRC_PATH} = $abs_bindir;

    my $testing_preset_rc;
    {
        my $preset_dest = File::Spec->catdir(
            File::Spec->curdir(), "t", "Presets"
        );
        my $preset_src = File::Spec->catfile(
            $abs_bindir, "Presets"
        );
        my $pset_files_dest = File::Spec->catdir($preset_dest, "presets");
        my $pset_files_src  = File::Spec->catdir($preset_src, "presets");
        mkpath($pset_files_dest);
        opendir (my $d, $pset_files_src);
        while (defined(my $f = readdir($d)))
        {
            if ($f =~ m{\.sh\z})
            {
                copy(
                    File::Spec->catfile($pset_files_src, $f),
                    File::Spec->catfile($pset_files_dest, $f)
                );
            }
        }
        closedir($d);

        my $files_dest_abs = File::Spec->rel2abs(File::Spec->catfile($pset_files_dest, "STUB.TXT"));

        $files_dest_abs =~ s{STUB\.TXT\z}{};

        $testing_preset_rc = File::Spec->rel2abs(File::Spec->catfile($preset_dest, "presetrc"));

        open my $in, "<", File::Spec->catfile($fcs_path, "Presets", "presetrc");
        open my $out, ">", $testing_preset_rc;
        while (my $line = <$in>)
        {
            $line =~ s{\A(dir=)(.*)}{$1$files_dest_abs\n}ms;
            print {$out} $line;
        }
        close($in);
        close($out);
    }


    local $ENV{FREECELL_SOLVER_PRESETRC} = $testing_preset_rc;
    local $ENV{FREECELL_SOLVER_QUIET} = 1;
    Env::Path->PATH->Prepend(
        File::Spec->catdir(Cwd::getcwd(), "board_gen"),
        File::Spec->catdir($abs_bindir, "t", "scripts"),
    );

    Env::Path->CPATH->Prepend(
        $abs_bindir,
    );

    Env::Path->LD_LIBRARY_PATH->Prepend(
        $fcs_path
    );

    my $foo_lib_dir = File::Spec->catdir($abs_bindir, "t", "t", "lib");
    foreach my $add_lib (Env::Path->PERL5LIB())
    {
        $add_lib->Append($foo_lib_dir);
    }
    local $ENV{FCS_PY2_LIBDIR} = $foo_lib_dir;
    local $ENV{FCS_PY3_LIBDIR} = File::Spec->catdir($abs_bindir, "t", "t", "lib-python3");

    my $get_config_fn = sub {
        my $basename = shift;

        return
        File::Spec->rel2abs(
            File::Spec->catdir(
                $bindir,
                "t", "config", $basename
            ),
        )
        ;
    };

    local $ENV{HARNESS_ALT_INTRP_FILE} =
        $get_config_fn->("alternate-interpreters.yml");

    local $ENV{HARNESS_TRIM_FNS} = 'keep:1';

    local $ENV{HARNESS_PLUGINS} = join(' ', qw(
        BreakOnFailure ColorSummary ColorFileVerdicts AlternateInterpreters
        TrimDisplayedFilenames
        )
    );

    my $is_ninja = (-e "build.ninja");
    if ($is_ninja)
    {
        system("ninja", "boards");
    }
    else
    {
        if (system("make", "-s", "boards"))
        {
            die "make failed";
        }
    }

    chdir("t");

    if (! $is_ninja)
    {
        if (system("make", "-s"))
        {
            die "make failed";
        }
    }

    # Cancelled because it is done by the build system.
    # if (! glob('t/valgrind--*.t'))
    # {
        # my $start = time();
        # system($^X, "$abs_bindir/scripts/generate-individual-valgrind-test-scripts.pl");
        # my $end = time();
        # print "StartGen=$start\nEndGen=$end\n";
    # }

    # Put the valgrind tests last, because they take a long time.
    my @tests =
        sort
        {
            (
                (($a =~ /valgrind/) <=> ($b =~ /valgrind/))
                    *
                (_is_parallized() ? -1 : 1)
            )
                ||
            (basename($a) cmp basename($b))
                ||
            ($a cmp $b)
        }
        (glob("t/$tests_glob"),glob("./$tests_glob"),
            (
                ($fcs_path ne $abs_bindir)
                ? (glob("$abs_bindir/t/t/$tests_glob"))
                : ()
            ),
        )
        ;

    if (! $ENV{FCS_TEST_BUILD})
    {
        @tests = grep { !/build-process/ } @tests;
    }

    if ($ENV{FCS_TEST_WITHOUT_VALGRIND})
    {
        @tests = grep { !/valgrind/ } @tests;
    }

    {
        print STDERR "FCS_PATH = $ENV{FCS_PATH}\n";
        print STDERR "FCS_SRC_PATH = $ENV{FCS_SRC_PATH}\n";
        run_tests(\@tests);
    }
}


=head1 COPYRIGHT AND LICENSE

Copyright (c) 2000 Shlomi Fish

Permission is hereby granted, free of charge, to any person
obtaining a copy of this software and associated documentation
files (the "Software"), to deal in the Software without
restriction, including without limitation the rights to use,
copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following
conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.



=cut

