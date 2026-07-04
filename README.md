# FreeRTOS QEMU Demo

> FreeRTOS running on **ARM Cortex-M3**, emulated via QEMU (`lm3s6965evb` — Stellaris LM3S6965).  
> Built from scratch as part of a learning path toward embedded systems security research and UAV autopilot development.

---

## What This Project Demonstrates

This project implements three foundational RTOS concepts in C, running on a real ARM Cortex-M3 instruction set (not simulated — QEMU executes actual ARM Thumb instructions):

| Concept | Implementation |
|---|---|
| **Task creation & preemptive scheduling** | Three independent tasks with different priorities |
| **Queue-based producer-consumer** | Sensor task produces data, processor task consumes it |
| **Binary semaphore** | Protects UART output from concurrent task access |

---

## Architecture

```
main()
├── xQueueCreate()           ← 5-slot SensorData queue
├── xSemaphoreCreateBinary() ← UART access guard
│
├── vSensorTask    [Priority 2] ── produces data every 500ms → Queue
├── vProcessorTask [Priority 3] ── consumes Queue → anomaly detection
└── vHeartbeatTask [Priority 1] ── heartbeat every 1s, reports free heap
         │
         ▼
    vTaskStartScheduler()
         │
         ▼
    SysTick (1000 Hz) → FreeRTOS tick → PendSV → context switch
```

---

## Sample Output

```
========================================
FreeRTOS QEMU Demo
ARM Cortex-M3 | lm3s6965evb
3 Task: Sensor + Processor + Heartbeat
========================================

[MAIN]   Task'lar olusturuldu. Scheduler basliyor...

[PROC]   Task basladi. Queue'dan veri bekliyor.
[SENSOR] Task basladi. Her 500ms veri uretecek.
[PROC]   Paket #1 islendi. Timestamp: 0ms
[SENSOR] Veri uretildi -> Sicaklik: 235 | Basinc: 101325
[HEART]  Beat #1 | Bos heap: 16248 byte
[PROC]   Paket #2 islendi. Timestamp: 500ms
[SENSOR] Veri uretildi -> Sicaklik: 236 | Basinc: 101332
[PROC]   Paket #7 islendi. Timestamp: 3000ms [!] YUKSEK SICAKLIK UYARISI
```

Task start order reflects priority: `[PROC]` (3) starts before `[SENSOR]` (2) before `[HEART]` (1).  
Timestamps are exactly 500ms apart — SysTick running at 1000Hz confirmed.

---

## Build & Run

### Requirements

```bash
sudo apt install gcc-arm-none-eabi qemu-system-arm
```

### Build

```bash
git clone https://github.com/mVonal/FreeRTOS-QEMU-demo.git
cd FreeRTOS-QEMU-demo
make
```

Expected output:
```
--- Bellek Kullanimi ---
   text    data     bss     dec     hex filename
  10544       4   24872   35420    8a5c freertos-demo.elf
```

### Run

```bash
make run
```

Exit QEMU: `Ctrl+A` then `X`

### Debug (GDB)

```bash
make debug   # starts QEMU waiting on port 1234

# in another terminal:
arm-none-eabi-gdb freertos-demo.elf
(gdb) target remote :1234
(gdb) break main
(gdb) continue
```

---

## Project Structure

```
FreeRTOS-QEMU-demo/
├── src/
│   ├── main.c              ← Three FreeRTOS tasks + queue + semaphore
│   ├── startup.c           ← ARM Cortex-M3 vector table + Reset_Handler
│   ├── FreeRTOSConfig.h    ← RTOS configuration (tick rate, heap, priorities)
│   └── mem.c               ← Bare-metal memset/memcpy (no stdlib)
├── freertos/
│   ├── tasks.c             ← Scheduler, context switch, task management
│   ├── queue.c             ← Queue, semaphore, mutex implementation
│   ├── list.c              ← Internal linked list (ready/blocked/delayed tasks)
│   ├── timers.c            ← Software timers
│   ├── event_groups.c      ← Event flag groups
│   ├── include/            ← FreeRTOS public headers
│   └── portable/
│       ├── GCC/ARM_CM3/    ← Cortex-M3 port (PendSV, SysTick, SVC handlers)
│       └── MemMang/        ← heap_4.c — dynamic memory management
├── lm3s6965.ld             ← Linker script (Flash/RAM layout, vector table)
└── Makefile                ← Cross-compilation with arm-none-eabi-gcc
```

---

## Key Technical Details

**Why `-nostdlib`?**  
No operating system, no C runtime. `memset`/`memcpy` are provided manually in `mem.c`. This is standard bare-metal practice.

**How does context switch work?**  
SysTick fires every 1ms → FreeRTOS tick handler runs → if a higher-priority task became ready, PendSV is pended → when all other interrupts clear, PendSV fires → saves current task registers to its stack, loads next task's registers → execution continues in new task. All of this is in `freertos/portable/GCC/ARM_CM3/port.c`.

**Stack overflow detection:**  
`configCHECK_FOR_STACK_OVERFLOW = 2` in `FreeRTOSConfig.h`. On every context switch, FreeRTOS checks if the stack canary at the bottom of each task's stack has been overwritten. If so, `vApplicationStackOverflowHook()` is called — critical on Cortex-M3 which has no MMU.

---

## Security Relevance

This project is part of a broader UAV security research series:

- Understanding how autopilot software (ArduPilot, PX4) manages concurrent tasks helps identify attack surfaces in real-time control loops
- Stack overflow vulnerabilities in RTOS tasks without MMU protection can lead to unpredictable behavior in flight-critical systems
- JTAG/SWD debug interfaces (which this project uses via `make debug`) are a primary hardware attack vector on production UAV systems — understanding them from a development perspective informs defensive security assessments

**Related project:** [mavlink-fuzzer](https://github.com/mVonal/mavlink-fuzzer) — black-box MAVLink v2 protocol fuzzer tested against ArduPilot SITL.

---

## References

- [FreeRTOS Documentation](https://freertos.org/Documentation/RTOS_book.html)
- [ARM Cortex-M3 Technical Reference](https://developer.arm.com/documentation/ddi0337/latest/)
- [QEMU ARM machines](https://qemu.org/docs/master/system/target-arm.html)
- [FreeRTOS Kernel Source](https://github.com/FreeRTOS/FreeRTOS-Kernel)
