/* main.c — FreeRTOS QEMU Demo
 *
 * Bu proje uc temel FreeRTOS kavramini gosteriyor:
 * 1. Task olusturma ve scheduler
 * 2. Queue ile task-arasi veri aktarimi (producer-consumer)
 * 3. Binary semaphore ile senkronizasyon
 *
 * Calisma ortami: ARM Cortex-M3, QEMU lm3s6965evb
 * Cikti: UART0 uzerinden seri port (QEMU -nographic ile gorulur)
 */

#include <stdint.h>
#include <string.h>

/* FreeRTOS kernel header'lari */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

/* ----------------------------------------------------------------
 * lm3s6965evb UART0 Register Adresleri
 *
 * Gercek donanim register'larina dogrudan yaziyoruz —
 * bu bare-metal programlamanin ozidir.
 * QEMU bu register'lari emule ediyor, biz yazinca
 * terminal ekraninda gozukuyor.
 * ---------------------------------------------------------------- */
#define UART0_BASE      0x4000C000
#define UART0_DR        (*((volatile uint32_t *)(UART0_BASE + 0x000)))
#define UART0_FR        (*((volatile uint32_t *)(UART0_BASE + 0x018)))
#define UART0_FR_TXFF   (1 << 5)   /* TX FIFO dolu mu? */

/* UART0'a tek karakter yaz */
static void uart_putc(char c) {
    while (UART0_FR & UART0_FR_TXFF) {}   /* TX hazir olana kadar bekle */
    UART0_DR = c;
}

/* UART0'a string yaz */
static void uart_puts(const char *s) {
    while (*s) {
        uart_putc(*s++);
    }
}

/* Sayiyi string'e cevir ve UART'a yaz (basit itoa) */
static void uart_put_uint32(uint32_t n) {
    char buf[12];
    int i = 0;
    if (n == 0) { uart_putc('0'); return; }
    while (n > 0) {
        buf[i++] = '0' + (n % 10);
        n /= 10;
    }
    /* Tersten yaz */
    for (int j = i - 1; j >= 0; j--) {
        uart_putc(buf[j]);
    }
}

/* ----------------------------------------------------------------
 * Queue ve Semaphore handle'lari
 *
 * Handle: FreeRTOS nesnelerine referans.
 * Queue ve semaphore olusturuldugunda bu degiskenlere atanir,
 * task'lar bu handle'lar uzerinden iletisir.
 * ---------------------------------------------------------------- */
static QueueHandle_t     xSensorQueue    = NULL;
static SemaphoreHandle_t xPrintSemaphore = NULL;

/* ----------------------------------------------------------------
 * UART cikisini korumak icin yardimci makrolar.
 * Birden fazla task ayni anda UART'a yazarsa cikti karisir.
 * Semaphore ile "bir seferde yalnizca bir task yazsin" sagliyoruz.
 * ---------------------------------------------------------------- */
#define PRINT_BEGIN()   xSemaphoreTake(xPrintSemaphore, portMAX_DELAY)
#define PRINT_END()     xSemaphoreGive(xPrintSemaphore)

/* ----------------------------------------------------------------
 * Sensor verisi yapisi — Queue ile gonderilecek veri
 *
 * Gercek bir sistemde IMU, barometri, GPS verisi olurdu.
 * Biz simule ediyoruz.
 * ---------------------------------------------------------------- */
typedef struct {
    uint32_t timestamp_ms;    /* kac ms gecti */
    int32_t  temperature;     /* sicaklik (x10, yani 235 = 23.5 C) */
    uint32_t pressure;        /* basinc (Pa) */
} SensorData_t;

/* ----------------------------------------------------------------
 * TASK 1: Sensor Producer
 *
 * Gercek bir sistemde bu task IMU veya barometreden okurdu.
 * Biz her 500ms'de sahte veri uretip queue'ya koyuyoruz.
 *
 * Oncelik: 2 (orta)
 * ---------------------------------------------------------------- */
static void vSensorTask(void *pvParameters) {
    (void)pvParameters;

    SensorData_t data;
    uint32_t tick_count = 0;

    PRINT_BEGIN();
    uart_puts("[SENSOR] Task basladi. Her 500ms veri uretecek.\r\n");
    PRINT_END();

    for (;;) {
        /* Sahte sensor verisi uret */
        data.timestamp_ms = xTaskGetTickCount();   /* FreeRTOS tick sayaci */
        data.temperature  = 235 + (tick_count % 10);  /* 23.5 - 24.4 C arasi */
        data.pressure     = 101325 + (tick_count * 7); /* basinc simulasyonu */
        tick_count++;

        /* Queue'ya veri ekle.
         * portMAX_DELAY: queue dolu olsa bile sonsuza kadar bekle.
         * Blog: "producer veri koyar, consumer daha sonra alir" */
        if (xQueueSend(xSensorQueue, &data, portMAX_DELAY) == pdPASS) {
            PRINT_BEGIN();
            uart_puts("[SENSOR] Veri uretildi -> Sicaklik: ");
            uart_put_uint32(data.temperature);
            uart_puts(" | Basinc: ");
            uart_put_uint32(data.pressure);
            uart_puts("\r\n");
            PRINT_END();
        }

        /* 500ms bekle — bu arada diger task'lar CPU'yu kullanabilir */
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

/* ----------------------------------------------------------------
 * TASK 2: Data Processor (Consumer)
 *
 * Queue'dan veri alip isleme yapan task.
 * Gercek sistemde: PID hesabi, filtreleme, karar verme.
 * Biz basit bir analiz yapiyoruz.
 *
 * Oncelik: 3 (yuksek — isleme oncelikli)
 * ---------------------------------------------------------------- */
static void vProcessorTask(void *pvParameters) {
    (void)pvParameters;

    SensorData_t received;
    uint32_t packet_count = 0;

    PRINT_BEGIN();
    uart_puts("[PROC]   Task basladi. Queue'dan veri bekliyor.\r\n");
    PRINT_END();

    for (;;) {
        /* Queue'dan veri al.
         * Queue bos olursa task BLOCKED olur — CPU'yu tüketmez.
         * Blog: "queue bossa consumer task blocked olur,
         *        yeni veri gelince otomatik uyanir" */
        if (xQueueReceive(xSensorQueue, &received, portMAX_DELAY) == pdPASS) {
            packet_count++;

            PRINT_BEGIN();
            uart_puts("[PROC]   Paket #");
            uart_put_uint32(packet_count);
            uart_puts(" islendi. Timestamp: ");
            uart_put_uint32(received.timestamp_ms);
            uart_puts("ms");

            /* Basit anomali tespiti: sicaklik 240 ustu? */
            if (received.temperature > 240) {
                uart_puts(" [!] YUKSEK SICAKLIK UYARISI");
            }
            uart_puts("\r\n");
            PRINT_END();
        }
    }
}

/* ----------------------------------------------------------------
 * TASK 3: Heartbeat
 *
 * Sistemin yasadigini gosteren periyodik sinyal.
 * Gercek sistemde bu task'in durması = sistem arizasi.
 * MAVLink fuzzer projesinde HEARTBEAT timeout tespiti
 * tam olarak bu mantiga dayaniyordu.
 *
 * Oncelik: 1 (dusuk — sadece sistem bosta iken calisir)
 * ---------------------------------------------------------------- */
static void vHeartbeatTask(void *pvParameters) {
    (void)pvParameters;

    uint32_t beat = 0;

    PRINT_BEGIN();
    uart_puts("[HEART]  Task basladi. Her 1sn heartbeat gonderecek.\r\n");
    PRINT_END();

    for (;;) {
        beat++;

        PRINT_BEGIN();
        uart_puts("[HEART]  Beat #");
        uart_put_uint32(beat);
        uart_puts(" | Bos heap: ");
        uart_put_uint32(xPortGetFreeHeapSize());
        uart_puts(" byte\r\n");
        PRINT_END();

        /* 1000ms = 1 saniye bekle */
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* ----------------------------------------------------------------
 * Stack Overflow Hook
 *
 * FreeRTOSConfig.h'da configCHECK_FOR_STACK_OVERFLOW=2 ayarladik.
 * Bir task'in stack'i tasarsa FreeRTOS bu fonksiyonu cagirir.
 * Blog: "MMU olmayan sistemlerde stack overflow kritik"
 * ---------------------------------------------------------------- */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    (void)xTask;
    uart_puts("\r\n[FATAL] STACK OVERFLOW: ");
    uart_puts(pcTaskName);
    uart_puts("\r\n");
    while (1) {}
}

/* ----------------------------------------------------------------
 * Malloc Fail Hook
 *
 * pvPortMalloc() bellek bulamazsa bu cagirilir.
 * ---------------------------------------------------------------- */
void vApplicationMallocFailedHook(void) {
    uart_puts("\r\n[FATAL] MALLOC FAILED\r\n");
    while (1) {}
}

/* ----------------------------------------------------------------
 * main() — Her seyi burada baslat, sonra scheduler'a birak
 * ---------------------------------------------------------------- */
int main(void) {

    /* Baslangic mesaji */
    uart_puts("\r\n========================================\r\n");
    uart_puts("FreeRTOS QEMU Demo\r\n");
    uart_puts("ARM Cortex-M3 | lm3s6965evb\r\n");
    uart_puts("3 Task: Sensor + Processor + Heartbeat\r\n");
    uart_puts("========================================\r\n\r\n");

    /* Queue olustur: en fazla 5 SensorData_t siginir.
     * Blog: "producer koyar, consumer alir, ikisi ayni anda
     *        orada olmak zorunda degil" */
    xSensorQueue = xQueueCreate(5, sizeof(SensorData_t));
    if (xSensorQueue == NULL) {
        uart_puts("[FATAL] Queue olusturulamadi!\r\n");
        while (1) {}
    }

    /* Binary semaphore olustur: UART erisimini korumak icin.
     * Blog: "binary semaphore iki durum — verildi ya da verilmedi" */
    xPrintSemaphore = xSemaphoreCreateBinary();
    if (xPrintSemaphore == NULL) {
        uart_puts("[FATAL] Semaphore olusturulamadi!\r\n");
        while (1) {}
    }
    /* Semaphore'u bos olusturuluyor, once bir Give yapiyoruz
     * ki ilk Take calissın */
    xSemaphoreGive(xPrintSemaphore);

    /* Task'lari olustur.
     * xTaskCreate parametreleri:
     *   1. Fonksiyon pointer
     *   2. Isim (debug icin)
     *   3. Stack boyutu (word cinsinden)
     *   4. Parametre (NULL = yok)
     *   5. Oncelik (yuksek sayi = yuksek oncelik)
     *   6. Handle (NULL = istemiyoruz) */
    xTaskCreate(vSensorTask,    "Sensor",    256, NULL, 2, NULL);
    xTaskCreate(vProcessorTask, "Processor", 256, NULL, 3, NULL);
    xTaskCreate(vHeartbeatTask, "Heartbeat", 256, NULL, 1, NULL);

    uart_puts("[MAIN]   Task'lar olusturuldu. Scheduler basliyor...\r\n\r\n");

    /* Scheduler'i baslat — artik main() geri donmez.
     * Blog: "SVC exception ile ilk task baslatilir" */
    vTaskStartScheduler();

    /* Buraya hic ulasilmamali */
    uart_puts("[FATAL] Scheduler durdu!\r\n");
    while (1) {}

    return 0;
}
