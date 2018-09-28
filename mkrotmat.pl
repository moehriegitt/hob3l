#! /usr/bin/perl

use strict;
use warnings;
use Data::Dumper;

sub all_0($$@)
{
    my ($x,$y,@a) = @_;
    for my $a (@a) {
        if ($a->[$x][$y]) {
            return 0;
        }
    }
    return 1;
}

sub ind($)
{
    my ($i) = @_;
    return '    ' x $i;
}

sub is0($$)
{
    my ($x,$y) = @_;
    return "cp_equ(m->m[$y][$x], 0)";
}

sub write_test($$@);
sub write_test($$@)
{
    my ($i, $cs, @a) = @_;

    my %c = ();
    for my $c (@$cs) {
        my ($x,$y) = @$c;
        $c{$x}{$y} = 1;
    }

    for my $y (0..2) {
        print ind($i).($y == 0 ? "/*" : " *");
        for my $a (@a) {
            print "  ";
            for my $x (0..2) {
                my $t = $a->[$y][$x];
                if ($c{$x}{$y}) {
                    if ($t) {
                        print " ?";
                    }
                    else {
                        print " 0";
                    }
                }
                elsif ($t) {
                    print " X";
                }
                else {
                    print " O";
                }
            }
        }
        print ($y == 2 ? "  */\n" : "\n");
    }

    my $cs2 = [];
    for my $c (@$cs) {
        my ($x,$y) = @$c;
        if (all_0($x, $y, @a)) {
            print ind($i)."if (!".is0($x,$y).") { return false; }\n";
        }
        else {
            push @$cs2, $c;
        }
    }
    $cs = $cs2;
    if (scalar(@a) == 1) {
        print ind($i)."return true;\n";
        return;
    }

    my $cb = undef;
    my $b1 = undef;
    my $b0 = undef;
    for my $c (@$cs) {
        my ($x,$y) = @$c;
        my $a1 = [ grep {  $_->[$y][$x] } @a ];
        my $a0 = [ grep { !$_->[$y][$x] } @a ];
        if ((scalar(@$a1) != 0) || (scalar(@$a0) != 0)) {
            if (!defined($cb) ||
                (abs(scalar(@$a1) - scalar(@$a0)) < abs(scalar(@$b1) - scalar(@$b0))))
            {
                $cb = $c;
                $b1 = $a1;
                $b0 = $a0;
            }
        }
    }

    $cs2 = [
        grep {
            ($_->[0] != $cb->[0]) || ($_->[1] != $cb->[1])
        }
        @$cs
    ];
    die unless (scalar(@$cs2) + 1) == scalar(@$cs);

    my ($x,$y) = @$cb;
    print ind($i)."if (!".is0($x,$y).") {\n";
    write_test($i+1, $cs2, @$b0);
    print ind($i)."}\n";
    write_test($i, $cs2, @$b1);
}

print
    "#include <cpmat/mat.h>\n".
    "extern bool cp_mat3_is_rect_rot(\n".
    "    cp_mat3_t const *m)\n".
    "{\n";
write_test(
    1,
    [
        map {
            my $x = $_;
            map { [ $x, $_ ] } 0..2;
        }
        0..2
    ],
    [
        [1,0,0],
        [0,1,0],
        [0,0,1],
    ],
    [
        [1,0,0],
        [0,0,1],
        [0,1,0],
    ],
    [
        [0,1,0],
        [1,0,0],
        [0,0,1],
    ],
    [
        [0,1,0],
        [0,0,1],
        [1,0,0],
    ],
    [
        [0,0,1],
        [1,0,0],
        [0,1,0],
    ],
    [
        [0,0,1],
        [0,1,0],
        [1,0,0],
    ]);
print "}\n";
