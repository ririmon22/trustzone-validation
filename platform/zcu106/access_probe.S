.text
.global zcu106_probe_load
.global zcu106_probe_store
.global zcu106_probe_load_fault
.global zcu106_probe_load_resume
.global zcu106_probe_store_fault
.global zcu106_probe_store_resume
zcu106_probe_load:
zcu106_probe_load_fault:
    ldr x0, [x0]
zcu106_probe_load_resume:
    dsb sy
    ret
zcu106_probe_store:
zcu106_probe_store_fault:
    str x1, [x0]
zcu106_probe_store_resume:
    dsb sy
    ret

.global zcu106_probe_load32
.global zcu106_probe_store32
.global zcu106_probe_load32_fault
.global zcu106_probe_load32_resume
.global zcu106_probe_store32_fault
.global zcu106_probe_store32_resume
zcu106_probe_load32:
zcu106_probe_load32_fault:
    ldr w0, [x0]
zcu106_probe_load32_resume:
    dsb sy
    ret
zcu106_probe_store32:
zcu106_probe_store32_fault:
    str w1, [x0]
zcu106_probe_store32_resume:
    dsb sy
    ret
