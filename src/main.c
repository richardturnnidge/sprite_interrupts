/*
Example of:
  1) loading simple bitmap from memory into a sprite
  2) moving sprite around screen
  3) animating sprite on timer tick
*/

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <agon/vdp.h>
#include <agon/mos.h>
#include <agon/timer.h>

#include <ez80f92.h>
#include "PRT_timer.h"

#include "man.h"
#include "gem.h"

// ---------------------------------------------
// Timer Control Register Bit Definitions
// Only some used in this demo, but others for reference
// ---------------------------------------------

#define TMR_CTL  0x80  				      // base address of timer controls

#define PRT_IRQ_0     0b00000000    // The timer does not reach its end-of-count value. 
                                    // This bit is reset to 0 every time the TMRx_CTL register is read.
#define PRT_IRQ_1     0b10000000    // The timer reaches its end-of-count value. If IRQ_EN is set to 1,
                                    // an interrupt signal is sent to the CPU. This bit remains 1 until 
                                    // the TMRx_CTL register is read.

#define IRQ_EN_0      0b00000000    // Timer interrupt requests are disabled.
#define IRQ_EN_1      0b01000000    // Timer interrupt requests are enabled.

#define PRT_MODE_0    0b00000000    // The timer operates in SINGLE PASS mode. PRT_EN (bit 0) is reset to
                                    //  0,and counting stops when the end-of-count value is reached.
#define PRT_MODE_1    0b00010000    // The timer operates in CONTINUOUS mode. The timer reload value is
                                    // written to the counter when the end-of-count value is reached.

// ; CLK_DIV is a 2-bit mask that sets the timer input source clock divider
#define CLK_DIV_256   0b00001100    // 
#define CLK_DIV_64    0b00001000    // 
#define CLK_DIV_16    0b00000100    //
#define CLK_DIV_4     0b00000000    //

#define RST_EN_0      0b00000000    // The reload and restart function is disabled. 
#define RST_EN_1      0b00000010    // The reload and restart function is enabled. 
                                    // When a 1 is written to this bit,the values in the reload registers
                                    // are loaded into the downcounter when the timer restarts. The 
                                    // programmer must ensure that this bit is set to 1 each time 
                                    // SINGLE-PASS mode is used.

// // disable/enable the programmable reload timer
#define PRT_EN_0   0b00000000       //
#define PRT_EN_1   0b00000001       //

// ---------------------------------------------
// constants
// ---------------------------------------------

const uint8_t screen_mode = 8;
const int delay_ms = 10;
const int man_bitmap_ID = 10;
const int gem_sprite_start_bitmap_ID = 0;
const int man_sprite_ID = 0;
const int gem_sprite_ID = 1;
const int numSprites = 2;
const int gem_frames = 6;
const int gem_freq = 4; // ticks per second update

// ---------------------------------------------
// global variables
// ---------------------------------------------

uint16_t x = 0;
uint16_t y = 0;
int8_t dx = 2;
int8_t dy = 1;

// ---------------------------------------------
// functions used
// ---------------------------------------------

void animate_sprites(void);

static inline void ei(void) {
    __asm__ __volatile__("ei");
}

static inline void di(void) {
    __asm__ __volatile__("di");
}

// silly test
static inline void bell(void) {
    __asm__ __volatile__("ld a, 7");
    __asm__ __volatile__("rst.lil 0x10");
}

// ---------------------------------------------
// main program
// ---------------------------------------------

int main(void) {

    // rest position data
  x = 0;
  y = 0;
  dx = 2;
  dy = 1;

  should_animate = false;
  vdp_adv_clear_buffer(-1);
  vdp_mode(screen_mode);
  vdp_set_pixel_coordinates();
  vdp_cursor_enable(false);
  vdp_set_text_colour( 15 );
  vdp_clear_screen();
  vdp_reset_sprites();
   // set hardware sprites
  vdp_set_variable(2,1);

  //draw some background tringles to illustrate alpha channel
  vdp_set_graphics_fg_colour( 0, GREEN );
  vdp_filled_triangle( 0,175 ,200,230 , 260,30 );
  
  vdp_set_graphics_fg_colour( 0, YELLOW );
  vdp_filled_triangle( 50,30 ,100,220 , 280, 90 );

  vdp_set_graphics_fg_colour( 0, RED );
  vdp_filled_triangle( 120,30 ,90,150 , 300, 170 );

  printf("AgonDev Timer Interrupt driven\n\nSprite Example\n\n");
  printf("Press ESC to exit");

  // load man bitmap and setup sprite
  vdp_select_bitmap(man_bitmap_ID);
  vdp_load_bitmap( 16, 16, man_bitmap );

  vdp_select_sprite(man_sprite_ID);
  vdp_clear_sprite();
  vdp_add_sprite_bitmap(man_bitmap_ID);
  vdp_show_sprite();

  // load bitmaps for animated gem sprite, all in gem.h file
  vdp_select_bitmap(gem_sprite_start_bitmap_ID);
  vdp_load_bitmap( 16, 16, blue1 );
  vdp_select_bitmap(gem_sprite_start_bitmap_ID + 1);
  vdp_load_bitmap( 16, 16, blue2 );
  vdp_select_bitmap(gem_sprite_start_bitmap_ID + 2);
  vdp_load_bitmap( 16, 16, blue3 );
  vdp_select_bitmap(gem_sprite_start_bitmap_ID + 3);
  vdp_load_bitmap( 16, 16, blue4 );
  vdp_select_bitmap(gem_sprite_start_bitmap_ID + 4);
  vdp_load_bitmap( 16, 16, blue5 );
  vdp_select_bitmap(gem_sprite_start_bitmap_ID + 5);
  vdp_load_bitmap( 16, 16, blue6 );

  // create sprite with 6 frames of loaded bitmaps
  vdp_select_sprite(gem_sprite_ID);
  vdp_clear_sprite();
  // create a sprite with 6 images in a sequence
  vdp_create_sprite( gem_sprite_ID, gem_sprite_start_bitmap_ID, 6 );
  // plot sprite on screen at x,y position
  vdp_move_sprite_to( 150,110 );
  vdp_show_sprite();

  // activate the sprites
  vdp_activate_sprites(numSprites);

  // setup all the timer features

  // set PRT frequency
  uint16_t timerDefault = 0xffff / gem_freq;
  uint8_t timerL = timerDefault % 256;
  uint8_t timerH = timerDefault / 256;
  
  IO(TMR1_RR_L) = timerL; // low byte of timer
  IO(TMR1_RR_H) = timerH; // high byte of timer. Bigger = longer delay

  // set the ISR function 	
  mos_setintvector(0x0C, timer_handler_1 );

	// ; enable timer, with interrupt and CONTINUOUS mode, clock divider 256
  IO(TMR1_CTL) = PRT_IRQ_0 | IRQ_EN_1 | PRT_MODE_1 | CLK_DIV_256 | RST_EN_1 | PRT_EN_1; 


  // main loop - just move character sprite around until ESC key pressed
  while(true) {
    delay(delay_ms);
    di();
      vdp_select_sprite(man_sprite_ID);
      vdp_move_sprite_to( x, y );
      vdp_refresh_sprites();
    ei();

    x += dx;
    if (x>300) dx = -1;
    if (x<1) dx = 1;
    y += dy;
    if (y>220) dy = -1;
    if (y<1) dy = 1;
   
    // example for tick version of timer1
    // if(should_animate){
    //   animate_sprites();
    //   should_animate = false;
    // }
    
    if(vdp_getKeyCode() == 27) break;
  }

    bell();

   // stop intterrupt timer
  IO(TMR1_CTL) = PRT_IRQ_0 | IRQ_EN_0 | PRT_MODE_0 | CLK_DIV_256 | RST_EN_1 | PRT_EN_0; 

  vdp_clear_screen();
  vdp_cursor_enable(true);
  vdp_adv_clear_buffer(-1);
  vdp_reset_sprites();

  // set software sprites
  vdp_set_variable(2,0);
  return 0; 
}


void animate_sprites(void){   
  vdp_select_sprite(gem_sprite_ID);
  vdp_next_sprite_frame();
  vdp_refresh_sprites();
}