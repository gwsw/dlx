#! /usr/bin/perl
use strict;

# tile L
# ****
# *
# end
# 
# tile R
# .**
# **
# .*
# end

main();
sub main {
    my $tiles = (shift(@ARGV) or 'tiles');
    my $in_tile = '';
    my $y = 0;
    print "std::list<Tile> $tiles;\n";
    while (<>) {
        chomp;
        next if /^\s*$/ or /^\s*#/;
        if ($in_tile) {
            if (/^\s*end\s*$/) {
                print "$tiles.push_back(tile); }\n";
                $in_tile = '';
            } else {
                s/\s//g;
                my $x = 0;
                foreach my $ch (split //) {
                    print "tile.add(Cell($x,$y));\n" if $ch eq '*';
                    ++$x;
                }
                ++$y;
            }
        } else {
            if (/^\s*tile\s+(\w+)/) {
                $in_tile = $1;
                $y = 0;
                print "{ Tile tile (\"$in_tile\");\n";
            } else {
                print "invalid line: $_";
            }
        }
    }
}
