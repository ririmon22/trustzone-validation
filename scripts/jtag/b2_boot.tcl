# sourceのみでは操作しない。生成planのBLOCKEDを変更して実行してはならない。
proc tz_wait_stopped {pc timeout_ms} {
    set deadline [expr {[clock milliseconds] + $timeout_ms}]
    while {[clock milliseconds] < $deadline} {
        if {[string match -nocase *stopped* [state]]} {
            set actual [rrd -value pc]
            if {$actual != $pc} { error "Stopped at unexpected PC: $actual expected $pc" }
            return
        }
        after 20
    }
    stop
    error "TIMEOUT: target stopped for register and RAM-log inspection"
}
proc tz_b2_execute {} {
    global tz_boot_authorized tz_images tz_apu_filter tz_pmu_filter tz_pmu_ready tz_stops tz_timeout
    if {!$tz_boot_authorized} { error "BLOCKED: no load/run/reset permitted; collect missing evidence first" }
    # 副作用: PMUとAPUへのdownload、breakpoint設定、実行、timeout時停止。
    # resetや保護register変更は行わず、初期化条件は外側のreviewで確定させる。
    targets -set -filter $tz_pmu_filter
    dow [dict get $tz_images pmufw]
    con
    lassign $tz_pmu_ready address mask expected
    set deadline [expr {[clock milliseconds] + $tz_timeout}]
    set ready 0
    while {[clock milliseconds] < $deadline} {
        if {([lindex [mrd -value -size w $address 1] 0] & $mask) == $expected} { set ready 1; break }
        after 20
    }
    if {!$ready} { error "PMUFW readiness timeout; FSBL/BL31 will not be started" }
    targets -set -filter $tz_apu_filter
    dow [dict get $tz_images fsbl]
    set bp [bpadd -addr [dict get $tz_stops fsbl_exit] -type hw]
    con
    tz_wait_stopped [dict get $tz_stops fsbl_exit] $tz_timeout
    bpremove $bp
    # FSBL Exit到達を引渡し条件の確認と混同しない。
    foreach role {bl32 bl33 bl31} { dow [dict get $tz_images $role] }
    if {[rrd -value pc] != [dict get $tz_stops bl31_entry]} { error "BL31 entry PC mismatch after download" }
    # BL31 entryで止めてEL/SCR/MMU/cacheを人が確認する。ここでは先へ進めない。
    puts "LOADED: review BL31 entry and handoff state before a separate approved continuation"
}
# 以下はBL31停止状態を人がreviewした後の段階別候補。現行planでは到達不能。
proc tz_b2_to_optee {} {
    global tz_boot_authorized tz_stops tz_timeout
    if {!$tz_boot_authorized} { error "BLOCKED: BL31 execution not authorized" }
    set bp [bpadd -addr [dict get $tz_stops optee_init] -type hw]
    con
    tz_wait_stopped [dict get $tz_stops optee_init] $tz_timeout
    bpremove $bp
    puts "OP-TEE initialization stop reached; review EL and registers before continuing"
}
proc tz_b2_to_bl33 {} {
    global tz_boot_authorized tz_stops tz_timeout
    if {!$tz_boot_authorized} { error "BLOCKED: BL33 continuation not authorized" }
    set bp [bpadd -addr [dict get $tz_stops bl33_main] -type hw]
    con
    tz_wait_stopped [dict get $tz_stops bl33_main] $tz_timeout
    bpremove $bp
    puts "BL33 stop reached; runtime-ready gate remains authoritative"
}
