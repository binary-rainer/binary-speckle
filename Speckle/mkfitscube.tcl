#!@@PATH_TO_WISH@@
# -*- Mode: tcl -*-
#
# mkfitscube.tcl
# Created:     Mon Jul  2 16:21:44 2001 by Koehler@hex.ucsd.edu
# Last Change: Wed Jul 18 14:16:42 2001
#
# This file is Hellware!
# It may send you and your data straight to hell without further notice.
#
#############################################################################\
exec wish $0 $@

set OPT(Speckledir) "@@SPECKLE_DIR@@"

if {[info exists env(TCL_RK_LIBRARY)] && [file isdir $env(TCL_RK_LIBRARY)]} {
    # all right
} elseif [file isdir "$OPT(Speckledir)/tcl-rk"] {
    set env(TCL_RK_LIBRARY) "$OPT(Speckledir)/tcl-rk"
} elseif [file isdir "./tcl-rk"] {
    set env(TCL_RK_LIBRARY) "[pwd]/tcl-rk"
} else {
    puts "No Tcl-RK library found!"
}
lappend auto_path $env(TCL_RK_LIBRARY)
rkInit

#############################################################################

proc display {fname} {
    if [catch "exec xpaget 'ds9*' version"] {
	# no ds9 present, replicate a new one
	exec ds9 $fname &
	after 1000; # give it a second to come up
    } else {
	exec xpaset -p "ds9*" file "[pwd]/$fname"
    }
}

#############################################################################

frame .lf -relief raised -bd 1
listbox .lf.lst -width 40 -height 42 -yscroll ".lf.scl set" -selectmode extended -font "courier 12"
scrollbar .lf.scl -command ".lf.lst yview"

# 123456789.123456789.123456789.
# ircal1234.fits.gz   PPM123456

bind Listbox <Button-3> {
    set f [%W get @%x,%y]
    regexp {^[^ ]+} $f f ;# select everything from start to 1.space
    display $f
    break
}

pack .lf.lst -side left -padx 0 -pady 2 -expand y -fill both
pack .lf.scl -side left -padx 2 -pady 2 -fill y

pack .lf -side left -padx 1 -pady 1 -expand y -fill both

#############################################################################

frame .bf -relief raised -bd 1
label .bf.rc -text "Right-click on\nfile to display it" -justify left
pack .bf.rc -side top -padx 3 -pady 5 -fill x

#############################################################################
label .bf.sel -text "Select files" -anchor w
frame .bf.ftf
label .bf.ftf.fl -text "From:"
rkIntEntry .bf.ftf.fe -relief flat -entrywd 5 -command "select_by_number select from w" -textvar select(from)
label .bf.ftf.tl -text "To:"
rkIntEntry .bf.ftf.te -relief flat -entrywd 5 -command "select_by_number select to w"   -textvar select(to)
grid .bf.ftf.fl .bf.ftf.fe -sticky e
grid .bf.ftf.tl .bf.ftf.te -sticky e

button .bf.nxt -text "Next set" -command {
    set no [expr $select(to) - $select(from)]
    set select(from) $select(to); incr select(from)
    set select(to) [expr $select(from) + $no]
}

pack .bf.sel -side top -padx 3 -pady 3 -fill x
pack .bf.ftf -side top -padx 3 -pady 0 -fill x
pack .bf.nxt -side top -padx 3 -pady 3 -fill x

pack .bf -side left -padx 1 -pady 1 -expand y -fill y

proc select_by_number { var elem op } {
    global select

    .lf.lst selection clear 0 end
    for {set i 0} {$i < [.lf.lst index end]} {incr i} {
	set f [.lf.lst get $i]
	if [regexp -nocase {^[a-z_.0]+([1-9][0-9]*)[a-z_.0]*} $f match n] {
	    if {$n >= $select(from) && $n <= $select(to)} {
		.lf.lst selection set $i
	    }
	}
    }
}

trace variable select w select_by_number

set select(test) test

#############################################################################

button .bf.q  -text "Quit" -command exit
button .bf.go -text "Make Cube" -command {
    set files ""
    foreach i [.lf.lst curselection] {
	set f [.lf.lst get $i]
	regexp {^[^ ]+} $f f ;# select everything from start to 1.space
	lappend files $f
    }
    puts $files
    eval exec mkfitscube $files >& /dev/tty
}

pack .bf.q .bf.go -side bottom -padx 3 -pady 3 -fill x

#############################################################################
#
# Fill the listbox
#
foreach f [lsort [glob *]] {
    if [file isfile $f] {
	if [regexp {\.gz$} $f] {
	    set infp [open "|gzcat $f"]
	} else {
	    set infp [open $f]
	}
	set line [read $infp 80]
	if ![regexp {^SIMPLE  = +} $line] {
	    catch {close $infp}
	    continue
	}
	set obj "???"
	for { set line "" } { ![regexp "^END     " $line]} {} {
	    set line [read $infp 80]
	    if { $line == "" } { break }
	    if [regexp {^OBJECT  = '?([^']+)'?} $line match obj] {
		break
	    }
	}
	catch {close $infp}

	.lf.lst insert end [format "%-18s %s" $f $obj]
    }
}
