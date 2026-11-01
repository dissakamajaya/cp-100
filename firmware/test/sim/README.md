# Suite historis Rev08

Direktori ini menyimpan harness delapan pad dengan panel MCP23017 dan console lama. Skenarionya tidak berlaku untuk revisi empat pad + SERVICE via HP tanggal 31 Oktober 2026. Jangan mengutip hasil historis sebagai hasil firmware remote.

Suite aktif portable: `sh firmware/test/run-host.sh --asan` dari root repo. Coverage saat ini dan batasnya dijelaskan di `firmware/README.md`. Runner lama memakai GNU linker `--wrap` dan tidak portable ke Apple linker.
