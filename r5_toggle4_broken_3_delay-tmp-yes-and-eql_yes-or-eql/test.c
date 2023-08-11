#include <stdint.h>
#include "ipc_rpmsg_linux_resource_table.h"
#include <string.h>
#include <stdio.h>
#include "r5/kernel/dpl/CacheP.h"
#include "r5/kernel/dpl/MpuP_armv7.h"
#include <stdlib.h>

#include "r5/kernel/dpl/CycleCounterP.h"

// global structures used by MPU and cache init code
CacheP_Config gCacheConfig = {1, 0}; // cache on, no forced writethrough
MpuP_Config gMpuConfig = {4, 1, 1}; // 3 regions, background region on, MPU on
MpuP_RegionConfig gMpuRegionConfig[4] = {
    // Complete 32-bit address space
    {
	.baseAddr = 0x0u,
        .size = MpuP_RegionSize_4G,
	.attrs = {
            .isEnable = 1,
            .isCacheable = 0,
            .isBufferable = 0,
            .isSharable = 0,
            .isExecuteNever = 0,
            .tex = 1,
            .accessPerm = MpuP_AP_ALL_RW,
            .subregionDisableMask = 0x0u},
    },
    // MSRAM region
    {
        .baseAddr = 0x70000000u,
        .size = MpuP_RegionSize_8M,
        .attrs = {
            .isEnable = 1,
            .isCacheable = 1,
            .isBufferable = 1,
            .isSharable = 0,
            .isExecuteNever = 0,
            .tex = 7,
            .accessPerm = MpuP_AP_ALL_RW,
            .subregionDisableMask = 0x0u},
    },
    // DDR region
    {
        .baseAddr = 0x80000000u,
        .size = MpuP_RegionSize_2G,
        .attrs = {.isEnable = 1, .isCacheable = 1, .isBufferable = 1, .isSharable = 0, .isExecuteNever = 0, .tex = 7, .accessPerm = MpuP_AP_ALL_RW, .subregionDisableMask = 0x0u},
    },
    // rpmsg region
    {
        .baseAddr = 0xA1000000u,
        .size = MpuP_RegionSize_1M,
        .attrs = {.isEnable = 1, .isCacheable = 0, .isBufferable = 0, .isSharable = 1, .isExecuteNever = 1, .tex = 1, .accessPerm = MpuP_AP_ALL_RW, .subregionDisableMask = 0x0u},
    },
};

// NOTE: R5FSS defaults to ARM at reset so these must all be ARM instead of Thumb

void Reset_Handler(void) __attribute__((naked, section(".boot.reset"), target("arm")));
void Default_Handler(void) __attribute__((naked, section(".boot.handler"), target("arm")));

void Undef_Handler(void) __attribute__((weak, alias("Default_Handler")));
void SVC_Handler(void) __attribute__((weak, alias("Default_Handler")));
void PAbt_Handler(void) __attribute__((weak, alias("Default_Handler")));
void DAbt_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ_Handler(void) __attribute__((weak, alias("Default_Handler")));
void FIQ_Handler(void) __attribute__((weak, alias("Default_Handler")));

__attribute__((naked, section(".vector"), target("arm"))) void vectors()
{
    asm volatile(
        "LDR    PC, =Reset_Handler                        \n"
        "LDR    PC, =Undef_Handler                        \n"
        "LDR    PC, =SVC_Handler                          \n"
        "LDR    PC, =PAbt_Handler                         \n"
        "LDR    PC, =DAbt_Handler                         \n"
        "NOP                                              \n"
        "LDR    PC, =IRQ_Handler                          \n"
        "LDR    PC, =FIQ_Handler                          \n");
}

// newlib startup code
extern void _start();

void Reset_Handler()
{
    asm volatile(
        // initialize stack
        "ldr sp, =__stack \n"

        // disable interrupts
        "mrs r0, cpsr \n"
        "orr r0, r0, #0xc0 \n"
        "msr cpsr_cf, r0 \n"

        // enable FPU
        "mrc p15,#0x0,r0,c1,c0,#2 \n"
        "mov r3,#0xf00000 \n"
        "orr r0,r0,r3 \n"
        "mcr p15,#0x0,r0,c1,c0,#2 \n"
        "mov r0,#0x40000000 \n"
        "fmxr fpexc,r0 \n");

    // must initialize MPU if code is on external memory
    MpuP_init();
    CacheP_init();

    _start();
}

void Default_Handler()
{
    while (1)
        ;
}

__attribute__((section(".log_shared_mem"))) char gDebugMemLog[DebugP_MEM_LOG_SIZE];

RPMessage_ResourceTable gRPMessage_linuxResourceTable __attribute__((section(".resource_table"), aligned(4096))) =
    {
        {
            1U, /* we're the first version that implements this */
            2U, /* number of entries, MUST be 2 */
            {
                0U,
                0U,
            } /* reserved, must be zero */
        },
        /* offsets to the entries */
        {
            offsetof(RPMessage_ResourceTable, vdev),
            offsetof(RPMessage_ResourceTable, trace),
        },
        /* vdev entry */
        {
            RPMESSAGE_RSC_TYPE_VDEV,
            RPMESSAGE_RSC_VIRTIO_ID_RPMSG,
            0U,
            1U,
            0U,
            0U,
            0U,
            2U,
            {0U, 0U},
        },
        /* the two vrings */
        {RPMESSAGE_RSC_VRING_ADDR_ANY, 4096U, 256U, 1U, 0U},
        {RPMESSAGE_RSC_VRING_ADDR_ANY, 4096U, 256U, 2U, 0U},
        {
            (RPMESSAGE_RSC_TRACE_INTS_VER0 | RPMESSAGE_RSC_TYPE_TRACE),
            (uint32_t)gDebugMemLog,
            DebugP_MEM_LOG_SIZE,
            0,
            "trace:r5fss_test",
        },
};

// retarget stdout to remoteproc trace buffer
int _write(int handle, char *data, int size)
{
    static int idx = 0;
    int count;

    for (count = 0; count < size; count++)
    {
        if (idx > DebugP_MEM_LOG_SIZE)
            idx = 0;

        gDebugMemLog[idx++] = data[count];
    }
    CacheP_wb(gDebugMemLog, DebugP_MEM_LOG_SIZE, CacheP_TYPE_L1D);

    return count;
}

// DELAY
// 7=5.176ns
// 44=1.0us
// 50=1.136us
// 500=11.03us 
// 5000=110us
// 50000 = 1.1ms
// 500000 = 11ms period

int runToggle ()
{
        #define DELAY 3000000
	
	volatile uint64_t* pDir = (uint64_t *) 0x00600060;
        volatile uint64_t* pSet = (uint64_t *) 0x00600068;
        volatile uint64_t* pClr = (uint64_t *) 0x0060006C;

	//Configure GPIO0_93 pin 9_14 as output

	///////////////////////////////////////////////
	volatile uint64_t tmp = *pDir;
        tmp &= 0xDFFFFFFF;
	for(volatile int i = 0; i < 50; i++);
        *pDir = tmp;
	///////////////////////////////////////////////

        printf ("\n");

        printf ("r5_toggle (Language: C)\n");
        printf ("\n");
  
        printf ("started\n");

        for(int i = 0; i < 10000; i++)
        {
                //set the output high
                *pSet |= 0x20000000;
                for(volatile int j=0; j< DELAY; j++);
                //set the output low
                *pClr |= 0x20000000;
                for(volatile int j=0; j< DELAY; j++);
        }
        printf ("stopped\n");

	return 0;
}

int main()
{
    CycleCounterP_reset();
    runToggle();
    return 0;
}


