/* startup.c — ARM Cortex-M3 baslangic kodu
 *
 * CPU reset oldugunda veya QEMU basladiginda ilk calisan kod burasıdır.
 * Blog yazisinda konustugumuz konular burada hayat buluyor:
 * - Interrupt Vector Table
 * - Stack pointer baslatma
 * - .data ve .bss segment baslatma
 * - main()'e atlama
 */

#include <stdint.h>

/* main() ve FreeRTOS interrupt handler'larinin prototipleri */
extern int main(void);
extern void xPortPendSVHandler(void);   /* PendSV: context switch */
extern void xPortSysTickHandler(void);  /* SysTick: RTOS tick */
extern void vPortSVCHandler(void);      /* SVC: ilk task baslatma */

/* Linker script'ten gelen semboller.
 * Linker script bu isimleri tanimlıyor, biz burada sadece
 * "bu adresler var" diye bildiriyoruz. */
extern uint32_t _estack;      /* stack'in bitis adresi (RAM'in tepesi) */
extern uint32_t _sdata;       /* .data segmentinin RAM'deki baslangici */
extern uint32_t _edata;       /* .data segmentinin RAM'deki bitisi */
extern uint32_t _sidata;      /* .data segmentinin Flash'taki kaynak adresi */
extern uint32_t _sbss;        /* .bss segmentinin baslangici */
extern uint32_t _ebss;        /* .bss segmentinin bitisi */

/* Reset handler — CPU basladiginda burasi calisir */
void Reset_Handler(void);

/* Varsayilan interrupt handler — tanimlanmamis interrupt'lar buraya duser */
void Default_Handler(void) {
    /* Sonsuz dongu — debugger ile yakalanabilir */
    while (1) {}
}

/* ----------------------------------------------------------------
 * Interrupt Vector Table
 *
 * Blog: "CPU reset oldugunda ilk 4 byte'i okur: stack pointer.
 * Sonraki 4 byte: Reset_Handler adresi."
 *
 * Bu tablo Flash'in en basina yerlesir (linker script ile).
 * Cortex-M3 basladiginda bu tabloyu okur.
 * ---------------------------------------------------------------- */
__attribute__((section(".isr_vector")))
void (* const g_pfnVectors[])(void) = {
    (void (*)(void))(&_estack),   /* 0x00: Ilk stack pointer degeri */
    Reset_Handler,                 /* 0x04: Reset — CPU basladiginda */
    Default_Handler,               /* 0x08: NMI */
    Default_Handler,               /* 0x0C: HardFault */
    Default_Handler,               /* 0x10: MemManage */
    Default_Handler,               /* 0x14: BusFault */
    Default_Handler,               /* 0x18: UsageFault */
    0, 0, 0, 0,                   /* 0x1C-0x2B: Reserved */
    vPortSVCHandler,               /* 0x2C: SVC — FreeRTOS ilk task */
    Default_Handler,               /* 0x30: DebugMon */
    0,                             /* 0x34: Reserved */
    xPortPendSVHandler,            /* 0x38: PendSV — context switch */
    xPortSysTickHandler,           /* 0x3C: SysTick — RTOS tick */
};

/* ----------------------------------------------------------------
 * Reset_Handler — sistem basladiginda ilk calisan fonksiyon
 *
 * Yapilacaklar:
 * 1. .data segmentini Flash'tan RAM'e kopyala
 * 2. .bss segmentini sifirla
 * 3. main()'i cagir
 * ---------------------------------------------------------------- */
void Reset_Handler(void) {

    /* ADIM 1: .data segmentini Flash'tan RAM'e kopyala.
     * Global degiskenler Flash'ta saklanir (program bellegi),
     * ama calisma zamaninda RAM'de olmasi gerekir.
     * Linker script _sidata (kaynak), _sdata/_edata (hedef) verir. */
    uint32_t *src = &_sidata;
    uint32_t *dst = &_sdata;
    while (dst < &_edata) {
        *dst++ = *src++;
    }

    /* ADIM 2: .bss segmentini sifirla.
     * Baslangic degeri olmayan global degiskenler C standardina gore
     * sifir olmali. Donanim bunu garantilemez, biz yapiyoruz. */
    dst = &_sbss;
    while (dst < &_ebss) {
        *dst++ = 0;
    }

    /* ADIM 3: main()'e atla */
    main();

    /* main() donerse (donmemeli) sonsuz dongu */
    while (1) {}
}
