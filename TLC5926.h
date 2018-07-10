#ifndef TLC5926_h
#define TLC5926_h

// #include <inttypes.h>
#include <Arduino.h>

class TLC5926 {
    private:
         int SDI;
         int CLK;
         int LE;
         int iOE;
         int SDO; // error-data-return
         int ct;
         boolean debugging;
         boolean pwm;

         TLC5926* debug_prefix();
         void debug_print(const char * msg);
         void do_clk_ioe_le(const int state_list[][3]);
         void switch_mode(const int pattern[][3]);
        
    public:
         int SDI_pin();
         int CLK_pin();
         int LE_pin();
         int iOE_pin();
         int SDO_pin();

        TLC5926();
        // Everybody returns self for chaining, because.
        TLC5926* debug(boolean v);
        // FIXME: move chain_ct out -- who uses it?
        TLC5926* attach(int chained_ct, int sdi_pin, int clk_pin, int le_pin, int ioe_pin, int sdo_pin = -1);
        TLC5926* attach(int sdi_pin, int clk_pin, int le_pin, int ioe_pin);
        TLC5926* attach(int chained_ct, int sdi_pin, int clk_pin);
        TLC5926* attach(int sdi_pin, int clk_pin);
        TLC5926* latch_pulse();
        TLC5926* reset();
        TLC5926* normal_mode();
        unsigned int error_detect();
        TLC5926* config(int hi_lo_current, int hi_lo_voltage_band, int voltage_gain);
        TLC5926* off();
        TLC5926* on();
        void shift(unsigned int pattern); // not chainable!
        TLC5926* send(unsigned int pattern);
        TLC5926* all(int hilo);
        TLC5926* send_bits(int ct, short int bits, int delay_between = 0);
        TLC5926* brightness(char brightness);
        TLC5926* delay(unsigned long duration);
        TLC5926* delayMicroseconds(unsigned int duration);
        TLC5926* flash(unsigned int on = 50, unsigned int bracket = 200, boolean leave_on = true);
        unsigned short int read_sdo();



    };

#endif
