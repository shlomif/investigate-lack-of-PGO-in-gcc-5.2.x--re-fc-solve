package FC_Solve::VarBaseDigitsWriter::XS;

use strict;
use warnings;

use Config;

use Inline (
    C => <<'EOF',
#include "var_base_writer.h"

SV* _proto_new() {
        fcs_var_base_writer_t * s;
        SV*      obj_ref = newSViv(0);
        SV*      obj = newSVrv(obj_ref, "FC_Solve::VarBaseDigitsWriter::XS");

        New(42, s, 1, fcs_var_base_writer_t);
        fc_solve_var_base_writer_init(s);
        fc_solve_var_base_writer_start(s);

        sv_setiv(obj, (IV)s);
        SvREADONLY_on(obj);
        return obj_ref;
}

#define DEREF() ((fcs_var_base_writer_t*)SvIV(SvRV(obj)))

void _proto_write(SV* obj, int base, int item) {
    fcs_var_base_writer_t * w;
    w = DEREF();

    fc_solve_var_base_writer_write(w, base, item);
}

SV * _proto_get_data(SV* obj) {
    SV * ret;
    unsigned char exported[800];
    size_t count;

    fcs_var_base_writer_t * w = DEREF();

    count = fc_solve_var_base_writer_get_data(w, exported);

    ret = newSVpvn(exported, count);

    return ret;
}

void DESTROY(SV* obj) {
  fcs_var_base_writer_t * s = DEREF();
  fc_solve_var_base_writer_release(s);
  Safefree(s);
}

EOF
    NAME => "FC_Solve::VarBaseDigitsWriter::XS",
    CLEAN_AFTER_BUILD => 0,
    INC => "-I$ENV{FCS_PATH} -I$ENV{FCS_SRC_PATH}",
    LIBS => "-L" . $ENV{FCS_PATH} . ' -lgmp',
    OPTIMIZE => '-g',
    CCFLAGS => "$Config{ccflags} -std=gnu99"
);

sub new
{
    my $class = shift;
    return FC_Solve::VarBaseDigitsWriter::XS::_proto_new();
}

sub write
{
    my ($self, $args) = @_;

    my $base = $args->{base};
    my $item = $args->{item};

    $self->_proto_write($base, $item);

    return;
}

sub get_data
{
    my $self = shift;

    return $self->_proto_get_data();
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

