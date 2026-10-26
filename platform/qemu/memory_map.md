領域終端は排他的。QEMU 9.2 virt secure RAM窓に基づく。実行未確認。

|Region|Base|End|Size|Align|Security|Type|Owner|Purpose|Protection|
|---|---:|---:|---:|---:|---|---|---|---|---|
|el3_firmware|0xe000000|0xe010000|0x10000|0x1000|secure|normal-nc|el3|code|qemu-secure-ram|
|exception_vector|0xe010000|0xe020000|0x10000|0x1000|secure|normal-nc|el3|vector|qemu-secure-ram|
|secure_code|0xe020000|0xe030000|0x10000|0x1000|secure|normal-nc|secure|code|qemu-secure-ram|
|secure_data|0xe030000|0xe040000|0x10000|0x1000|secure|normal-nc|secure|data|qemu-secure-ram|
|secure_stack|0xe040000|0xe050000|0x10000|0x1000|secure|normal-nc|secure|stack|qemu-secure-ram|
|protected_buffer|0xe050000|0xe060000|0x10000|0x1000|secure|normal-nc|secure|protected|qemu-secure-ram|
|el3_stack|0xe060000|0xe070000|0x10000|0x1000|secure|normal-nc|el3|stack|qemu-secure-ram|
|nonsecure_code|0x40200000|0x40300000|0x100000|0x1000|nonsecure|normal-nc|nonsecure|code|none|
|nonsecure_data|0x40300000|0x40400000|0x100000|0x1000|nonsecure|normal-nc|nonsecure|data|none|
|nonsecure_stack|0x40400000|0x40500000|0x100000|0x1000|nonsecure|normal-nc|nonsecure|stack|none|
|shared_memory|0x40500000|0x40600000|0x100000|0x1000|shared|normal-nc|client|shared|none|
|tfa_reserved|0xe080000|0xe100000|0x80000|0x1000|secure|normal-nc|tfa|reserved|qemu-secure-ram|
|optee_reserved|0xe100000|0xf000000|0xf00000|0x1000|secure|normal-nc|optee|reserved|qemu-secure-ram|
