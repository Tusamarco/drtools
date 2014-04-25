#!/usr/bin/env perl -w

use strict;

my $dir = $ARGV[0] || "dump";
print "Dir: $dir\n";
mkdir($dir);

my $files = {};
while(<STDIN>) {
	/(.*?)\t(.*)/;
	unless ($files->{$1}) {
		print "Opening file '$dir/$1.csv'\n";
		open($files->{$1}, ">$dir/$1.csv");
	}
	my $F = $files->{$1};
	print($F "$2\n");
}

