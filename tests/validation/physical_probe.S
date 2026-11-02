.text
.global b2_physical_read
.global b2_physical_read_fault
.global b2_physical_read_resume
.global b2_physical_write
.global b2_physical_write_fault
.global b2_physical_write_resume
b2_physical_read:
b2_physical_read_fault:
    ldr x0, [x0]
b2_physical_read_resume:
    ret
b2_physical_write:
b2_physical_write_fault:
    str x1, [x0]
    dsb sy
b2_physical_write_resume:
    ret
