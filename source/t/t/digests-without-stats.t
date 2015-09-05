#!/usr/bin/perl

use strict;
use warnings;

=head1 digests-without-stat.t

This test script aims to verify that scripts yield a good result of SHA-256
digests and lengths while stripping away the lines of the statistics. This is
useful for pre-testing desirable solutions while avoiding the issue of the
statistics like the number of states checked and the number of stored states.

=cut

use Test::More tests => 6;

use File::Basename qw( dirname );
use File::Spec;

use lib './t/lib';

use Games::Solitaire::FC_Solve::CheckResults;

my $data_dir = File::Spec->catdir(
    dirname( __FILE__), 'data'
);

my $digests_storage_fn = "$data_dir/digests-and-lens-wo-stats-storage.yml";

my $verifier = Games::Solitaire::FC_Solve::CheckResults->new(
    {
        data_filename => $digests_storage_fn,
        trim_stats => 1,
    }
);

sub verify_solution_test
{
    local $Test::Builder::Level = $Test::Builder::Level + 1;

    return $verifier->verify_solution_test(@_);
}

# 24 is my lucky number. (Shlomif)
# TEST
verify_solution_test({id => "freecell_default24", deal => 24, theme => [],},
    "Verifying the solution of deal #24");

# TEST
verify_solution_test(
    {
        id => "freecell_simple_flare_2",
        deal => 2,
        theme => [qw(
            --flare-name dfs
            --next-flare --method a-star --flare-name befs
            --flares-plan), q{Run:500@dfs,Run:1500@befs},
        ],
    }
);

# This should generate the same results as --method dfs.
# TEST
verify_solution_test(
    {
        id => "freecell_flares_cp_1_deal_6",
        deal => 6,
        theme => [qw(
            --flare-name dfs
            --next-flare --method a-star --flare-name befs
            --flares-plan), q{Run:300@dfs,Run:3000@befs,CP:,Run:200@dfs},
        ],
    }
);

# This should generate the same results as --method dfs.
# It checks that the plan is restarted over after it reaches the end
# and yields the end.
# TEST
verify_solution_test(
    {
        id => "freecell_flares_cp_1_circular_deal_6",
        deal => 6,
        theme => [qw(
            --flare-name dfs
            --next-flare --method a-star --flare-name befs
            --flares-plan), q{Run:300@dfs,Run:1000@befs,CP:,Run:100@dfs},
        ],
    }
);

# This should test the run-indefinitely
# TEST
verify_solution_test(
    {
        id => "freecell_flares_run_indef_1_deal_6",
        deal => 6,
        theme => [qw(
            --flare-name dfs
            --next-flare --method a-star --flare-name befs
            --flares-plan), q{Run:500@dfs,RunIndef:befs},
        ],
    }
);

# This checks for an infinite loop when several identically-spaced quotas
# are given to the flares.
# TEST
verify_solution_test(
    {
        id => "freecell_flares_equally_spaced_quotas_deal_1",
        deal => 1,
        theme => [
            # This is to avoid warnings on commas in qw(...)
        grep { /\S/ } split(/\s+/, <<'EOF')

--method a-star -asw 0.2,0.8,0,0,0 -step 500 --st-name 11 --flare-name 11 -nf
--method a-star -to 0123467 -asw 0.5,0,0.3,0,0 -step 500 --st-name 18 --flare-name 18 -nf
--flares-plan Run:200@18,Run:200@11,Run:200@18

EOF

        ],
    }
);

# Store the changes at the end so they won't get lost.
$verifier->end();


=head1 COPYRIGHT AND LICENSE

Copyright (c) 2008 Shlomi Fish

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

