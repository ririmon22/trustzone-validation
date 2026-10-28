# UG1725 2025.1のtargets/state/mrdに基づく。sourceだけではtargetへ接続しない。
proc tz_dump {filter plan output_dir} {
    # 対象0件・複数件を許可せず、番号を固定しない。
    targets -set -filter $filter
    if {![string match -nocase *stopped* [state]]} {
        error "BLOCKED: halt and review the target before collection; this procedure never stops it"
    }
    file mkdir $output_dir
    foreach item $plan {
        lassign $item name address bytes
        if {![regexp {^[a-zA-Z0-9_]+$} $name] || $bytes < 1 || $bytes > 16777248} {
            error "Invalid capture plan"
        }
        # -size bなのでcountはbyte数。MMIO範囲を自動探索しない。
        mrd -bin -file [file join $output_dir "$name.bin"] -size b $address $bytes
    }
}
# 副作用はCPU停止だけ。呼出し前に対象runと停止の影響をreviewする。
proc tz_stop_for_capture {filter} {
    targets -set -filter $filter
    stop
    set deadline [expr {[clock milliseconds] + 3000}]
    while {[clock milliseconds] < $deadline} {
        if {[string match -nocase *stopped* [state]]} { return }
        after 20
    }
    error "Target did not report stopped; do not collect an incoherent live dump"
}
