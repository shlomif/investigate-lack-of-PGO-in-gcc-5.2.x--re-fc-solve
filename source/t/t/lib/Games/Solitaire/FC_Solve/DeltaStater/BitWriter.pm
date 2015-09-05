package Games::Solitaire::FC_Solve::DeltaStater::BitWriter;

use strict;
use warnings;

use parent 'Games::Solitaire::Verify::Base';

__PACKAGE__->mk_acc_ref([qw(_bits_ref _bit_idx)]);

sub _init
{
    my $self = shift;

    $self->_bits_ref(\(my $s = ''));

    $self->_bit_idx(0);

    return;
}

sub _next_idx
{
    my $self = shift;

    my $ret = $self->_bit_idx;

    $self->_bit_idx($ret+1);

    return $ret;
}

sub write
{
    my ($self, $len, $data) = @_;

    while ($len)
    {
        vec(${$self->_bits_ref()}, $self->_next_idx(), 1) = ($data & 0b1);
    }
    continue
    {
        $len--;
        $data >>= 1;
    }

    return;
}

sub get_bits
{
    my $self = shift;

    return ${$self->_bits_ref()};
}

1;

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

