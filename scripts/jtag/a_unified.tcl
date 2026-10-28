# sourceのみでは副作用なし。生成planは現在load_authorized=0のままとする。
proc tz_a_load {filter} {
    global tz_a_load_authorized tz_a_elf tz_a_stops
    if {!$tz_a_load_authorized} { error "BLOCKED: A unified load/run not authorized" }
    targets -set -filter $filter
    if {![string match -nocase *stopped* [state]]} { error "Target must already be halted by reviewed initialization" }
    dow $tz_a_elf
    if {[rrd -value pc] != [dict get $tz_a_stops entry]} { error "EL3 entry mismatch after download" }
}
proc tz_a_run_to {filter stage timeout_ms} {
    global tz_a_load_authorized tz_a_stops
    if {!$tz_a_load_authorized} { error "BLOCKED: A unified execution not authorized" }
    if {$stage ni {monitor secure normal finish} || $timeout_ms < 1 || $timeout_ms > 300000} { error "Invalid stop stage or timeout" }
    targets -set -filter $filter
    set expected [dict get $tz_a_stops $stage]
    set bp [bpadd -addr $expected -type hw]
    set deadline [expr {[clock milliseconds] + $timeout_ms}]
    con
    while {[clock milliseconds] < $deadline} {
        if {[string match -nocase *stopped* [state]]} {
            bpremove $bp
            if {[rrd -value pc] != $expected} { error "Unexpected stop; collect registers and diagnostic records" }
            return
        }
        after 20
    }
    stop
    bpremove $bp
    error "TIMEOUT: CPU stopped; collect diagnostics before reset/reload"
}
