#!/usr/bin/perl

use strict;
use warnings;

use Carp (qw/ confess /);

sub run
{
	my $cmd = shift;
	if (system(@$cmd))
	{
		confess ("[@{$cmd}] failed! $! $?");
	}
}

my $build_dir = "B";
mkdir($build_dir);
chdir($build_dir);

run(
	[
	qq{C:/Program Files/CMake 2.8/bin/CMake},
	"-G", "MinGW Makefiles",
    # These variables require libgmp which isn't provided by default.
    "-DFCS_WITH_TEST_SUITE=", "-DFCS_ENABLE_DBM_SOLVER=",
	@ARGV,
	".."
	]
);

# my $make_path = "C:/Dwimperl/c/bin/mingw32-make";
my $make_path = "C:/strawberry/c/bin/mingw32-make";

run( [ $make_path, ] );

run( [ $make_path, "package", ] );



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

