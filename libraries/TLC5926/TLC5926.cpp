/*
    A library that knows how to talk to a TLC5926/TLC5927 (16-bit shift-register).
    TLC5916/TLC5927 code would be shockingly similar (8bit instead of 16).

    In the future: Supports SPI (hardware interface) mode, 
        SPI mode assumes that you have a setup that can handle the high-frequency
        signals to the TLC5926. That usually means decoupling caps (1muf) on the
        signal lines (near the TLC5926).
        -- Untested because I don't have the caps.

    Supports "slow" (digitalWrite / shiftOut) mode (non-SPI).

    The brightness feature uses PWM, so is not blocking (requires iOE pin).

    Supports 2-4 signal lines:
        Minimal/2-Wire -- SDI + CLK
            minimal control lines
            assumes LE->CLK, and /OE->GND 
            subject to dim flickerings -- data is visible as it is shifted
            no brigtness control (roll your own blocking pwm)
        No Flicker -- SDI + CLK + LE
            no flickering during shifting -- data is not visible until end-of-pattern
            assumes /OE->GND
            no brigtness control (roll your own blocking pwm)
        Not implemented: SDI + CLK + OE
            Could work a lot like SDI + CLK + LE
            better crude-pwm
            could do no-flicker
        Master-Power/better-pwm (OE) -- SDI + CLK + LE + /OE
            can turn the whole shift-register chain on/off with one bit
            brightness control
        Diagnostics
            can read diagnostics -- not implemented yet

        What mode to choose:
            Are you trying to control/signal something other than an LED?
                You probably don't want the shifted-data to appear while it is being written.
                choose a setup with the LE line
            Do you want to control the brightness of the entire chain?
                choose a setup with the /OE line
            Are you "walking" a pattern (shifting the pattern a few)?
                2-Wire should work.
                dim-flickerings probably won't be noticeable.
            Are you rapidly changing the pattern?
                To avoid dim-flickering use the LE line: No Flicker setup (or Master-Power).
            Are you controlling something that takes a PWM input (like a servo)?
                It might work with the brightness feature, experiment.
                But, remember, all HIGH outputs are controlled at once!
                Someone should scope this and see how clean the outputs are.
                Someone should experiment with blocking PWM and LE.
*/

/* Use:
    TLC5926 shift_register1;
    // avoid pin 13, the bootloader does stuff to it
    const int SDI_pin = 2;
    const int CLK_pin = 3;
    const int LE_pin = 4;
    const int iOE_pin = 5;

    void setup() {
        ...
        Serial.begin(9600); // for warnings

        // turn on warnings. probably turn it off when in production
        shift_register1.debug(1); // requires Serial.begin(...);

        // LE_pin and iOE_pin would be -1 if not hooked up
        shift_register1.attach(1, SDI_pin, CLK_pin, LE_pin, iOE_pin ); // attach 1 shift-register, "1" is optional
        // nice to set everything off/clear at first
        // otherwise, leaves the tlc5926 with whatever data it had, and powering outputs
        shift_register1.off();
        shift_register1.reset();
        ...
        }

    // A pattern of alternating on/off
    const word on_off_pattern = 0xAAAA; // 16 bits. beware signed int's! high int may not get set!
    // ... off/on
    const word off_on_pattern = 0x5555;
    const int marguee_pattern = 0b11101110;

    void loop() {
        ...
        // Shifts the data out 
        // Uses LE if it was provided -- for no-flicker
        // We are always MSBFIRST
        shift_register1.send(on_off_pattern); // or a list of patterns

        delay(1000);
        shift_register1.off(); // turns output-power off. warning if not using /OE

        shift_register1.all(LOW); // shifts LOW into all shift-registers
        shift_register1.on(); // power is on but all outputs are off (from previous)

        // Partial ints are slower than whole bytes/words. Still, uses LE if provided
        shift_register1.send_bits(4,0x8); // shift in 4 bits

        shift_register1.all(HIGH); // shifts HIGH into all shift-registers

        // runs crude-pwm for brightness for a duration
        // Uses /OE if provided, or LE if provided, or you get dim-flickerings
        // duty-cycle approximates brightness: some fractional amount
        shift_register1.brightness(200); // 0-255

        // Animation -- cycle through patterns
        // Will use LE for no-flicker if provided
        unsigned int start = millis();
        const int animate_delay = 200;
        while (millis()-start < 10000) { // for 10 seconds
            // Just a 16-output pattern:
            shift_register1.send( 0b00011000 ); delay(animate_delay);
            shift_register1.send( 0b00111100 ); delay(animate_delay);
            shift_register1.send( 0b01100110 ); delay(animate_delay);
            shift_register1.send( 0b11000011 ); delay(animate_delay);
            shift_register1.send( 0b10000001 ); delay(animate_delay);
            shift_register1.send( 0b00000000 ); delay(animate_delay);
            shift_register1.send( 0b11000011 ); delay(animate_delay);
            shift_register1.send( 0b01100110 ); delay(animate_delay);
            shift_register1.send( 0b00111100 ); delay(animate_delay);
            shift_register1.send( 0b00011000 ); delay(animate_delay);
            shift_register1.send( 0b00000000 ); delay(animate_delay);
            }

        // Send data and control the latch yourself
        // (assume LE is low)
        shift_register1.shift(0x0808); // 
        shift_register1.latch_pulse(); // trigger latch, pattern shows on outputs
        ...
        
        // diagnostics, "Special Mode"
        // Error-Detection, Current-Gain-Control
        enter special (Mode Switching)
            CLK -+-+-+-+-+
            /OE ++--++++++
                Does perform output-enable!
            LE  ------++--
                LE does not latch-data here
            SDI ...
                Is shifted!
                
        fetch data:
            CLK to shift 16 bits out, appears on SDO
            open/short
            CLK -+-+-+-+-+-+-+-+-+-+-+-
            /OE ++-----..>2mus.+...
                loads the "
            LE  -...
            SDO .......d-d-d-d-d-d-d-d-
                data clocks-out on sdo
                read data after CLK-to-high
            Can't chain
                   
        set Current-Gain-Control (global): 
            Configuration Code out SDI, with LE (on ->high), 8bits
                current range: 1/12 to 127/128 in 256 steps
            CLK -+..clock data as normal
            SDI ++--...data w/clk as normal
                16 bits
            /OE +...
            LE  -...........-+-
                LE to low on clock-to-high
                LE does not latch-data here
                writes to settings
            Could be chained: it's waiting for LE
            
        Back to normal mode:
            CLK -+-+-+-+-+
            OE  ++--++++++ less than 2mus on clock
            LE  ----------     
        
        // trouble shooting:
        flaky when using /OE
            if you  
        }
*/


#include <TLC5926.h>
#include "pins_arduino.h"

TLC5926::TLC5926() {
    SDI = -1;
    CLK = -1;
    LE = -1;
    iOE = -1;
    SDO = -1;
    ct = 0; // use this as the signal for attached
    pwm = false; // is iOE on pwm?
    debugging = false;
    }

int TLC5926::SDI_pin() { return SDI; }
int TLC5926::CLK_pin() { return CLK; }
int TLC5926::LE_pin() { return LE; }
int TLC5926::iOE_pin() { return iOE; }
int TLC5926::SDO_pin() { return SDO; }

TLC5926* TLC5926::debug(boolean v) { debugging = v; return this; }

void TLC5926::debug_print(const char * msg ) {
    debug_prefix();
    Serial.println(msg);
    }

TLC5926* TLC5926::debug_prefix() {
        Serial.print("[TLC5926 ");
        Serial.print((uintptr_t)this);
        Serial.print("] ");
        return this;
        }

TLC5926* TLC5926::attach(int sdi_pin, int clk_pin) {
    return attach(1, sdi_pin, clk_pin, -1, -1, -1); 
    }

TLC5926* TLC5926::attach(int chained_ct, int sdi_pin, int clk_pin) {
    return attach(chained_ct, sdi_pin, clk_pin, -1, -1, -1); 
    }

TLC5926* TLC5926::attach(int sdi_pin, int clk_pin, int le_pin, int ioe_pin) { 
    return attach(1, sdi_pin, clk_pin, le_pin, ioe_pin, -1); 
    }

TLC5926* TLC5926::attach(int chained_ct, int sdi_pin, int clk_pin, int le_pin, int ioe_pin, int sdo_pin) {
    if (ct) {
        if (debugging) debug_print("Warning, already attached.");
        }

    else {

        ct = chained_ct;
        SDI = sdi_pin;
        CLK = clk_pin;
        LE = le_pin;
        iOE = ioe_pin;
        SDO = sdo_pin;

        pinMode(CLK, OUTPUT);
        digitalWrite(CLK, LOW);
        pinMode(SDI, OUTPUT);
        if (debugging) {
            debug_prefix();
            Serial.print("Attached to ");
            Serial.print(ct);
            Serial.print(" x #");
            Serial.print(SDI);
            Serial.print(" clock #");
            Serial.print(CLK);
            }

        if (LE > -1) {
            if (debugging) {
                Serial.print(", w/LE #");
                Serial.print(LE);
                }
            pinMode(LE, OUTPUT);
            digitalWrite(LE, LOW);
            }
        if (iOE > -1) 
            {
            if (debugging) {
                Serial.print(", w/iOE #");
                Serial.print(iOE);
                }
            if (digitalPinToTimer(iOE) != NOT_ON_TIMER) {
                pwm = true;
                }
            on();
            }

        if (SDO > -1) {
            if (debugging) {
                Serial.print(", w/SDO #");
                Serial.print(SDO);
                }
            pinMode(SDO, INPUT); // don't sink
            }

        if (debugging) Serial.println();
        }

    return this;
    }

TLC5926* TLC5926::reset() {
    // Try safest sequence to sane config:
    // We should be config(default), normal_mode, etc

    if (pwm) pinMode(iOE,OUTPUT); // pwm inhibits digitalWritey

    if (iOE != -1 && LE != -1) normal_mode();
    if (iOE != -1) off();
    if (SDO != -1) pinMode(SDO, INPUT); // so it doesn't affect the chained sdo
    all(LOW); // this could take some time...
    if (iOE != -1 && LE != -1) config(1,1,127);
    if (iOE != -1) on();
    return this;
    }

const int NORMAL_MODE_PATTERN[5][3] = {
    // CLK -+
    // OE  ++ less than 2mus on clock
    // LE  --     
    { LOW , HIGH, LOW }, { HIGH, HIGH, LOW }, // LE low here == normal
    { LOW , HIGH , LOW  }, { HIGH, HIGH , LOW }, // final "fill"
    { -1,-1,-1 }
    };
const int SWITCH_MODE_PATTERN[10][3] = {
    // CLK -+-+...
    // OE  ++--++++ less than 2mus on clock
    // LE  ------++     
    { LOW , HIGH, LOW }, { HIGH, HIGH, LOW }, // start OE hi
    { LOW , LOW , LOW }, { HIGH, LOW , LOW }, // oe pulse
    { LOW , HIGH, LOW }, { HIGH, HIGH, LOW },
    { -1,-1,-1 }
    };
const int ERROR_DETECT_MODE_PATTERN[9][3] = {
    // CLK -+ -+ -+ -+ -+ -+ -+ -+ -+ -+
    // iOE ++ -- -- -- wait 2mus, the ++ and clk out 16 bits
    // LE  -- -- -- -- ...
    //NB: causes outputs to sink current!
    { LOW , HIGH, HIGH }, { HIGH, HIGH, HIGH }, // le pulse = "special mode"
    { LOW , HIGH , LOW  }, { HIGH, HIGH , LOW }, // final special mode pattern -- "fill"
    { LOW , LOW , LOW }, { HIGH, LOW , LOW }, // 1 of 3 iOE low
    { LOW , LOW , LOW }, { HIGH, LOW , LOW }, // 2 of 3 iOE low
    // wait 2mus from first OE low, then ERROR_DETECT_READY
    { -1, -1, -1 }
    };
const int ERROR_DETECT_READY[4][3] = { 
    { LOW , LOW , LOW }, { HIGH, LOW , LOW }, // 3 of 3 iOE low
    // CLK will shift data like normal while iOE is high
    { LOW, HIGH, LOW }, // iOE back high, note no clock
    { -1, -1, -1 }
    };
const int CONFIGURATION_MODE_PATTERN[5][3] = {
    { LOW , HIGH , HIGH }, { HIGH, HIGH , HIGH }, // LE = "special mode"
    { LOW , HIGH , LOW  }, { HIGH, HIGH , LOW }, // final special mode pattern -- "fill"
    { -1, -1, -1 } // nothing
    };

#define d_digitalWrite(p, v) (Serial.print("Set "), Serial.print(p), Serial.print("="),Serial.println(v), digitalWrite(p,v))

void TLC5926::do_clk_ioe_le(const int state_list[][3]) {
    // output patterns for the mode stuff: clk+iOE+LE
    const int *pins;

    for(int i=0; state_list[i][0] != -1; i++) {
        // debug_prefix(); Serial.print("state line"); Serial.println(i);
        pins = state_list[i];
        // Serial.print("COL ");Serial.print(pins[0]); Serial.print(pins[1]);Serial.println(pins[2]);
        digitalWrite(CLK, pins[0]);
        digitalWrite(iOE, pins[1]);
        digitalWrite(LE, pins[2]);
        }
    digitalWrite(CLK, LOW);
    }

void TLC5926::switch_mode(const int pattern[][3]) {
    if (debugging) debug_print("Switch mode...");

    if (pwm) pinMode(iOE,OUTPUT); // pwm inhibits digitalWrite

    do_clk_ioe_le(SWITCH_MODE_PATTERN);
    if (debugging) debug_print( 
        pattern == NORMAL_MODE_PATTERN 
            ? "Normal Mode" 
            : (pattern == ERROR_DETECT_MODE_PATTERN
            ? "Error Detect Mode"
            : (pattern == CONFIGURATION_MODE_PATTERN
            ? "Config Mode"
            : "undef mode"))
        );
    do_clk_ioe_le(pattern);
    }

TLC5926* TLC5926::normal_mode() {
        /*
        Back to normal mode:
            CLK -+-+-+ -+-+
            OE  ++--++ ++++ less than 2mus on clock
            LE  ------ ----     
        */
    if (LE == -1 || iOE == -1) {
        if (debugging) debug_print("Can't do normal_mode() w/o LE and iOE");
        }
    else {
        switch_mode(NORMAL_MODE_PATTERN);
        }
    return this;
    }

unsigned short int TLC5926::read_sdo() {
    if (SDO == -1) {
        if (debugging) debug_print("Can't do read_sdo() w/o SDO");
        return 0;
        }
    else {
        pinMode(SDO, INPUT);
        return digitalRead(SDO); 
        }
    }

unsigned int TLC5926::error_detect() {
    // Returns 16 bits
    unsigned int status = 0;

    if (LE == -1 || iOE == -1 || SDO == -1) {
        if (debugging) debug_print("Can't do error_detect() w/o LE, iOE, and SDO");
        }
    else {
        send(0xFFFF); // everybody on for detect. we can only read the first in a chain
        // on(); // need it on to work
        switch_mode(ERROR_DETECT_MODE_PATTERN);
        ::delayMicroseconds(3); // actually, from earlier in the pattern, but "at least 2"

        if (debugging) debug_print("Read status...");
        do_clk_ioe_le(ERROR_DETECT_READY); // ready for read
        if (debugging) debug_print("Ready");

        pinMode(SDO, INPUT);
        digitalWrite(SDI, LOW); // we clock SDO out, and clock SDI in, so leave low

        for(int i=0; i<16; i++) {
            int r;
            r = digitalRead(SDO); 
            Serial.println(r,HEX);
            status = (status << 1) | r;
            Serial.print("clock data ");Serial.print(i); Serial.print(" "); Serial.println(status,BIN);

            digitalWrite(CLK,HIGH); // "detect" on rising
            digitalWrite(CLK,LOW);

            }
        Serial.println("clocked!");
        if (debugging) {
            debug_prefix();
            Serial.print("Error Detect Status ");
            Serial.println(status,BIN);
            }

        pinMode(SDO, OUTPUT);
        normal_mode();
        }
    return status;
    }

TLC5926* TLC5926::config(int hi_lo_current, int hi_lo_voltage_band, int voltage_gain) {
    if (LE == -1 || iOE == -1) {
        if (debugging) debug_print("Can't do config() w/o LE, iOE");
        }
    else {
        send(0xFFFF); // everybody on for detect. we can only read the first in a chain
        switch_mode(CONFIGURATION_MODE_PATTERN);

        // We are going to do LSB shift, so voltage_gain reads: large=high
        unsigned int value;
        value = voltage_gain & 0x003F ; // 0-127
        if (hi_lo_current) value |= 0x80;
        if (hi_lo_voltage_band) value |= 0x40;
        // Serial.print("Config "); Serial.println(value, BIN);
        shiftOut(SDI, CLK, MSBFIRST, 00); // high-bits are zero
        shiftOut(SDI, CLK, LSBFIRST, value); // CM.HC.CC6
        latch_pulse();
        normal_mode();
        }

    return this;
    }

TLC5926* TLC5926::latch_pulse() {
    if (LE != -1) {
        digitalWrite(LE, HIGH); // we were low, this is "doit"
        digitalWrite(LE, LOW); // for next time
        }
    else {
        if (debugging) debug_print("Warning: latch_pulse() -- LE not specified");
        }
    return this;
    }

TLC5926* TLC5926::on() {
    if (iOE != -1) {
        if (debugging) debug_print("ON");
        if (pwm) pinMode(iOE,OUTPUT); // pwm inhibits digitalWriteyy
        digitalWrite(iOE, LOW); // inverted
        }
    else {
        if (debugging) debug_print("Warning: on() -- iOE not specified");
        }
    return this;
    }

TLC5926* TLC5926::off() {
    if (iOE != -1) {
        if (debugging) debug_print("OFF");
        if (pwm) pinMode(iOE,OUTPUT); // pwm inhibits digitalWriteyy
        digitalWrite(iOE, HIGH); // inverted
        }
    else {
        if (debugging) debug_print("Warning: off() -- iOE not specified");
        }
    return this;
    }

TLC5926* TLC5926::send(unsigned int pattern) {
    shift(pattern);
    if (LE != -1) latch_pulse();
    return this;
    }

void TLC5926::shift(unsigned int pattern) {
    shiftOut(SDI, CLK, MSBFIRST, pattern >> 8); // msb
    shiftOut(SDI, CLK, MSBFIRST, lowByte(pattern)); // msb
    }

TLC5926* TLC5926::all(int hilo) {
    for (int i=0; i<ct; i++) {
        shift(hilo ? 0xFFFF : 0);
        }
    if (LE != -1) latch_pulse();
    return this;
    }

TLC5926* TLC5926::send_bits(int ct, short int bits, int delay_between) { 
    for (; ct>0; ct--) {
        digitalWrite(SDI, bitRead(bits, ct-1));
        digitalWrite(CLK, HIGH);
        digitalWrite(CLK, LOW);
        if (delay_between) {
            if (LE != -1) latch_pulse();
            ::delay(delay_between) ;
            }
        }
    if (!delay_between && LE != -1) latch_pulse();
    return this;
    } 

TLC5926* TLC5926::brightness(char brightness) {
    // 255 levels of brightness should be enough for anyone?
    if (iOE == -1) {
        if (debugging) debug_print("Warning, no way to do brightness if not using iOE");
        return this; // we could do blocking brightness
        }
    else if (!pwm) {
        if (debugging) debug_print("Warning, iOE pin is not on a PWM");
        // we could do blocking brightness
        }
    else {
        analogWrite(iOE, ~(brightness));
        }
    return this;
    }

TLC5926* TLC5926::delay(unsigned long duration) {
    // just for convenience of chaining
    ::delay(duration);
    return this;
    }

TLC5926* TLC5926::delayMicroseconds(unsigned int duration) {
    // just for convenience of chaining
    ::delayMicroseconds(duration);
    return this;
    }

TLC5926* TLC5926::flash(unsigned int on_duration, unsigned int bracket, boolean leave_on) {
    // Flash the current pattern using iOE, leaves it ON by default
    // Brackets the ON time with off time on both sides (to make the flash visible)
    // thus, you don't need to delay() on either side.
    // ... for non iOE mode, could starttime, shift in 1's, latch, hold for brackettimeleft, 0's, latch, hold
    //          but that might be confusing because in that mode it whacks the pattern?
    off();
    ::delay(bracket);
    on();
    ::delay(on_duration);
    off();
    ::delay(bracket);
    if (leave_on) on();
    return this;
    }
