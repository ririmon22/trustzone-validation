# sourceだけでは操作しない。適用は停止済み実機で別途承認した場合だけ行う。
proc tz_xmpu_test_apply {plan output authorization} {
    if {$authorization ne "REVIEWED_DEDICATED_XMPU_WRITE"} {error "BLOCKED: explicit XMPU write review required"}
    if {[dict get $plan schema] != 1 || [file exists $output]} {error "BLOCKED: invalid plan or existing output"}
    set selected [targets -target-properties -filter {is_current == 1}]
    if {[llength $selected] != 1 || ![string match -nocase *PSU* [dict get [lindex $selected 0] name]]} {
        error "BLOCKED: reviewed PSU physical access target required"
    }
    # state属性が得られない版では推測して続行せず、XSDB対応を確認する。
    set cpus [targets -target-properties -filter {name =~ "*Cortex-A53*"}]
    if {[llength $cpus] != 4} {error "BLOCKED: four A53 targets required"}
    foreach cpu $cpus {
        if {![dict exists $cpu state] || ![string match -nocase *stopped* [dict get $cpu state]]} {
            error "BLOCKED: all A53 cores must be confirmed stopped"
        }
    }
    set writes [dict get $plan writes]
    if {[llength $writes] == 0 || [llength $writes] % 8 != 0} {error "BLOCKED: invalid write groups"}
    set used {}
    foreach {a0 start a1 end a2 master a3 config} $writes {
        set bank [expr {$a0 & ~0xffff}]
        set offset [expr {$a0 - $bank}]
        if {$bank < 0xfd000000 || $bank > 0xfd050000 ||
            $offset < 0x100 || $offset > 0x1f0 || $offset % 16 != 0 ||
            $a1 != $a0+4 || $a2 != $a0+8 || $a3 != $a0+12 ||
            $start != 0x10800 || $end != 0x10800 || $config != 7 ||
            ($master & ~0x03ff03ff) != 0 || $bank in $used} {
            error "BLOCKED: only dedicated 0x10800000..0x10900000 region writes allowed"
        }
        lappend used $bank
        foreach address [list $a0 $a1 $a2 $a3] {
            if {![dict exists $plan expected $address]} {error "BLOCKED: missing pre-read"}
        }
        if {[dict get $plan expected $a3] & 1} {error "BLOCKED: region must start disabled"}
    }
    set stream [open $output w]
    puts $stream "XMPU TEST APPLY; hardware protection PASS is not implied"
    if {[catch {
        # どれか一つでも採取値から変わっていたら、最初の書込み前に拒否する。
        foreach {address expected} [dict get $plan expected] {
            set actual [mrd -value -size w $address 1]
            puts $stream "PRE $address $actual EXPECTED $expected"
            if {[llength $actual] != 1 || [expr {[lindex $actual 0] != $expected}]} {error "snapshot changed"}
        }
        foreach {address value} [dict get $plan writes] {
            puts $stream "WRITE $address $value"
            flush $stream
            mwr -size w $address $value
            set actual [mrd -value -size w $address 1]
            puts $stream "POST $address $actual EXPECTED $value"
            flush $stream
            if {[llength $actual] != 1 || [expr {[lindex $actual 0] != $value}]} {error "write readback mismatch"}
        }
        # 全bank適用後にも対象全fieldを再検査する。
        foreach {address value} [dict get $plan writes] {
            if {[expr {[lindex [mrd -value -size w $address 1] 0] != $value}]} {error "final readback mismatch"}
        }
    } reason]} {
        puts $stream "PARTIAL_OR_BLOCKED $reason; do not resume; no automatic rollback"
        close $stream
        error "BLOCKED: $reason; keep target stopped and use reviewed recovery"
    }
    puts $stream "APPLIED_READBACK_ONLY; probe/unchanged-data/continuation NOT_RUN"
    close $stream
}
