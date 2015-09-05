#!/usr/bin/perl

# This program converts the solution output of dbm_fc_solver
# (and depth_dbm_fc_solver) to something that can be used as input for
# verify-solitaire-solution. It accepts the same command line arguments
# as verify-solitaire-solution so you should use it something like that:
#
# perl scripts/convert-dbm-fc-solver-solution-to-fc-solve-solution.pl \
#  --freecells-num 2 -
#
use strict;
use warnings;

package Games::Solitaire::Verify::App::From_DBM_FC_Solver;

use parent 'Games::Solitaire::Verify::Base';

use Games::Solitaire::Verify::VariantsMap;
use Games::Solitaire::Verify::Solution;
use Games::Solitaire::Verify::State;
use Games::Solitaire::Verify::Move;

use Getopt::Long qw(GetOptionsFromArray);

__PACKAGE__->mk_acc_ref(
    [
        qw(
            _filename
            _variant_params
        )
    ]
);

sub _init
{
    my ($self, $args) = @_;

    my $argv = $args->{'argv'};

    my $variant_map = Games::Solitaire::Verify::VariantsMap->new();

    my $variant_params = $variant_map->get_variant_by_id("freecell");

    GetOptionsFromArray(
        $argv,
        'g|game|variant=s' => sub {
            my (undef, $game) = @_;

            $variant_params = $variant_map->get_variant_by_id($game);

            if (!defined($variant_params))
            {
                die "Unknown variant '$game'!\n";
            }
        },
        'freecells-num=i' => sub {
            my (undef, $n) = @_;
            $variant_params->num_freecells($n);
        },
        'stacks-num=i' => sub {
            my (undef, $n) = @_;
            $variant_params->num_columns($n);
        },
        'decks-num=i' => sub {
            my (undef, $n) = @_;

            if (! ( ($n == 1) || ($n == 2) ) )
            {
                die "Decks should be 1 or 2.";
            }

            $variant_params->num_decks($n);
        },
        'sequences-are-built-by=s' => sub {
            my (undef, $val) = @_;

            my %seqs_build_by =
            (
                (map { $_ => $_ }
                    (qw(alt_color suit rank))
                ),
                "alternate_color" => "alt_color",
            );

            my $proc_val = $seqs_build_by{$val};

            if (! defined($proc_val))
            {
                die "Unknown sequences-are-built-by '$val'!";
            }

            $variant_params->seqs_build_by($proc_val);
        },
        'empty-stacks-filled-by=s' => sub {
            my (undef, $val) = @_;

            my %empty_stacks_filled_by_map =
            (map { $_ => 1 } (qw(kings any none)));

            if (! exists($empty_stacks_filled_by_map{$val}))
            {
                die "Unknown empty stacks filled by '$val'!";
            }

            $variant_params->empty_stacks_filled_by($val);
        },
        'sequence-move=s' => sub {
            my (undef, $val) = @_;

            my %seq_moves = (map { $_ => 1 } (qw(limited unlimited)));

            if (! exists ($seq_moves{$val}) )
            {
                die "Unknown sequence move '$val'!";
            }

            $variant_params->sequence_move($val);
        },
    )
        or die "Cannot process command line arguments";

    my $filename = shift(@$argv);

    if (!defined($filename))
    {
        $filename = "-";
    }

    $self->_variant_params($variant_params);
    $self->_filename($filename);

    return;
}

sub run
{
    my $self = shift;

    my $filename = $self->_filename();
    my $variant_params = $self->_variant_params();

    my $fh;

    if ($filename eq "-")
    {
        $fh = *STDIN;
    }
    else
    {
        open $fh, "<", $filename
            or die "Cannot open '$filename' - $!";
    }

    my $found = 0;

    LINES_PREFIX:
    while (my $line = <$fh>)
    {
        chomp($line);
        if ($line eq "Success!")
        {
            $found = 1;
            last LINES_PREFIX;
        }
    }

    if (!$found)
    {
        close($fh);
        die "State was not solved successfully.";
    }

    my $read_next_state = sub {
        my $line = <$fh>;
        chomp($line);
        if ($line eq "END")
        {
            return;
        }
        elsif ($line ne "--------")
        {
            die "Incorrect format.";
        }

        my $s = <$fh>;
        LINES:
        while ($line = <$fh>)
        {
            if ($line !~ /\S/)
            {
                last LINES;
            }
            $s .= $line;
        }
        $line = <$fh>;
        chomp($line);
        if ($line ne "==")
        {
            die "Cannot find '==' terminator";
        }

        return Games::Solitaire::Verify::State->new(
            {
                variant => "custom",
                variant_params => $self->_variant_params(),
                string => $s,
            },
        );
    };

    my $initial_state = $read_next_state->();

    my $running_state = $initial_state->clone();

    my @cols_iter = (0 .. ($running_state->num_columns() - 1));
    my @fc_iter = (0 .. ($running_state->num_freecells() - 1));
    my @cols_indexes = @cols_iter;
    my @fc_indexes = @fc_iter;

    print "-=-=-=-=-=-=-=-=-=-=-=-\n\n";

    my $out_running_state = sub {
        print $running_state->to_string();
        print "\n\n====================\n\n";
    };

    my $perform_and_output_move = sub {
        my ($move_s) = @_;

        print "$move_s\n\n";

        $running_state->verify_and_perform_move(
            Games::Solitaire::Verify::Move->new(
                {
                    fcs_string => $move_s,
                    game => $running_state->_variant(),
                },
            )
        );
        $out_running_state->();

        return;
    };

    my $calc_foundation_to_put_card_on = sub {
        my $card = shift;

        DECKS_LOOP:
        for my $deck (0 .. $running_state->num_decks() - 1)
        {
            if ($running_state->get_foundation_value($card->suit(), $deck) ==
                $card->rank() - 1)
            {
                my $other_deck_idx;

                for $other_deck_idx (0 ..
                    (($running_state->num_decks() << 2) - 1)
                )
                {
                    if ($running_state->get_foundation_value(
                            $card->get_suits_seq->[$other_deck_idx % 4],
                            ($other_deck_idx >> 2),
                        ) < $card->rank() - 2 -
                        (($card->color_for_suit(
                            $card->get_suits_seq->[$other_deck_idx % 4]
                        ) eq $card->color()) ? 1 : 0)
                    )
                    {
                        next DECKS_LOOP;
                    }
                }
                return [$card->suit(), $deck];
            }
        }
        return;
    };

    $out_running_state->();
    MOVES:
    while (my $move_line = <$fh>)
    {
        chomp($move_line);

        if ($move_line eq "END")
        {
            last MOVES;
        }

        # I thought I needed them, but I did not eventually.
        #
        # my @rev_cols_indexes;
        # @rev_cols_indexes[@cols_indexes] = (0 .. $#cols_indexes);
        # my @rev_fc_indexes;
        # @rev_fc_indexes[@fc_indexes] = (0 .. $#fc_indexes);

        my ($src, $dest);
        my $dest_move;

        my @tentative_fc_indexes = @fc_indexes;
        my @tentative_cols_indexes = @cols_indexes;
        if (($src, $dest) = $move_line =~ m{\AColumn (\d+) -> Freecell (\d+)\z})
        {
            $dest_move = "Move a card from stack $tentative_cols_indexes[$src] to freecell $tentative_fc_indexes[$dest]";
        }
        elsif (($src, $dest) = $move_line =~ m{\AColumn (\d+) -> Column (\d+)\z})
        {
            $dest_move = "Move 1 cards from stack $tentative_cols_indexes[$src] to stack $tentative_cols_indexes[$dest]";
        }
        elsif (($src, $dest) = $move_line =~ m{\AFreecell (\d+) -> Column (\d+)\z})
        {
            $dest_move = "Move a card from freecell $tentative_fc_indexes[$src] to stack $tentative_cols_indexes[$dest]";
        }
        elsif (($src) = $move_line =~ m{\AColumn (\d+) -> Foundation \d+\z})
        {
            $dest_move = "Move a card from stack $tentative_cols_indexes[$src] to the foundations";
        }
        elsif (($src) = $move_line =~ m{\AFreecell (\d+) -> Foundation \d+\z})
        {
            $dest_move = "Move a card from freecell $tentative_fc_indexes[$src] to the foundations";
        }
        else
        {
            die "Unrecognized Move line '$move_line'.";
        }

        $perform_and_output_move->($dest_move);

        # Now do the horne's prune.
        my $num_moved = 1; # Always iterate at least once.

        my $perform_prune_move = sub {
            my $prune_move = shift;

            $num_moved++;
            $perform_and_output_move->($prune_move);

            return;
        };

        my $check_for_prune_move = sub {
            my ($card, $prune_move) = @_;

            if (defined($card))
            {
                my $f = $calc_foundation_to_put_card_on->($card);

                if (defined($f))
                {
                    $perform_prune_move->($prune_move);
                }
            }

            return;
        };

        while ($num_moved)
        {
            $num_moved = 0;
            foreach my $idx (@cols_iter)
            {
                my $col = $running_state->get_column($idx);

                $check_for_prune_move->(
                    scalar($col->len() ? $col->top() : undef()),
                    "Move a card from stack $idx to the foundations",
                );
            }

            foreach my $idx (@fc_iter)
            {
                $check_for_prune_move->(
                    $running_state->get_freecell($idx),
                    "Move a card from freecell $idx to the foundations",
                );
            }
        }

        my $new_state = $read_next_state->();

        my $populate_new_resource_indexes = sub {
            my ($iter, $get_pivot_cb) = @_;

            my @new_resources_indexes;

            my %non_assigned_resources = (map { $_ => 1 } @$iter);
            my %old_resources_map;

            foreach my $idx (@$iter)
            {
                my $card = $get_pivot_cb->($running_state, $idx);

                push @{$old_resources_map{$card}}, $idx;
            }

            foreach my $idx (@$iter)
            {
                my $card = $get_pivot_cb->($new_state, $idx);
                my $aref = $old_resources_map{$card};

                if ((!defined($aref)) or (! @$aref))
                {
                    $aref = $old_resources_map{''};
                }
                my $i = shift(@$aref);

                $new_resources_indexes[$idx] = $i;
                if (defined($i))
                {
                    delete($non_assigned_resources{$i});
                }
            }

            my @non_assigned_resources_list =
                sort { $a <=> $b } keys(%non_assigned_resources);

            foreach my $resource_idx (@new_resources_indexes)
            {
                if (!defined($resource_idx))
                {
                    $resource_idx = shift(@non_assigned_resources_list);
                }
            }

            return \@new_resources_indexes;
        };

        my $new_cols_indexes = $populate_new_resource_indexes->(
            \@cols_iter,
            sub {
                my ($state, $idx) = @_;
                my $col = $state->get_column($idx);
                return ($col->len ? $col->pos(0)->to_string() : '');
            },
        );

        my $new_fc_indexes = $populate_new_resource_indexes->(
            \@fc_iter,
            sub {
                my ($state, $idx) = @_;
                my $card_obj = $state->get_freecell($idx);
                return (defined($card_obj) ? $card_obj->to_string() : '');
            },
        );

        my $verify_state =
            Games::Solitaire::Verify::State->new(
                {
                    variant => 'custom',
                    variant_params => $self->_variant_params(),
                }
            );

        foreach my $idx (@cols_iter)
        {
            $verify_state->add_column(
                $running_state->get_column($new_cols_indexes->[$idx])->clone()
            );
        }

        $verify_state->set_freecells(
            Games::Solitaire::Verify::Freecells->new(
                {
                    count => $running_state->num_freecells(),
                }
            )
        );

        foreach my $idx (@fc_iter)
        {
            my $card_obj =
                $running_state->get_freecell($new_fc_indexes->[$idx]);

            if (defined($card_obj))
            {
                $verify_state->set_freecell($idx, $card_obj->clone());
            }
        }

        $verify_state->set_foundations($running_state->_foundations->clone());

        {
            my $v_s = $verify_state->to_string();
            my $n_s = $new_state->to_string();
            if ($v_s ne $n_s)
            {
                die "States mismatch:\n<<\n$v_s\n>>\n vs:\n<<\n$n_s\n>>\n.";
            }
        }

        @cols_indexes = @$new_cols_indexes;
        @fc_indexes = @$new_fc_indexes;
    }

    print "This game is solveable.\n";

    close($fh);
}

package main;

Games::Solitaire::Verify::App::From_DBM_FC_Solver->new({ argv => [@ARGV] })->run();

=head1 COPYRIGHT & LICENSE

Copyright 2012 by Shlomi Fish

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
