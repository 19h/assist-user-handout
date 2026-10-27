#ifndef REGS_H
#define REGS_H
#include <stdint.h>

/* STM32F4-compatible register map (the Kestrel lock MCU is an F401 clone). */
#define REG32(addr) (*(volatile uint32_t *)(addr))

#define RCC_BASE    0x40023800u
#define RCC_AHB1ENR REG32(RCC_BASE + 0x30)
#define RCC_APB1ENR REG32(RCC_BASE + 0x40)
#define RCC_APB2ENR REG32(RCC_BASE + 0x44)

typedef struct {
    volatile uint32_t MODER, OTYPER, OSPEEDR, PUPDR, IDR, ODR, BSRR, LCKR, AFR[2];
} gpio_t;
#define GPIOA ((gpio_t *)0x40020000u)
#define GPIOB ((gpio_t *)0x40020400u)

typedef struct {
    volatile uint32_t SR, DR, BRR, CR1, CR2, CR3, GTPR;
} usart_t;
#define USART1 ((usart_t *)0x40011000u)

typedef struct {
    volatile uint32_t CR1, CR2, SMCR, DIER, SR, EGR, CCMR1, CCMR2, CCER, CNT, PSC, ARR, RCR, CCR1, CCR2, CCR3, CCR4;
} tim_t;
#define TIM3 ((tim_t *)0x40000400u)

typedef struct {
    volatile uint32_t ACR, KEYR, OPTKEYR, SR, CR, OPTCR;
} flash_t;
#define FLASH ((flash_t *)0x40023C00u)

#define IWDG_KR  REG32(0x40003000u)
#define SYST_CSR REG32(0xE000E010u)
#define SYST_RVR REG32(0xE000E014u)
#define NVIC_ISER1 REG32(0xE000E104u)

#endif
