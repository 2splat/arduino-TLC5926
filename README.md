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
        can turn the whole Dandelion chain on/off with one bit
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

Use:
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
