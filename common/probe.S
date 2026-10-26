.text
.global probe_read
.global probe_write
.global probe_read_fault
.global probe_read_resume
.global probe_write_fault
.global probe_write_resume
probe_read:
probe_read_fault:
    ldr x1, [x0]
probe_read_resume:
    ret
probe_write:
probe_write_fault:
    str x1, [x0]
probe_write_resume:
    ret
