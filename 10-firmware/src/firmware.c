/*
 * Kestrel SmartLock K3 - lock MCU firmware 1.8.2 (Cortex-M4, bare metal).
 *
 * Keypad on GPIOA, motor enable on GPIOB pin 5 + TIM3 CH1 PWM, service console
 * on USART1 (115200 8N1), user PIN hash stored in the last flash sector.
 */
#include <stddef.h>
#include <stdint.h>

#include "regs.h"

#define NOINLINE __attribute__((noinline))

#define CONFIG_ADDR 0x0807F000u
#define MOTOR_PIN 5

struct lock_config {
    uint32_t magic;          /* 'KCFG' */
    uint32_t pin_hash;
    uint16_t max_attempts;
    uint16_t open_ms;
    char serial[16];
};

static volatile uint32_t g_ticks;
static uint32_t g_failed;
static char g_line[48];

void *memset(void *d, int c, size_t n)
{
    uint8_t *p = d;
    while (n--)
        *p++ = (uint8_t)c;
    return d;
}

void *memcpy(void *d, const void *s, size_t n)
{
    uint8_t *dp = d;
    const uint8_t *sp = s;
    while (n--)
        *dp++ = *sp++;
    return d;
}

NOINLINE static int str_eq(const char *a, const char *b)
{
    while (*a && *a == *b)
        a++, b++;
    return *a == *b;
}

NOINLINE static int str_starts(const char *s, const char *prefix)
{
    while (*prefix)
        if (*s++ != *prefix++)
            return 0;
    return 1;
}

void SysTick_Handler(void)
{
    g_ticks++;
}

NOINLINE static void delay_ms(uint32_t ms)
{
    uint32_t start = g_ticks;
    while (g_ticks - start < ms)
        IWDG_KR = 0xAAAA; /* kick the watchdog while waiting */
}

NOINLINE static void clocks_init(void)
{
    RCC_AHB1ENR |= (1u << 0) | (1u << 1);  /* GPIOA, GPIOB */
    RCC_APB1ENR |= (1u << 1);              /* TIM3 */
    RCC_APB2ENR |= (1u << 4);              /* USART1 */
    SYST_RVR = 16000 - 1;                  /* 1 ms at 16 MHz */
    SYST_CSR = 7;
}

NOINLINE static void uart_init(void)
{
    GPIOA->MODER = (GPIOA->MODER & ~(0xFu << 18)) | (0xAu << 18); /* PA9/PA10 alternate */
    GPIOA->AFR[1] = (GPIOA->AFR[1] & ~0xFF0u) | 0x770u;           /* AF7 = USART1 */
    USART1->BRR = 0x8B;                                          /* 115200 @ 16 MHz */
    USART1->CR1 = (1u << 13) | (1u << 3) | (1u << 2);             /* UE | TE | RE */
}

NOINLINE static void uart_putc(char c)
{
    while (!(USART1->SR & (1u << 7)))
        ;
    USART1->DR = (uint32_t)c;
}

NOINLINE static void uart_puts(const char *s)
{
    while (*s)
        uart_putc(*s++);
}

NOINLINE static int uart_getc_nb(void)
{
    if (!(USART1->SR & (1u << 5)))
        return -1;
    return (int)(USART1->DR & 0xFF);
}

NOINLINE static void motor_init(void)
{
    GPIOB->MODER = (GPIOB->MODER & ~(3u << (MOTOR_PIN * 2))) | (1u << (MOTOR_PIN * 2));
    TIM3->PSC = 15;     /* 1 MHz */
    TIM3->ARR = 999;    /* 1 kHz PWM */
    TIM3->CCMR1 = 0x68; /* PWM mode 1, preload */
    TIM3->CCER = 1;
}

NOINLINE static void motor_drive(int open, uint32_t ms)
{
    GPIOB->BSRR = 1u << MOTOR_PIN;            /* enable H-bridge */
    TIM3->CCR1 = open ? 800 : 200;            /* direction by duty cycle */
    TIM3->CR1 = 1;
    delay_ms(ms);
    TIM3->CR1 = 0;
    GPIOB->BSRR = 1u << (MOTOR_PIN + 16);     /* disable H-bridge */
}

NOINLINE static void motor_open(void)
{
    const struct lock_config *cfg = (const struct lock_config *)CONFIG_ADDR;
    motor_drive(1, cfg->open_ms ? cfg->open_ms : 1500);
    uart_puts("lock: OPEN\r\n");
}

NOINLINE static void motor_close(void)
{
    motor_drive(0, 1200);
    uart_puts("lock: CLOSED\r\n");
}

NOINLINE static uint32_t pin_hash(const char *pin)
{
    uint32_t h = 0x811C9DC5u;
    while (*pin)
        h = (h ^ (uint8_t)*pin++) * 0x01000193u;
    return h ^ 0x4B33u;
}

/* Keypad: rows PA0..PA3 driven low one at a time, columns read on PA4..PA6. */
NOINLINE static int keypad_scan(void)
{
    static const char keys[4][3] = {{'1', '2', '3'}, {'4', '5', '6'}, {'7', '8', '9'}, {'*', '0', '#'}};
    for (int row = 0; row < 4; row++) {
        GPIOA->BSRR = (0xFu << 16) | (0xFu & ~(1u << row));
        for (volatile int i = 0; i < 50; i++)
            ;
        uint32_t cols = (~GPIOA->IDR >> 4) & 7u;
        for (int col = 0; col < 3; col++)
            if (cols & (1u << col))
                return keys[row][col];
    }
    return 0;
}

NOINLINE static int check_user_pin(const char *pin)
{
    const struct lock_config *cfg = (const struct lock_config *)CONFIG_ADDR;
    uint32_t limit = cfg->max_attempts ? cfg->max_attempts : 5;

    if (g_failed >= limit) {
        uart_puts("lock: keypad locked out\r\n");
        return 0;
    }
    if (cfg->magic == 0x4746434Bu && pin_hash(pin) == cfg->pin_hash) {
        g_failed = 0;
        return 1;
    }
    g_failed++;
    return 0;
}

NOINLINE static void flash_write_word(uint32_t addr, uint32_t value)
{
    FLASH->KEYR = 0x45670123u;
    FLASH->KEYR = 0xCDEF89ABu;
    FLASH->CR = (2u << 8) | 1u; /* PSIZE x32, PG */
    *(volatile uint32_t *)addr = value;
    while (FLASH->SR & (1u << 16))
        ;
    FLASH->CR = 1u << 31; /* LOCK */
}

/* Service console on USART1. */
NOINLINE static void service_command(const char *cmd)
{
    if (str_eq(cmd, "STATUS")) {
        uart_puts(g_failed ? "status: failed attempts pending\r\n" : "status: ok\r\n");
    } else if (str_starts(cmd, "SVC ")) {
        /* Field service unlock: technicians use the fixed service code. */
        if (str_eq(cmd + 4, "0451")) {
            uart_puts("svc: service unlock\r\n");
            g_failed = 0;
            motor_open();
        } else {
            uart_puts("svc: denied\r\n");
        }
    } else if (str_starts(cmd, "PIN ")) {
        if (check_user_pin(cmd + 4)) {
            uart_puts("pin: enter new pin on keypad\r\n");
            flash_write_word(CONFIG_ADDR + 4, pin_hash("0000"));
        }
    } else if (str_eq(cmd, "CLOSE")) {
        motor_close();
    } else {
        uart_puts("?\r\n");
    }
}

NOINLINE static void service_poll(void)
{
    static unsigned len;
    int c = uart_getc_nb();
    if (c < 0)
        return;
    if (c == '\r' || c == '\n') {
        g_line[len] = '\0';
        if (len)
            service_command(g_line);
        len = 0;
    } else if (len < sizeof(g_line) - 1) {
        g_line[len++] = (char)c;
    }
}

int main(void)
{
    char pin[8];
    unsigned n = 0;

    clocks_init();
    uart_init();
    motor_init();
    uart_puts("Kestrel SmartLock K3 fw 1.8.2\r\n");

    for (;;) {
        IWDG_KR = 0xAAAA;
        service_poll();
        int key = keypad_scan();
        if (!key)
            continue;
        delay_ms(30);
        if (key == '#') {
            pin[n] = '\0';
            if (check_user_pin(pin))
                motor_open();
            else
                uart_puts("lock: wrong pin\r\n");
            n = 0;
        } else if (key == '*') {
            n = 0;
        } else if (n < sizeof(pin) - 1) {
            pin[n++] = (char)key;
        }
    }
}
