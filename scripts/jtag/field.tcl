# sourceだけでは接続/停止/読出し/書込みを行わない。
set tz_field_dir [file dirname [file normalize [info script]]]
source [file join $tz_field_dir capture.tcl]
source [file join $tz_field_dir a_unified.tcl]
source [file join $tz_field_dir b2_boot.tcl]
proc tz_field_check {plan_file} {
    source $plan_file
    puts "Plan syntax loaded for inspection; no reset/load/run issued"
}
proc tz_field_dump {filter plan_file output_dir} {
    # 生成planをglobal scopeへ読み込む。副作用は停止済みCPUのRAM読出しだけ。
    uplevel #0 [list source $plan_file]
    global tz_capture_plan
    tz_dump $filter $tz_capture_plan $output_dir
}
proc tz_field_stop {filter} { tz_stop_for_capture $filter }
proc tz_field_a_start {filter plan_file} {
    uplevel #0 [list source $plan_file]
    tz_a_load $filter
    tz_a_run_to $filter monitor 10000
}
proc tz_field_b2_start {plan_file} {
    uplevel #0 [list source $plan_file]
    tz_b2_execute
}
proc tz_field_reset {} { error "BLOCKED: reset scope/initialization not verified; no reset issued" }
