//  Sord M5 ROM Cart emulator 'NiseM5Cart'
//
//  MAX 20K ROM x 64 Bank
//  TMS9918 Emulation for VGA OUT

//  Connection (see schematics for detail)
//  ----- M5 cart slot
//  GP0-7:  D0-7
//  GP8-23: A0-15
//  GP25: RESET (w/Switch)
//  GP26: IORD
//  GP27: IOWR
//  GP28: MERD
//  GP29: MEWR
//  ----- VGA OUT
//  GP30: HSYNC
//  GP31: VSYNC
//  GP32: Blue0
//  GP33: Blue1
//  GP34: Blue2
//  GP35: Red0
//  GP36: Red1
//  GP37: Red2
//  GP38: Green0
//  GP39: Green1
//  -----  Switch & LEDs
//  GP40:  SW1
//  GP41:  SW2
//  GP42:  LED0
//  GP43:  LED1
//  GP44:  LED2
//  GP45:  LED3
//  GP46:  LED4
//  GP47:  LED5

#define FLASH_INTERVAL 300      // 5sec

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/sync.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/timer.h"
#include "hardware/uart.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "hardware/vreg.h"

#include "vga16_graphics.h"
#include "tms9918/vrEmuTms9918.h"
#include "tms9918/vrEmuTms9918Util.h"

// ROM configuration
// WeACT RP2350B with 16MiB Flash
// Data for 1R12    32KiB * 64 = 2MiB @ 0x10080000

#define ROMBASE 0x10080000u

uint8_t *cartrom=(uint8_t *)(ROMBASE);
//uint8_t *fdrom=(uint8_t *)(ROMBASE-0x10000);

// RAM configuration
// cart: 32KiB
// RAM:  32KiB

uint8_t cart[0x8000];
uint8_t exram[0x8000];

//uint8_t fdrom_copy[0x2000];
//uint8_t fdram[0x8000];

#define MAXROMPAGE 64

volatile uint8_t rompage=0;
volatile uint32_t flash_command=0;

#define SWHOLD 100

// VDP
VrEmuTms9918 *mainscreen;
uint8_t scandata[256];
extern unsigned char vga_data_array[];
volatile uint32_t video_hsync,video_vsync,scanline,vsync_scanline;

uint8_t colors[]={
    0b00000000  ,  // #000000 Black (Transparent)
    0b00000000  ,  // #000000 Black 
    0b10100000  ,  // #00AC00 medium green 
    0b01110010  ,  // #24DB55 light green
    0b10010011  ,  // #2424FF dark blue
    0b01001011  ,  // #4949FF light blue
    0b10010100  ,  // #AC2400 dark red 
    0b01110011  ,  // #24DBFF cyan
    0b10011100  ,  // #FF2400 medium red
    0b11011110  ,  // #FF6D55 light red
    0b10110100  ,  // #ACAC00 dark yellow
    0b10101100  ,  // #DBAC55 light yellow
    0b00100000  ,  // #009200 dark green
    0b10010101  ,  // #AC24AA magenta
    0b10110111  ,  // #ACACAA gray
    0b11111111     // #FFFFFF White
};

// *REAL* H-Sync for emulation
void __not_in_flash_func(hsync_handler)(void) {

    uint32_t vramindex;
    uint32_t tmsscan;
    uint8_t bgcolor;

    pio_interrupt_clear(pio0, 0);

    if((scanline!=0)&&(gpio_get(31)==0)) { // VSYNC
        scanline=0;
        video_vsync=1;
    } else {
        scanline++;
    }

    if((scanline%2)==0) {
        video_hsync=1;

        // VDP Draw on HSYNC

        // VGA Active starts scanline 35
        // TMS9918 Active scanline 75(0) to 474(199)

        if(scanline==78) {
//            if(menumode==0) {
                bgcolor=vrEmuTms9918RegValue(mainscreen,TMS_REG_FG_BG_COLOR) & 0x0f;
//            } else {
//                bgcolor=vrEmuTms9918RegValue(menuscreen,TMS_REG_FG_BG_COLOR) & 0x0f;
//            }
            memset(vga_data_array+320*4,colors[bgcolor],320);
        }

//        if((scanline>=75)&&(scanline<=456)) {
        if((scanline>=81)&&(scanline<=464)) {

            tmsscan=(scanline-81)/2;
//            if(menumode==0) {
                vrEmuTms9918ScanLine(mainscreen,tmsscan,scandata);
                bgcolor=vrEmuTms9918RegValue(mainscreen,TMS_REG_FG_BG_COLOR) & 0x0f;
//            } else {
//                vrEmuTms9918ScanLine(menuscreen,tmsscan,scandata);
//                bgcolor=vrEmuTms9918RegValue(menuscreen,TMS_REG_FG_BG_COLOR) & 0x0f;
//            }
            vramindex=(tmsscan%4)*320;

            memset(vga_data_array+(tmsscan%4)*320,colors[bgcolor],32);
            memset(vga_data_array+(tmsscan%4)*320+32+256,colors[bgcolor],32);

            for(int j=0;j<256;j++) {
                vga_data_array[vramindex+j+32]=colors[scandata[j]];
            }           
        }

    }

    return;

}

//  reset

void __not_in_flash_func(z80reset)(uint gpio,uint32_t event) {

    gpio_acknowledge_irq(25,GPIO_IRQ_EDGE_FALL);
    vrEmuTms9918Reset(mainscreen);

    memcpy(cart,cartrom+(rompage*0x8000),0x5000);

    return;
}

static inline void io_write(uint16_t address, uint8_t data)
{

    uint8_t b;

    switch(address&0xff) {

        case 0x11:  // VDP Address
            vrEmuTms9918WriteAddr(mainscreen,data);
            return; 

        case 0x10: // VDP Data
            vrEmuTms9918WriteData(mainscreen,data);
            return;

        default:
            return;

    }
 
    return;

}

void init_emulator(void) {


    memcpy(cart,cartrom,0x5000); // Load first slot (may be BASIC-?)

}

// Main thread (Core1)

void __not_in_flash_func(main_core1)(void) {

    volatile uint32_t bus;

    uint32_t control,address,data,response;
    uint32_t needwait=0;

    gpio_init_mask(0x3fffffff);
    gpio_set_dir_all_bits(0x00000000);  // All pins are INPUT

    while(1) {

        bus=gpio_get_all();

        control=bus&0x3c000000;

        if(control==0x2c000000) {
            // Memory READ

            address=(bus&0xffff00)>>8;
            if (address<0x2000) {
                response=0;
            } else if (address<0x7000) {

                data=cart[address-0x2000];
                response=1;
            } else if (address >= 0x8000) {
                data=exram[address&0x7fff];
                response=1;
            } else {
                response=0;
            }

            if(response) {

                // Set GP0-7 to OUTPUT
                gpio_set_dir_masked(0xff,0xff);
                gpio_put_masked(0xff,data);

                // Wait while RD# is low

                control=0;

                while(control==0) {
                    bus=gpio_get_all();
                    control=bus&0x10000000;
                }

                // Set GP0-7 to INPUT
                gpio_set_dir_masked(0xff,0x00);

            } else {
                // Wait while RD# is low
                control=0;

                while(control==0) {
                    bus=gpio_get_all();
                    control=bus&0x10000000;
                }

            }

        } else if (control==0x1c000000) {
            // Memory Write
            address=(bus&0xffff00)>>8;
            data=bus&0xff;

            if(address>=0x8000) {
                exram[address&0x7fff]=data;
            }

            control=0;

            // Wait while WR# is low
            while(control==0) {
                bus=gpio_get_all();
                control=bus&0x20000000;
            }

        } else if(control==0x38000000) {
            // IO READ
            address=(bus&0xffff00)>>8;

            switch(address&0xff) {

                // VDP
                case 0x10:
                    data=vrEmuTms9918ReadData(mainscreen);
                    break;

                case 0x11:
                    data=vrEmuTms9918ReadStatus(mainscreen);
                    break;

                // PI-5
//                case 0x70:
//                    break;

                default:
                    response=0;

            }

            // DO NOTHING

#if 0
            if(response) {

                // Set GP0-7 to OUTPUT
                gpio_set_dir_masked(0xff,0xff);
                gpio_put_masked(0xff,data);

                // Wait while RD# is low

                control=0;

                while(control==0) {
                    bus=gpio_get_all();
                    control=bus&0x2000000;
                }

                // Set GP0-7 to INPUT

                gpio_set_dir_masked(0xff,0x00);

            } else {
#endif
                // Wait while RD# is low
                control=0;

                while(control==0) {
                    bus=gpio_get_all();
                    control=bus&0x4000000;
                }

#if 0
            }
#endif
            continue;

        } else if(control==0x34000000) {

            // IO Write
            address=(bus&0xffff00)>>8;
            data=bus&0xff;

            io_write(address,data);

            control=0;

            // Wait while WR# is low
            while(control==0) {
                bus=gpio_get_all();
                control=bus&0x8000000;
            }
            continue;
        }
    }
}

int main() {

    uint32_t menuprint=0;
    uint32_t filelist=0;
    uint32_t subcpu_wait;
    uint32_t rampacno;
    uint32_t pacpage;

    uint8_t swpin1,swpin2,lastswpin1,lastswpin2,holdswpin1,holdswpin2;

    static uint32_t hsync_wait,vsync_wait;

    vreg_set_voltage(VREG_VOLTAGE_1_20);  // for overclock to 300MHz
    set_sys_clock_khz(300000 ,true);

    initVGA();

    mainscreen=vrEmuTms9918New();

    irq_set_exclusive_handler (PIO0_IRQ_0, hsync_handler);
    irq_set_enabled(PIO0_IRQ_0, true);
    pio_set_irq0_source_enabled (pio0, pis_interrupt0 , true);

    init_emulator();

    multicore_launch_core1(main_core1);

    // Init Switch & LEDs

    gpio_init(40);
    gpio_init(41);
    gpio_init(42);
    gpio_init(43);
    gpio_init(44);
    gpio_init(45);
    gpio_init(46);
    gpio_init(47);

    gpio_set_dir(40,0);
    gpio_set_dir(41,0);
    gpio_set_dir(42,1);
    gpio_set_dir(43,1);
    gpio_set_dir(44,1);
    gpio_set_dir(45,1);
    gpio_set_dir(46,1);
    gpio_set_dir(47,1);

    gpio_set_pulls(40,1,0); // PULL UP
    gpio_set_pulls(41,1,0);

    lastswpin1=lastswpin2=1;

    // Set RESET# interrupt

    gpio_set_irq_enabled_with_callback(25,GPIO_IRQ_EDGE_FALL,true,z80reset);

    while(1) {

        // Check Switch condition
        // Effect by RESET

        swpin1=gpio_get(40);
        if(swpin1!=lastswpin1) {
            holdswpin1++;
            if(holdswpin1>SWHOLD) {
                if(swpin1==0) {
                    if(rompage>0) {
                        rompage--;
                    } else {
                        rompage=MAXROMPAGE-1;
                    }
                }
                lastswpin1=swpin1;
            }
        } else {
            holdswpin1=0;
        }

        swpin2=gpio_get(41);
        if(swpin2!=lastswpin2) {
            holdswpin2++;
            if(holdswpin2>SWHOLD) {
                if(swpin2==0) {
                    if(rompage<MAXROMPAGE-2) {
                        rompage++;
                    } else {
                        rompage=0;
                    }
                }
                lastswpin2=swpin2;
            }
        } else {
            holdswpin2=0;
        }     

        gpio_put_masked64(0xfc0000000000,((uint64_t)rompage)<<42);

    }
}

