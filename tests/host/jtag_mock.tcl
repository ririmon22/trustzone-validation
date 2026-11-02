# PCのTclだけで引数・順序・拒否を検査する。XSDB/MMIOや実機の代替ではない。
set root [file normalize [file join [file dirname [info script]] ../..]]
set calls {}
set downloads {}
set target_count 1
set pc 0x1000
set removed {}
set wrong_stop 0
proc targets {args} {
    if {[lsearch $args -target-properties] >= 0} {
        return [lrepeat $::target_count [dict create name SYNTHETIC]]
    }
    lappend ::calls [linsert $args 0 targets]
}
proc dow {args} {lappend ::downloads $args; set ::pc 0x1000}
proc con {} {lappend ::calls con}
proc mrd {args} {lappend ::calls [linsert $args 0 mrd]; return {1}}
proc state {} {return Stopped}
proc rrd {args} {return $::pc}
proc bpadd {args} {set ::pc [expr {$::wrong_stop ? 0x99 : [dict get $::tz_stops fsbl_exit]}]; return 1}
proc bpremove {args} {lappend ::removed [lindex $args 0]}
proc stop {} {lappend ::calls stop}
proc assert {value message} {if {![uplevel 1 [list expr $value]]} {error $message}}
source [file join $root scripts/jtag/b2_boot.tcl]
source [file join $root scripts/jtag/capture.tcl]
source [file join $root scripts/jtag/xmpu_test.tcl]
assert {[llength $calls] == 0} "source performed operations"
set tz_boot_authorized 0
foreach action {tz_b2_execute tz_b2_to_optee tz_b2_to_bl33} {
    assert {[catch {$action} message] && [string match BLOCKED:* $message]} "boot gate failed"
}
assert {[llength $calls] == 0} "blocked plan performed operations"
set tz_apu_filter SYNTHETIC_APU
set tz_pmu_filter SYNTHETIC_PMU
set tz_images [dict create pmufw PMU.elf fsbl FSBL.elf bl32 BL32.elf bl33 BL33.elf bl31 BL31.elf]
set tz_stops [dict create fsbl_exit 0x1234 bl31_entry 0x1000 optee_init 0x5678 bl33_main 0x9000]
set tz_pmu_ready {0 1 1}
set tz_timeout 10
set tz_signed_bundle [file normalize [lindex $argv 0]]
# ここでの許可1はmock関数だけを呼ぶ試験入力。実機用生成Tclは常に0。
set tz_boot_authorized 1
foreach count {0 2} {
    set target_count $count
    assert {[catch {tz_b2_execute} message] && [string match BLOCKED:* $message]} "ambiguous boot target accepted"
    assert {[catch {tz_dump FILTER {} /tmp/never-created-by-mock} message] && [string match BLOCKED:* $message]} "ambiguous capture target accepted"
}
assert {[llength $calls] == 0 && [llength $downloads] == 0} "target rejection performed operations"
set target_count 1
tz_b2_execute
assert {$downloads == [list {PMU.elf} {FSBL.elf} {BL32.elf} {BL33.elf} [list -data $tz_signed_bundle 0x10600000] {BL31.elf}]} "download order/arguments mismatch"
set removed {}
set downloads {}
set wrong_stop 1
assert {[catch {tz_b2_execute} message] && [string match "Stopped at unexpected PC:*" $message]} "unexpected FSBL PC accepted"
assert {$removed == {1} && $downloads == [list PMU.elf FSBL.elf]} "failed stop must remove breakpoint before loading BL32"
set wrong_stop 0
set calls {}
assert {[catch {tz_wait_stopped 0x99 0}]} "timeout accepted"
assert {$calls == {stop}} "timeout must stop"
assert {[catch {tz_xmpu_test_apply {} /tmp/never-created-by-mock DENIED} message] && [string match BLOCKED:* $message]} "unreviewed XMPU accepted"
puts "JTAG mock PASS: gate, unique targets, ELF/bundle load order, timeout, XMPU rejection; no hardware"
