package Games::Solitaire::FC_Solve::CheckResults;

use strict;
use warnings;

use parent 'Games::Solitaire::Verify::Base';

use YAML::XS qw(DumpFile LoadFile);

use String::ShellQuote;
use Carp;

use Test::More;
use Games::Solitaire::FC_Solve::ShaAndLen;

__PACKAGE__->mk_acc_ref([qw(
    digests_storage_fn
    digests_storage
    trim_stats
    )]);

sub new
{
    my $class = shift;
    my $self = {};

    bless $self, $class;

    $self->_init(@_);

    return $self;
}

sub _init
{
    my $self = shift;
    my $args = shift;

    $self->digests_storage_fn($args->{data_filename});
    $self->digests_storage(LoadFile($self->digests_storage_fn()));
    $self->trim_stats($args->{trim_stats});

    return 0;
}

sub end
{
    my $self = shift;

    # Make sure we do it only once.
    if (defined($self->digests_storage_fn()))
    {
        DumpFile($self->digests_storage_fn(), $self->digests_storage());
        $self->digests_storage_fn(undef());
    }
}

sub should_fill_in_id
{
    my $self = shift;

    my $id = shift;

    return
    (
        exists($self->digests_storage->{digests}->{$id})
        ?  (($ENV{'FCS_DIGESTS_REPLACE_IDS'} || "") =~ m{\Q,$id,\E} )
        :  (($ENV{'FCS_DIGESTS_FILL_IDS'} || "") =~ m{\Q,$id,\E} )
    );
}

sub verify_solution_test
{
    my $self = shift;

    local $Test::Builder::Level = $Test::Builder::Level + 1;

    my $args = shift;
    my $msg = shift;

    if (! $args->{id})
    {
        Carp::confess("ID not specified");
    }

    my $id = $args->{id};

    my $board = $args->{board};
    my $deal = $args->{deal};
    my $msdeals = $args->{msdeals};
    my $output_file = $args->{output_file};
    my $complete_command = $args->{complete_command};

    if ($deal !~ m{\A[1-9][0-9]*\z})
    {
        confess "Invalid deal $deal";
    }

    my $theme = $args->{theme} || ["-l", "gi"];

    my $variant = $args->{variant}  || "freecell";

    my $fc_solve_exe = shell_quote($ENV{'FCS_PATH'} . "/fc-solve");

    my $fc_solve_output;

    my $cl_prefix =
        ($msdeals
                ? "pi-make-microsoft-freecell-board $deal | "
                : "make_pysol_freecell_board.py $deal $variant | "
        ) . $fc_solve_exe . " ";

    my $cl_suffix = "-g $variant " . shell_quote(@$theme) . " -p -t -sam";

    if (defined($board))
    {
        $cl_prefix = $fc_solve_exe;
        $cl_suffix .= " " . shell_quote($board);
    }

    if ($complete_command)
    {
        open $fc_solve_output, "$complete_command |"
            or Carp::confess "Error! Could not open the complete command pipeline";
    }
    elsif (! $output_file)
    {
        open $fc_solve_output, "$cl_prefix $cl_suffix |"
            or Carp::confess "Error! Could not open the fc-solve pipeline";
    }
    else
    {
        if (system("$cl_prefix -o " . shell_quote($output_file) . " $cl_suffix"))
        {
            Carp::confess"Error could not execute the fc-solve pipeline.";
        }
        open $fc_solve_output, "<", $output_file
            or Carp::confess("Could not open file for reading - $!");
    }

    my $sha = Games::Solitaire::FC_Solve::ShaAndLen->new();

    if ($ENV{'FCS_DUMP_SOLS'})
    {
        open my $out, ">", "$id.SOLUTION.txt"
            or die "Cannot open '$id.txt' for writing";

        local $/;
        print {$out} <$fc_solve_output>;

        close($out);

        return ok(1, $msg);
    }
    if ($self->trim_stats)
    {
        $sha->add_processed_slurp(
            $fc_solve_output,
            sub {
                my $s = shift;
                $s =~ s/^(This game is solveable\.\n).*/$1/ms;
                return $s;
            }
        );
    }
    else
    {
        $sha->add_file($fc_solve_output);
    }

    close ($fc_solve_output);

    if ($output_file)
    {
        unlink($output_file);
    }

    if ($self->should_fill_in_id($id))
    {
        $self->digests_storage->{digests}->{$id} =
        {
            'len' => $sha->len(),
            'hexdigest' => $sha->hexdigest(),
        }
    }
    my $info = $self->digests_storage->{digests}->{$id};

    my $test_verdict = ok (
        ($sha->hexdigest() eq $info->{'hexdigest'}) &&
        ($sha->len() eq $info->{'len'})
        , $msg);

    if (!$test_verdict)
    {
        diag( "Expected Digest: " . $info->{'hexdigest'}. "\n"
            . "Got Digest: " . $sha->hexdigest() . "\n"
            . "Expected Len: " . $info->{'len'} . "\n"
            . "Got Len: " . $sha->len(). "\n"
        );
    }

    close($fc_solve_output);

    return $test_verdict;
}

1;

