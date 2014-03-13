# TLC5926 for Arduino

A library that knows how to talk to a TLC5926/TLC5927 (16-bit shift-register).

* Supports "slow" (digitalWrite / shiftOut) mode (non-SPI).
* The global brightness feature uses PWM, so is not blocking (requires iOE pin).
* Knows that /OE is inverted.
* Should work for TLC5916/TLC5917: just divide the "number chained together" in half.
    And, the patterns are 2 shift-registers wide.
* Can get the diagnostic-mode info (short/open/over-temp).
* Can set the current-gain value.

Supports 2-4 signal lines (with appropriate "pull-down" resistors):

* Minimal/2-Wire -- SDI + CLK
    * minimal control lines
    * assumes LE->CLK, and /OE->GND 
    * subject to dim flickerings -- data is visible as it is shifted
    * no brigtness control (roll your own blocking pwm)
* No Flicker -- SDI + CLK + LE
    * no flickering during shifting -- data is not visible until end-of-pattern
    * assumes /OE->GND
    * no brigtness control (roll your own blocking pwm)
* Master-Power/better-pwm (OE) -- SDI + CLK + LE + /OE
    * can turn the whole shift-register chain on/off with one bit
    * brightness control
* Diagnostics
    * can read diagnostics

## Hardware setup

These shift-registers let you set/limit the current with 1 resistor ("VEXT"). Check the curve in the datasheet, but you could use a trimpot to make it adjustable. You could "pull" VEXT to GND without any resistor, and that sets the current to.... Leaving it disconnected sets it to ....

If you put a pull-up on LE to CLK, then you can reduce the number of wires needed. However, you can get flickering if you do this. If you make this pull-up value high, like 100k, then you can choose if you want to use LE or not (the Arduino signal will override the LE pull-up). 

If you put a pull-down on /OE to GND, then you can reduce the number of wires needed. If you make this pull-up value high, like 100k, then you can choose if you want to use /OE or not (the Arduino signal will override the /OE pull-down).

So, if you do both of the above, you only need SDI + CLK to drive the TLC5926 (and power and ground).

For high-speed use (SPI), you'll probably have to put some capacitors on the signal-lines.

I've seen that the diagnostic mode doesn't do a good job of detecting open/shorted LEDs. There's a ratio of Vcc to LED voltage that is a threshold.

## What mode to choose:
* Are you trying to control/signal something other than an LED?
    * You probably don't want the shifted-data to appear while it is being written.
    * choose a setup with the LE line
* Do you want to control the brightness of the entire chain?
    * choose a setup with the /OE line on a PWM pin.
* Are you "walking" a pattern (shifting the pattern fairly slowly)?
    * 2-Wire should work.
    * dim-flickerings probably won't be noticeable.
* Are you rapidly changing the pattern?
    * To avoid dim-flickering use the LE line: "No Flicker" setup (or "Master-Power").
* Are you controlling something that takes a PWM input (like a servo)?
    * It might work with the global-brightness feature, experiment. But, remember, all HIGH outputs are controlled at once! Someone should scope this and see how clean the outputs are.
    * You could carefully do this "manually" by shifting patterns into the shift-register. You probably want to use LE.
    * Look into the Shift-PWM library: <http://www.elcojacobs.com/shiftpwm>

### Stupid Tricks

The diagnostic function can be used to get 16 digital inputs! Put a switch on one or more of the TLC's output-channels. The diagnostic result will tell you if an output-channel is open or closed. Obviously, anything that provides a digital value can be used: reading signals that rely on timing would be difficult. You should be able to sample the whole TLC at a fairly high rate. You can even mix input/outputs: put some LEDs on some, some switches on others.

By combining outputs, you might be able to run motors or servos. While the TLC provides up to 120ma, that's rarely enough to _start_ even small motors. Motors take a surge of current to start, something like 5 times their running current or more. So, hook 3 or more outputs together to power the motor, and turn them all on/off together. You'll want to use the LE signal.

Similarly, you could power big things by combining outputs.

In the future: SPI (hardware interface) mode, 

> * SPI mode assumes that you have a setup that can handle the high-frequency
> * signals to the TLC5926. That usually means decoupling caps (1muf) on the
> * signal lines (near the TLC5926).


# Use:

       // you could have several of these
       TLC5926 shift_register1;

       // avoid pins 0 and 1, that's typically used for USB and serial.
       // avoid pin 13, the bootloader does stuff to it
       const int SDI_pin = 2;
       const int CLK_pin = 3;
       const int LE_pin = 4;
       const int iOE_pin = 5; // put this on a PWM pin

       void setup() {
           ...
           Serial.begin(9600); // for warnings

           // turn on warnings. probably turn it off when in production
           shift_register1.debug(1); // requires Serial.begin(...);

           // LE_pin and iOE_pin would be -1 if not hooked up
           shift_register1.attach(1, SDI_pin, CLK_pin, LE_pin, iOE_pin ); // attach 1 shift-register, "1" is optional
           
           // A second shift_register obviously would be on different pins

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
           shift_register1.send(on_off_pattern);

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
           
           // Has fancy use style:
           // First one is a ".", the rest are "->"
           shift_register1.all(HIGH)->delay(100)->send( 0b0011001100110011 )->delay(500)->all(LOW);
           // Line breaks ok:
           shift_register1.all(HIGH)->delay(100)
               ->send( 0b0011001100110011 )->delay(500)
               ->all(LOW)
               ;
           }
