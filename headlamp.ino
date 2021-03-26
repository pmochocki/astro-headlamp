/*
MIT License

Copyright (c) 2021 Piotr Mochocki

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>

// Microswitch defines
#define KEY_PIN (1<<PB1)                    // micro switch pin connected to PB1
#define KEY_DOWN !(PINB & KEY_PIN)          // macro for checking if button is pressed used by push_button()

// LEDs defines
#define RED_LED_PIN (1<<PB0)                // red led connected to PB0 - this can not be changed because of PWM
#define WHITE_LED_PIN (1<<PB3)              // white led connected to PB3

// Helper functions
uint8_t push_button(void);                  // debounce handling (80ms)
void sleep(void);                           // puts mc to sleep - sleep mode (SLEEP_MODE_IDLE/SLEEP_MODE_PWR_DOWN)
                                            // is set outside this function using set_sleep_mode()

int main(void)
{
    // Initialization
    // Disabling unused staff to save power
    cli();                                  // Deactivate Interrupts      
    GIMSK |= (1<<INT0);                     // Enable INT0 interrupt
    ADCSRA &= ~(1<<ADEN);                   // By writing it to zero, the ADC is turned off
    ACSR |=  (1<<ACD);                      // When this bit is written logic one, the power to the Analog
                                            // Comparator is switched off
    WDTCR &= ~(1<<WDTIE);                   // Disabling the Watchdog Timer. Just in case WDTON fuse is programmed.
    // BOD - Ensure it is disabled using the fuses. It can not be changed from the program brightness_level.

    // Microswitch initialization
    DDRB  &= ~KEY_PIN;                      // set key pin as input
    PORTB |=  KEY_PIN;                      // enable internal pull-up resistor 

    // LEDs initialization
    DDRB  |= RED_LED_PIN;                   // set red led pin as output
    DDRB  |= WHITE_LED_PIN;                 // set withe led pin as output

    // PWM initialization
    TCCR0A |= (0 << WGM01) | (1 << WGM00);  // Table 11-8; phase correct mode 
    TCCR0A |= (1 << COM0A1) | (1 << COM0A0);                // set output to PB0 - check it maches the red LED
                                            // Table 11-4; Clear OC0A on Compare Match when up-counting.
                                            // Set OC0A on Compare Match when down-counting.
    TCCR0B |= (0 << CS02) | (0 << CS01) | (1 << CS00);    // no prescaling (Table 11-9)

    // red led brightness levels - the led brightness is not changing linear but exponential 
    uint8_t brightness_level[] = {1, 2, 4, 8, 16, 32, 64, 128, 255};  // used 9 values from 2^n 
    const uint8_t brightness_index_max = sizeof(brightness_level) - 1;
    int8_t brightness_index = 5;            // used for indexing the brightness levels table 
    int8_t direction = 1;                   // defines if a long press of the button will dim or brighten led 
    uint8_t white_on_fuse = 0;
    bool long_press = 0;                    // falg if the button was pressed long
    
    OCR0A = brightness_level[brightness_index];    // set inital red led brightness after reset 
    PORTB |=  (1 << WHITE_LED_PIN);           // turn white led off

    // endless loop
    while(1)
    {               
        if(push_button())
        {
            _delay_ms(400);
            if(KEY_DOWN)    // if after 400ms + debuncing the button is pressed - it is a long press
            {               // this means we will adjust the red led brightness
                brightness_index = brightness_index + direction;
                if( brightness_index >= brightness_index_max )
                {
                    brightness_index = brightness_index_max;
                    white_on_fuse++;
                }
                else if( brightness_index <= 0)
                {
                    brightness_index = 0;
                }
                
                if(white_on_fuse >= 10)
                {
                    PORTB  &= ~WHITE_LED_PIN;
                    OCR0A = 0;
                }
                else
                {
                    OCR0A = brightness_level[brightness_index];         
                }
                set_sleep_mode(SLEEP_MODE_IDLE);    // in long press mode we allways go to idle mode
                long_press = 1;
            }
            else    // the button was pressed but shortly or released during 400ms after changing brightness
            {
                if(!long_press)   // the button was pressed but shortly - on/off case
                {
                    if(OCR0A)     // led was on - switch it off and go to power down mode 
                    {
                        OCR0A = 0;
                        set_sleep_mode(SLEEP_MODE_PWR_DOWN);
                        _delay_ms(200);    // Additional 200ms before power down mode
                    }
                    else          // led was off - switch it on and go to idle mode
                    {
                        OCR0A = brightness_level[brightness_index];
                        set_sleep_mode(SLEEP_MODE_IDLE);
                    }      
                }
                else   // released during 400ms after changing brightness
                {
                    direction = -direction;
                    long_press = 0;
                    white_on_fuse = 0;
                }
            }
        }
        else if(long_press)    // long press released probably during debouncing
        {
            direction = -direction;
            long_press = 0;  
            white_on_fuse = 0; 
        }
        
        // go to sleep - the mode was already set 
        sleep();  
    }
}

// debounce handling (80ms)
uint8_t push_button(void)
{
    if(KEY_DOWN)
    {
        _delay_ms(80);
        if(KEY_DOWN) return 1;
    }
    return 0;
}

// puts mc to sleep - sleep mode (SLEEP_MODE_IDLE/SLEEP_MODE_PWR_DOWN) is set outside 
// this function using set_sleep_mode() - not the prettiest solution. 
void sleep(void)
{
    sleep_enable();
    sei();          //Activate Interrupts

    sleep_cpu();
    //and wake up
    cli();          //Deactivate Interrupts
    sleep_disable();  
}

// INT0_vect is empty; just need to wake up from SLEEP_MODE_PWR_DOWN
ISR(INT0_vect)
{
}
