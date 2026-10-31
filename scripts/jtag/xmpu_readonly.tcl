# UG1085第16章の既知DDR XMPU offsetを使用する。未知のlock定義は推測しない。
# sourceだけでは接続・読出し・停止を行わない。未知のLOCK offsetは推測しない。
proc tz_xmpu0_readonly {output authorization} {
    if {$authorization ne "REVIEWED_XMPU0_READONLY"} { error "BLOCKED: review XMPU0 read access first" }
    if {![string match -nocase *stopped* [state]]} { error "BLOCKED: selected target must already be stopped" }
    if {[file exists $output]} { error "Preserve previous snapshot; choose a new file" }
    set stream [open $output w]
    puts $stream "XMPU0_BASE=0xFD000000; LOCK_STATE=UNRESOLVED; no writes or clear performed"
    set offsets {0x0 0x4 0x8 0xC 0x10}
    for {set region 0} {$region < 16} {incr region} {
        foreach field {0 4 8 12} { lappend offsets [expr {0x100 + $region * 0x10 + $field}] }
    }
    foreach offset $offsets {
        set address [expr {0xFD000000 + $offset}]
        if {[catch {mrd -value -size w $address 1} value]} {
            puts $stream [format "0x%08X READ_ERROR %s" $address $value]
            close $stream
            error "BLOCKED: partial snapshot saved; do not continue scanning other banks"
        }
        puts $stream [format "0x%08X %s" $address $value]
    }
    close $stream
    puts "READONLY snapshot saved; lock/master/route/protection remain unverified"
}

# PSU経由の既知DDR XMPUだけを採取する。target選択・reset・writeは行わない。
proc tz_ddr_xmpu_readonly {output authorization} {
    if {$authorization ne "REVIEWED_PSU_DDR_XMPU_READONLY"} { error "BLOCKED: review PSU read route first" }
    set matches [targets -target-properties -filter {is_current == 1}]
    if {[llength $matches] != 1} { error "BLOCKED: exactly one current target is required" }
    set selected [lindex $matches 0]
    if {![dict exists $selected name] || ![string match -nocase *PSU* [dict get $selected name]]} {
        error "BLOCKED: select the reviewed PSU target; A53 mrd can undergo translation"
    }
    if {[file exists $output]} { error "Preserve previous snapshot; choose new output" }
    set stream [open $output w]
    puts $stream "READONLY; USER_REPORTED CTRL=0xB CONFIG=0x8 is not all-path protection evidence; LOCK unresolved"
    for {set bank 0} {$bank < 6} {incr bank} {
        set base [expr {0xFD000000 + $bank * 0x10000}]
        set offsets {0 4 8 12 16}
        for {set region 0} {$region < 16} {incr region} {
            foreach field {0 4 8 12} { lappend offsets [expr {0x100 + 0x10*$region + $field}] }
        }
        foreach offset $offsets {
            set address [expr {$base+$offset}]
            if {[catch {mrd -value -size w $address 1} value]} {
                puts $stream [format "BANK=%d ADDRESS=0x%08X READ_ERROR %s" $bank $address $value]
                close $stream
                error "BLOCKED: partial read saved; no further bank scanned"
            }
            puts $stream [format "BANK=%d ADDRESS=0x%08X VALUE=%s" $bank $address $value]
        }
    }
    close $stream
}
