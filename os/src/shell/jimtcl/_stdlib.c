/* autogenerated - do not edit */
#include <jim.h>
int Jim_stdlibInit(Jim_Interp *interp)
{
	if (Jim_PackageProvide(interp, "stdlib", "1.0", JIM_ERRMSG)) return JIM_ERR;
	return Jim_EvalSource(interp, "stdlib.tcl", 1, "\n"
"\n"
"\n"
"proc lambda {arglist args} {\n"
"	tailcall proc [ref {} function lambda.finalizer] $arglist {*}$args\n"
"}\n"
"\n"
"proc lambda.finalizer {name val} {\n"
"	rename $name {}\n"
"}\n"
"\n"
"\n"
"proc curry {args} {\n"
"	alias [ref {} function lambda.finalizer] {*}$args\n"
"}\n"
"\n"
"\n"
"\n"
"\n"
"\n"
"\n"
"\n"
"\n"
"\n"
"proc function {value} {\n"
"	return $value\n"
"}\n"
"\n"
"\n"
"\n"
"\n"
"proc stacktrace {{skip 0}} {\n"
"	set trace {}\n"
"	incr skip\n"
"	foreach level [range $skip [info level]] {\n"
"		lappend trace {*}[info frame -$level]\n"
"	}\n"
"	return $trace\n"
"}\n"
"\n"
"\n"
"proc stackdump {stacktrace} {\n"
"	set lines {}\n"
"	foreach {l f p} [lreverse $stacktrace] {\n"
"		set line {}\n"
"		if {$p ne \"\"} {\n"
"			append line \"in procedure '$p' \"\n"
"			if {$f ne \"\"} {\n"
"				append line \"called \"\n"
"			}\n"
"		}\n"
"		if {$f ne \"\"} {\n"
"			append line \"at file \\\"$f\\\", line $l\"\n"
"		}\n"
"		if {$line ne \"\"} {\n"
"			lappend lines $line\n"
"		}\n"
"	}\n"
"	join $lines \\n\n"
"}\n"
"\n"
"\n"
"\n"
"proc errorInfo {msg {stacktrace \"\"}} {\n"
"	if {$stacktrace eq \"\"} {\n"
"\n"
"		set stacktrace [info stacktrace]\n"
"\n"
"		lappend stacktrace {*}[stacktrace 1]\n"
"	}\n"
"	lassign $stacktrace p f l\n"
"	if {$f ne \"\"} {\n"
"		set result \"Runtime Error: $f:$l: \"\n"
"	}\n"
"	append result \"$msg\\n\"\n"
"	append result [stackdump $stacktrace]\n"
"\n"
"\n"
"	string trim $result\n"
"}\n"
"\n"
"\n"
"\n"
"proc {info nameofexecutable} {} {\n"
"	if {[info exists ::jim_argv0]} {\n"
"		if {[string match \"*/*\" $::jim_argv0]} {\n"
"			return [file join [pwd] $::jim_argv0]\n"
"		}\n"
"		foreach path [split [env PATH \"\"] $::tcl_platform(pathSeparator)] {\n"
"			set exec [file join [pwd] [string map {\\\\ /} $path] $::jim_argv0]\n"
"			if {[file executable $exec]} {\n"
"				return $exec\n"
"			}\n"
"		}\n"
"	}\n"
"	return \"\"\n"
"}\n"
"\n"
"\n"
"proc {dict with} {&dictVar {args key} script} {\n"
"	set keys {}\n"
"	foreach {n v} [dict get $dictVar {*}$key] {\n"
"		upvar $n var_$n\n"
"		set var_$n $v\n"
"		lappend keys $n\n"
"	}\n"
"	catch {uplevel 1 $script} msg opts\n"
"	if {[info exists dictVar] && ([llength $key] == 0 || [dict exists $dictVar {*}$key])} {\n"
"		foreach n $keys {\n"
"			if {[info exists var_$n]} {\n"
"				dict set dictVar {*}$key $n [set var_$n]\n"
"			} else {\n"
"				dict unset dictVar {*}$key $n\n"
"			}\n"
"		}\n"
"	}\n"
"	return {*}$opts $msg\n"
"}\n"
"\n"
"\n"
"proc {dict update} {&varName args script} {\n"
"	set keys {}\n"
"	foreach {n v} $args {\n"
"		upvar $v var_$v\n"
"		if {[dict exists $varName $n]} {\n"
"			set var_$v [dict get $varName $n]\n"
"		}\n"
"	}\n"
"	catch {uplevel 1 $script} msg opts\n"
"	if {[info exists varName]} {\n"
"		foreach {n v} $args {\n"
"			if {[info exists var_$v]} {\n"
"				dict set varName $n [set var_$v]\n"
"			} else {\n"
"				dict unset varName $n\n"
"			}\n"
"		}\n"
"	}\n"
"	return {*}$opts $msg\n"
"}\n"
"\n"
"\n"
"\n"
"proc {dict merge} {dict args} {\n"
"	foreach d $args {\n"
"\n"
"		dict size $d\n"
"		foreach {k v} $d {\n"
"			dict set dict $k $v\n"
"		}\n"
"	}\n"
"	return $dict\n"
"}\n"
"\n"
"proc {dict replace} {dictionary {args {key value}}} {\n"
"	if {[llength ${key value}] % 2} {\n"
"		tailcall {dict replace}\n"
"	}\n"
"	tailcall dict merge $dictionary ${key value}\n"
"}\n"
"\n"
"\n"
"proc {dict lappend} {varName key {args value}} {\n"
"	upvar $varName dict\n"
"	if {[exists dict] && [dict exists $dict $key]} {\n"
"		set list [dict get $dict $key]\n"
"	}\n"
"	lappend list {*}$value\n"
"	dict set dict $key $list\n"
"}\n"
"\n"
"\n"
"proc {dict append} {varName key {args value}} {\n"
"	upvar $varName dict\n"
"	if {[exists dict] && [dict exists $dict $key]} {\n"
"		set str [dict get $dict $key]\n"
"	}\n"
"	append str {*}$value\n"
"	dict set dict $key $str\n"
"}\n"
"\n"
"\n"
"proc {dict incr} {varName key {increment 1}} {\n"
"	upvar $varName dict\n"
"	if {[exists dict] && [dict exists $dict $key]} {\n"
"		set value [dict get $dict $key]\n"
"	}\n"
"	incr value $increment\n"
"	dict set dict $key $value\n"
"}\n"
"\n"
"\n"
"proc {dict remove} {dictionary {args key}} {\n"
"	foreach k $key {\n"
"		dict unset dictionary $k\n"
"	}\n"
"	return $dictionary\n"
"}\n"
"\n"
"\n"
"proc {dict values} {dictionary {pattern *}} {\n"
"	dict keys [lreverse $dictionary] $pattern\n"
"}\n"
"\n"
"\n"
"proc {dict for} {vars dictionary script} {\n"
"	if {[llength $vars] != 2} {\n"
"		return -code error \"must have exactly two variable names\"\n"
"	}\n"
"	dict size $dictionary\n"
"	tailcall foreach $vars $dictionary $script\n"
"}\n");
}