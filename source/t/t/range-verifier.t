#!/usr/bin/perl

use strict;
use warnings;

use Test::More tests => 5;
use Test::Differences;

use File::Spec;
use File::Basename qw( dirname );

use Storable qw(retrieve);

my $data_dir = File::Spec->catdir( dirname( __FILE__), 'data');
my $verifier_data_dir = File::Spec->catfile($data_dir, 'range-verifier');

my $summary_file = "$verifier_data_dir/summary.txt";
my $stats_file = "$verifier_data_dir/summary.stats.perl-storable";

sub delete_summary
{
    unlink ($summary_file);
    unlink ($stats_file);

    return;
}

delete_summary();

my $ranger_verifier = $ENV{'FCS_SRC_PATH'} . '/scripts/verify-range-in-dir-and-collect-stats.pl';

sub _test_range_verifier
{
    my ($args, $blurb) = @_;

    local $Test::Builder::Level = $Test::Builder::Level + 1;

    return ok (!system(
            $^X, $ranger_verifier,
            '--summary-lock', "$verifier_data_dir/summary.lock",
            '--summary-stats-file', $stats_file,
            '--summary-file', $summary_file,
            '-g', 'bakers_game',
            '--min-idx', $args->{min}, '--max-idx', $args->{max},
            $args->{sols_dir},
        ),
        $blurb,
    );
}

# TEST
_test_range_verifier(
    { min => 1, max => 10,
            sols_dir => "$verifier_data_dir/bakers-game-solutions-dir",
        },
    "1-10 Script was run successfully.",
);

sub _slurp
{
    my $filename = shift;

    open my $in, "<", $filename
        or die "Cannot open '$filename' for slurping - $!";

    local $/;
    my $contents = <$in>;

    close($in);

    return $contents;
}

sub _mysplit
{
    my $string = shift;

    return [split(/\n/, $string, -1)];
}

# TEST
eq_or_diff(
    _mysplit(_slurp($summary_file)),
    _mysplit(<<'EOF'),
Solved Range: Start=1 ; End=10
EOF
    "Summary file for Baker's Game Range 1-10 is Proper.",
);


sub _statistics_are
{
    my ($data, $blurb) = @_;

    local $Test::Builder::Level = $Test::Builder::Level + 1;

    return eq_or_diff(
        retrieve($stats_file),
        $data,
        $blurb,
    );
}

# TEST
_statistics_are(
    {
        counts =>
        {
            solved =>
            {
                iters =>
                {map { $_ => 1 } (73,145,146,164,453,726,815,1076,1213)},
                gen_states => {map { $_ => 1 } (106, 184, 187, 205, 500, 790, 870, 1100, 1246)},
                sol_lens => {(map { $_ => 1 } (97,98,100,109,112,122,124)), 119 => 2},
            },
            unsolved =>
            {
                iters => {3436 => 1,},
                gen_states => {3436 => 1,}
            },
        },
    },
    "1-10 Statistics are OK.",
);

# TEST
_test_range_verifier(
    { min => 11, max => 20,
            sols_dir => "$verifier_data_dir/bakers-game-11-to-20",
        },
    "11-20 Script was run successfully.",
);

# TEST
_statistics_are(
     {
        counts =>
        {
            solved =>
            {
                iters =>
                {(map { $_ => 1 } (72, 73, 75, 90, 101, 145,150, 164,300, 312, 453, 468, 726,815,1076,1213)), 146 => 2, },
                gen_states => {map { $_ => 1 } (96, 100, 106, 122, 147, 163, 177, 184, 187, 205, 321, 349, 500, 505, 790, 870, 1100, 1246)},
                sol_lens => {(map { $_ => 1 } (95, 96,98,99, 104, 105, 109,112,113, 117, 122,124)), 119 => 2, 97 => 2, 100 => 2, },
            },
            unsolved =>
            {
                iters => { map { $_ => 1 } (1891,3436) },
                gen_states => { map { $_ => 1 } (1891,3436) },
            },
        },
    },
    "1-20 Statistics are OK.",
);

# Clean up after everything.
delete_summary();

=head1 COPYRIGHT AND LICENSE

Copyright (c) 2011 Shlomi Fish

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

