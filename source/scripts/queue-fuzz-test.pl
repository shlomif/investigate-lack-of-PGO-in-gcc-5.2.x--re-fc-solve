#!/usr/bin/perl

# Run this script like this:
#
# [ From the git /source/ sub-dir ]
#
# cd ..
# mkdir B
# cd B
# ../source/Tatzer
# make
# export FCS_PATH="$(pwd)" FCS_SRC_PATH="$(cd ../source && pwd)"
# cd t
#
# perl -I ../../source/t/t/lib ../../source/scripts/queue-fuzz-test.pl 10000 2 100

use strict;
use warnings;

use lib './t/t/lib';

package RandGen;

use parent 'Games::ABC_Path::MicrosoftRand';

sub rand30
{
    my $self = shift;

    my $one = $self->rand;
    my $two = $self->rand;

    return ($one | ($two << 15));
}

package main;


use File::Spec;
use File::Path qw( mkpath );

use Data::Dump qw(dd);

use Games::Solitaire::FC_Solve::QueuePrototype;
use Games::Solitaire::FC_Solve::QueueInC;

my ($items_per_page, $data_seed, $interval_seed) = @ARGV;

my $input_gen =  RandGen->new(seed => $data_seed);
my $output_gen = RandGen->new(seed => $data_seed);
my $interval_gen = RandGen->new(seed => $interval_seed);

my $queue_offload_dir_path = File::Spec->catdir(
    $ENV{TMPDIR}, "queue-offload-dir"
);
mkpath($queue_offload_dir_path);

my $class = $ENV{'USE_C'} ? 'Games::Solitaire::FC_Solve::QueueInC' :
    'Games::Solitaire::FC_Solve::QueuePrototype';

my $queue = $class->new(
    {
        num_items_per_page => $items_per_page,
        offload_dir_path => $queue_offload_dir_path,
    }
);

while ($queue->get_num_extracted() < 1_000_000)
{
    # Insert some items.
    {
        my $interval = $interval_gen->rand30() % 1_000;

        foreach my $idx (1 .. $interval)
        {
            $queue->insert($input_gen->rand30());
        }
    }

    {
        my $interval = $interval_gen->rand30() % 1_000;

        foreach my $idx (1 .. $interval)
        {
            if (! $queue->get_num_items_in_queue())
            {
                last;
            }
            if ($queue->extract() != $output_gen->rand30())
            {
                print "Problem occured.";
                dd($queue);
                die "Error";
            }
        }
    }
    print "Checkpoint." . $queue->get_num_extracted() . "\n";
}

exit(0);

=head1 COPYRIGHT & LICENSE

Copyright 2014 by Shlomi Fish

This program is distributed under the MIT (X11) License:
L<http://www.opensource.org/licenses/mit-license.php>

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
