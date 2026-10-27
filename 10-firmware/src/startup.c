/* Vector table and reset handler. */
#include <stdint.h>

extern uint32_t _sidata, _sdata, _edata, _sbss, _ebss, _estack;
int main(void);
void SysTick_Handler(void);

void Default_Handler(void)
{
    for (;;)
        ;
}

void HardFault_Handler(void)
{
    for (;;)
        ;
}

void Reset_Handler(void)
{
    uint32_t *src = &_sidata, *dst = &_sdata;
    while (dst < &_edata)
        *dst++ = *src++;
    for (dst = &_sbss; dst < &_ebss;)
        *dst++ = 0;
    main();
    for (;;)
        ;
}

typedef void (*vector_fn)(void);

__attribute__((section(".isr_vector"), used)) const vector_fn g_vectors[16 + 54] = {
    (vector_fn)&_estack,
    Reset_Handler,
    Default_Handler,   /* NMI */
    HardFault_Handler, /* HardFault */
    Default_Handler, Default_Handler, Default_Handler, 0, 0, 0, 0,
    Default_Handler,   /* SVCall */
    Default_Handler, 0,
    Default_Handler,   /* PendSV */
    SysTick_Handler,
    [16 ... 69] = Default_Handler,
};
