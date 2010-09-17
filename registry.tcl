#!/usr/bin/wish
#
# Copyright (C) Youness Alaoui (KaKaRoTo)
#
# This software is distributed under the terms of the GNU General Public
# License ("GPL") version 3, as published by the Free Software Foundation.
#


proc hexify { str } {
	set out ""
	for {set i 0} { $i < [string length $str] } { incr i} {
		set c [string range $str $i $i]
		if {[string is ascii $c] && (![string is control $c] || $c == "\r" || $c == "\n") } {
			append out $c
		} else {
			binary scan $c H* h
			append out "\[[string toupper $h]\]"
		}
	}
	set out
}

proc hexify_all { str } {
	set out ""
	for {set i 0} { $i < [string length $str] } { incr i} {
		set c [string range $str $i $i]
		binary scan $c H* h
		append out "\[[string toupper $h]\]"
	}
	set out
}

proc unhexify { str } {
	set out "" 
	for { set i 0 } { $i < [string length $str] } { incr i } {
		if {[string range $str $i $i] == "\[" &&
		    [string length $str] > [expr {$i + 3}]  &&
		    [string range $str [expr {$i + 3}] [expr {$i + 3}]] == "\]" } {
			set d1 [string range $str [expr {$i + 1}] [expr {$i + 1}]]
			set d2 [string range $str [expr {$i + 2}] [expr {$i + 2 }]]
			if {([string is digit $d1] || $d1 == "a" || $d1 == "b" ||
			     $d1 == "c" || $d1 == "d" || $d1 == "e" || $d1 == "f") || 
			    ([string is digit $d2] || $d2 == "a" || $d2 == "b" ||
				     $d2 == "c" || $d2 == "d" || $d2 == "e" || $d2 == "f")} {
				append out [binary format H* "${d1}${d2}"]
				incr i 3
			} else {
				append out "\[${d1}${d2}\]"
			}
		} else {
			append out [string range $str $i $i]
		}
	}
	return $out
}


proc hex_to_string { hex } {
	return [string map { "\[" "" "\]" "" } $hex]
}
proc hexdump {str} {
	return [hex_to_string [hexify_all $str]]
}

proc read_key { fd } {
	set key(offset) [expr {[tell $fd] - 16}]
	set data [read $fd 5]
	#puts "Reading key at $key(offset) : [hexdump $data]"
	if {[hexdump $data] == "AABBCCDDEE" } {
		puts "Found end marker for keys"
		return [list]
	}
	binary scan $data SSc unknown length type
	set string [read $fd $length]
	set del [read $fd 1]
	if {$del != "\x00" } {
		puts "WARNING: Key delimiter at offset $key(offset) is not 0 : 0x[hexdump $del]"
	}

	set key(key_unk) $unknown
	set key(key_type) $type
	set key(key) $string

	return [array get key]
}

proc read_value { fd } {
	set data [read $fd 9]
	binary scan $data ISSc offset unknown length type
	#puts "Reading value at $offset : [hexdump $data]"
	if {[hexdump [string range $data 0 4]] == "AABBCCDDEE" } {
		puts "Found end marker for values"
		return [list]
	}
	set content [read $fd $length]
	set del [read $fd 1]
	if {$del != "\x00" } {
		puts "WARNING: Value delimiter at offset $offset is not 0 : 0x[hexdump $del]"
	}

	set value(offset) $offset
	set value(value_unk) $unknown
	set value(value_type) $type
	set value(length) $length
	set value(content) $content

	return [array get value]
}

proc build_ui { } {
	global registry
	
	set w_offset .f.f0.list
	set w_key .f.f1.list
	set w_key_type .f.f2.list
	set w_value_type .f.f3.list
	set w_length .f.f4.list
	set w_content .f.f5.list

	frame .f
	frame .f.f0
	pack [label .f.f0.header -bg grey -text "Offset"] -expand false -fill x -padx 2
	pack [listbox .f.f0.list -yscrollcommand "scroll_y list" -width 6] -expand true -fill both
	pack .f.f0 -side left -expand false -fill both
	frame .f.f1
	pack [label .f.f1.header -bg grey -text "Key"] -expand false -fill x -padx 2
	pack [listbox .f.f1.list -yscrollcommand "scroll_y list" -width 50] -expand true -fill both
	pack .f.f1 -side left -expand false -fill both
	frame .f.f2
	pack [label .f.f2.header -bg grey -text "Key type"] -expand false -fill x -padx 2
	pack [listbox .f.f2.list -yscrollcommand "scroll_y list" -width 1] -expand true -fill both
	pack .f.f2 -side left -expand false -fill both
	frame .f.f3
	pack [label .f.f3.header -bg grey -text "Value type"] -expand false -fill x -padx 2
	pack [listbox .f.f3.list -yscrollcommand "scroll_y list" -width 1] -expand true -fill both
	pack .f.f3 -side left -expand false -fill both
	frame .f.f4
	pack [label .f.f4.header -bg grey -text "Length"] -expand false -fill x -padx 2
	pack [listbox .f.f4.list -yscrollcommand "scroll_y list" -width 3] -expand true -fill both
	pack .f.f4 -side left -expand false -fill both
	frame .f.f5
	pack [label .f.f5.header -bg grey -text "Value"] -expand false -fill x -padx 2
	pack [listbox .f.f5.list -yscrollcommand "scroll_y list"] -expand true -fill both
	pack .f.f5 -side left -expand true -fill both

	proc ::scroll_y { from arg1 arg2 } {
		if {$from == "list" } {
			set start $arg1
			set end $arg2
			for {set i 0} {$i <= 5} {incr i} {
				.f.f$i.list yview moveto $start
			}
			.scroll_y set $start $end
		} elseif {$from == "scroll"} {
			set start $arg2
			.f.f0.list yview moveto $start
			foreach {start end} [.f.f0.list yview] break
			for {set i 0} {$i <= 5} {incr i} {
				.f.f$i.list yview moveto $start
			}
			.scroll_y set $start $end
		}
	}
	scrollbar .scroll_y -command "scroll_y scroll" -highlightthickness 0 \
		-borderwidth 1 -elementborderwidth 2


	pack .scroll_y -side right -expand false -fill y
	pack .f -side right -expand true -fill both
	#pack .scroll_x -side bottom -expand true -fill x


	set offsets [lsort -integer [array names registry]]

	foreach offset $offsets {
		array set data [set registry($offset)]
		catch {unset key}
		catch {unset content}
		if {[info exists data(key)] } {
			set key $data(key)
			set key_unk $data(key_unk)
			set key_type $data(key_type)
		}

		if {[info exists data(content)] } {
			set length $data(length)
			set content $data(content)
			set value_type $data(value_type)
			set value_unk $data(value_unk)
			set trimmed_content [string trim $content "\x00"]
			if {$trimmed_content == ""} {
				set trimmed_content "\x00"
			}
			set content_to_show [hexify $trimmed_content]
		} 

		$w_offset insert end "[format 0x%0.4X $offset]"

		if {![info exists key] } {
			$w_key insert end "N/A"
			$w_key itemconfigure end -background red
			$w_key_type insert end "N/A"
			$w_key_type itemconfigure end -background red
		} else {
			$w_key insert end "$key"
			$w_key_type insert end "$key_type"
		}

		if {![info exists content] } {
			$w_value_type insert end "N/A"
			$w_value_type itemconfigure end -background red
			$w_length insert end "N/A"
			$w_length itemconfigure end -background red
			$w_content insert end "N/A"
			$w_content itemconfigure end -background red
		} else {
			$w_value_type insert end "$value_type"
			$w_length insert end "$length"
			$w_content insert end "$content_to_show"
		}
	}
}

if {[llength $argv] != 1} {
	puts "Usage: registry.tcl /path/to/xRegistry.sys"
	exit
}



set filename [lindex $argv 0]
if { [catch {set fd [open $filename]}] } {
	puts "File not found"
	exit
}
fconfigure $fd -translation binary

set header [read $fd 16]

if {[hexdump  $header] != "BCADADBC0000009000000002BCADADBC"} {
	puts "Wrong file format : [hexdump $header]"
	exit
}


global registry

while {1} {
	set key [read_key $fd]
	if {$key == [list] } {
		break
	}
	catch {unset data}
	array set data $key
	set offset $data(offset)
	set registry($offset) $key
}

seek $fd [expr 0x10000] start
while {1} {
	set value [read_value $fd]
	if {$value == [list] } {
		break;
	}
	catch {unset data}
	array set data $value
	set offset $data(offset)
	array set data [set registry($offset)]
	set registry($offset) [array get data]
}

build_ui