#!/usr/bin/perl

use warnings;
use strict;

use Getopt::Std;
use File::Basename;

# The onbld_elfmod package is maintained in the same directory as this
# script, and is installed in ../lib/perl. Use the local one if present,
# and the installed one otherwise.
my $moddir = dirname($0);
$moddir = "$moddir/../lib/perl" if ! -f "$moddir/onbld_elfmod.pm";
require "$moddir/onbld_elfmod.pm";

use vars qw($EXRE_no_debuglink $EXRE_no_ctf);

sub has_ctf {
    my $file = shift;

    system("/bin/mcs -pn .SUNW_ctf $file | /bin/grep -q .");
    return ($? >> 8) == 0;
}

sub has_debuglink {
    my $file = shift;

    system("/bin/mcs -pn .gnu_debuglink $file | /bin/grep -q . ");
    return ($? >> 8) == 0;
}

my $code = 0;                       # Eventual exit code
sub error {
    my $msg = shift;
    print "error: $msg\n";
    $code = 1;
}

# onbld_elfmod reaches over here and reads 'opt', so it must be scoped to allow this.
# Ugh.
our %opt;
if (!getopts('f:e:', \%opt) || !$opt{f})  {
    print "usage: $0 -f filelist [-e exceptions]\n";
    exit 2;
}

# Locate and process the exceptions file
onbld_elfmod::LoadExceptionsToEXRE('debug');

my $fh = IO::File->new("< $opt{f}") or die "$0: Unable to open: $opt{f}";

my $prefix;
while (my $line = <$fh>) {
    $prefix = $1 if $line =~ /^PREFIX\s+(.*)$/;
    next unless $line =~ /^OBJECT/;

    my ($item, $class, $type, $verdef, $file) = split /\s+/, $line;
    my $obj = "$prefix/$file";

    if (($type eq "DYN") && $file !~ m!usr/lib/debug!) {
        unless (($file =~ $EXRE_no_ctf) || (has_ctf $obj)) {
            error "$file has no CTF";
        }

        unless ($file =~ $EXRE_no_debuglink) {
            if (has_debuglink $obj) {
                if (! -f "$prefix/usr/lib/debug/$file") {
                    error "$file debug object does not exist";
                }
            } else {
                error "$file has no .gnu_debuglink";
            }
        }

    } elsif (($type eq "REL") && $obj =~ m!/kernel/!) {
        unless ((has_ctf $obj) || $file =~ $EXRE_no_ctf) {
            error "$file has no CTF";
        }
    }
}

exit $code;

