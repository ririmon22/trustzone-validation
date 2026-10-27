# ZCU106暫定配置

全値は配置案であり実機ロード許可ではない。Endは排他的。

|Region|Base|End|Size|Align|Security|Type|Owner|Purpose|
|---|---:|---:|---:|---:|---|---|---|---|
|el3_firmware|0x60000000|0x60010000|0x10000|0x1000|secure|normal-nc|el3|code|
|exception_vector|0x60010000|0x60020000|0x10000|0x1000|secure|normal-nc|el3|vector|
|secure_code|0x60020000|0x60030000|0x10000|0x1000|secure|normal-nc|secure|code|
|secure_data|0x60030000|0x60040000|0x10000|0x1000|secure|normal-nc|secure|data|
|secure_stack|0x60040000|0x60050000|0x10000|0x1000|secure|normal-nc|secure|stack|
|protected_buffer|0x62010000|0x62020000|0x10000|0x1000|secure|normal-nc|secure|64 KiB inspection buffer inside separate DDR XMPU window|
|el3_stack|0x60060000|0x60070000|0x10000|0x1000|secure|normal-nc|el3|stack|
|nonsecure_code|0x10200000|0x10300000|0x100000|0x1000|nonsecure|normal-nc|nonsecure|code|
|nonsecure_data|0x10300000|0x10400000|0x100000|0x1000|nonsecure|normal-nc|nonsecure|data|
|nonsecure_stack|0x10400000|0x10500000|0x100000|0x1000|nonsecure|normal-nc|nonsecure|stack|
|shared_memory|0x10500000|0x10600000|0x100000|0x1000|shared|normal-nc|client|shared|
|tfa_reserved|0x60080000|0x60100000|0x80000|0x1000|secure|normal-nc|tfa|reserved|
|optee_reserved|0x60100000|0x61000000|0xf00000|0x1000|secure|normal-nc|optee|reserved|
|xmpu_test_window|0x62000000|0x62100000|0x100000|0x100000|secure|normal-nc|secure-test|exclusive DDR XMPU allocation; no code, stacks or firmware|

XMPU windowは0x62000000..0x62100000の1 MiB専用予約。
検査バッファはその内部0x62010000..0x62020000の64 KiB。
Secureコード・データ・stack、TF-A/OP-TEEの暫定予約とは重ならない。
windowは独立した保護範囲で、regionsの別allocationと重ねて使用しない。
FSBL/PMUFW/BSPの生成使用範囲とDDR容量は未照合。未知の領域と競合しないとは判断しない。
経路・master ID・既存XMPU policyも未確定なのでBLOCKEDを維持する。

## Vitis報告値に基づく構成Aの見直し

[構成A更新資料](../../docs/zcu106_a_vitis_update.md)とvitis_observations.json/jtag_a_plan.jsonを参照する。DDR0報告範囲0..0x7ff00000内で既存Aアドレスを維持し、FSBL4・PMUFW3の報告LOADと全A予約/保護窓を数値比較する。DDR1は有効範囲不明のため使用しない。元ELF/address view/firmware runtime予約は未取得でconfirmed=false、external_reservations_complete=falseを維持する。

各linkerはstack全体を独立RW非実行NOLOAD PT_LOADとして予約し、check_memory_map.pyがp_memszとheader/JSONのstack範囲一致を検査する。heapは0 size。旧TF-A/OP-TEE予約はB構成との互換性用で、Aの起動対象に含めない。GPIO/DMA保留は安全ゲート解除の根拠にしない。
