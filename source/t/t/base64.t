#!/usr/bin/perl

use strict;
use warnings;

use Test::More tests => 14;

use FC_Solve::Base64;

# TEST:$c=0;
sub test_base64
{
    local $Test::Builder::Level = $Test::Builder::Level + 1;
    my ($init_buffer, $blurb_base) = @_;

    my $encoded = FC_Solve::Base64::base64_encode($init_buffer);

    # TEST:$c++;
    like(
        $encoded,
        qr/\A[A-Za-z0-9\+_]*=*\z/,
        "$blurb_base - encoded string matches Base64 regexp."
    );

    my $roundtrip = FC_Solve::Base64::base64_decode($encoded);
    # TEST:$c++;
    is ($roundtrip, $init_buffer,
        "Got the same data after a encode+decode roundtrip."
    );

    return;
}

# TEST:$test_base64=$c;

{
    # TEST*$test_base64
    test_base64('FooBar24', 'String FooBar24');

    # TEST*$test_base64
    test_base64("\0", 'Single NUL');

    # TEST*$test_base64
    test_base64("\0\0", 'Double NUL');

    # TEST*$test_base64
    test_base64("\0\0\0", 'Triple NUL');

    # TEST*$test_base64
    test_base64("\0\0\0\0", 'Quadruple NUL');

    # TEST*$test_base64
    test_base64("\0\0bc\0\0", 'Between NULs');

    # TEST*$test_base64
    test_base64("\0b\0c\0d/+:", 'Between Three NULs');
}

