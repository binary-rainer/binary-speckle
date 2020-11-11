#!/usr/bin/perl -w
# binResults.pl
# Created:     Thu Sep 18 11:43:14 1997 by Koehler@sun18
# Last change: Fri Jul 25 15:23:54 2014
#
# This file is Hellware!
# It may send you and your data straight to hell without further notice.
#

# "[VBK]*.fit" is better than "Vis.fit KT.fit Bi.fit",
# because grep does not complain if one of the files is not there

open GREP, "grep TITLE [VBK]*.fit |" or die "Can't grep: $!\n";
$title = <GREP>;
close GREP;

chomp $title;
$title =~ s/^.*:TITLE.*:\s*//i;

print "$title\n";
print "=" x length($title), "\n\n";

#############################################################################

sub grep_Begleiter {
  my($pat) = @_;

  open GREP, "grep -H BEGLEITER $pat |" or die "Can't grep: $!\n";

  $Sum_pa= $Sum_d= $Sum_hv= $Sum2_pa= $Sum2_d= $Sum2_hv= 0.;
  $n = 0;
  while(<GREP>) {
    s/BEGLEITER:\s*/\t/;
    my($fname,$pa,$d,$hv) = split(" ");
    $Sum_pa += $pa;	$Sum2_pa += $pa * $pa;
    $Sum_d  += $d;	$Sum2_d  += $d  * $d;
    $Sum_hv += $hv;	$Sum2_hv += $hv * $hv;
    $n++;
    printf "%-12s	%9s	%8s	%7s\n", $fname, $pa, $d, $hv;
  }
  close GREP;
  return if $n == 0;

  print "\t\t", "-" x 40, "\n";
  $Sum_pa /= $n;	$Sum2_pa /= $n;
  $Sum_d  /= $n;	$Sum2_d  /= $n;
  $Sum_hv /= $n;	$Sum2_hv /= $n;
  printf "\t	%9.5f	%8.5f	%7.5f\n", $Sum_pa, $Sum_d, $Sum_hv;
  $n1 = $n-1.;
  if ($n1 > 0) {
      printf "\t    +/- %9.5f   +/- %8.5f    +/- %7.5f\n\n",
      		sqrt(($Sum2_pa - $Sum_pa * $Sum_pa) / $n1),
		sqrt(($Sum2_d  - $Sum_d  * $Sum_d ) / $n1),
		sqrt(($Sum2_hv - $Sum_hv * $Sum_hv) / $n1);
  }
}

#############################################################################

print "		P.A.		d		K2/1\n";

grep_Begleiter "Fit*.fit";


$list = "";
foreach (glob "*.fit") { $list .= " $_" unless /^Fit/; }

#print STDERR "$list\n";

grep_Begleiter $list;
