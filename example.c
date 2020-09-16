#include <avr/io.h>
#include <avr/interrupt.h>

// Value to hold potentiometer reading
int sensorVal = 0;

// Value to hold ADC reading, necessary because ADCL must be read first or ADCH would be overwritten
int lowVal = 0;

// Used to detect falling pin change
int buttonState = 0;
int prevButtonState = 0;

// The current state of rotation amount/ potentiometer reading
int divideCount = 0;

int main(){
  
  // Enable interrupts for timed servo pulse
  sei();

  // Enable our button reading pin
  pinMode(5, INPUT);
  // Enable our servo pulse pin
  pinMode(8, OUTPUT);

  // Analog read method inexplicably does not function outside of setup() loop() method structure, and setup() loop() structure for some reason sets TCNT1 counter to 8 bit instead of 10
  // Because of these problems, and my desire for better timing to reduce servo jitter, I was forced to write out all the code to read in ADC from pins

  // *** Begin ADC setup ***  
  
  // Set direction register for ADC pins to inputs
  DDRD = 0;

  // Enable ADC conversions (first bit)
  // Set prescaler (last 3 bits)
  // Information found on data sheet 
  ADCSRA = 0b10000111;

  // Set reference to 0 (3rd bit)
  // ADLAR to 0, to have 8 bit representation in ADCH, and 2 more bits of information in ADCL (if we wanted low resolution we could just use ADCH)
  // Set A0 pin to be our input port (final 3 bits)
  ADMUX = 0b01100000;
  
  // *** End ADC setup ***

  // *** Start Timer setup ***
  
  // Turn on our timer with a prescaler of 64 because 20 MHz is really fast and 10-bit counter is really small
  // For every 64 clock cycles we'll now have one count increment
  TCCR1B |= 1<<CS10;
  TCCR1B |= 1<<CS11;
  // Forces reset on comparison
  TCCR1B |= 1<<WGM12;
  // Enable comparison equivilence interrupt
  TIMSK1 |= 1<<OCIE1A;
  // Set comparison value, which when reached will set off servo pulse ISR
  // Value our counter reaches at 20ms, found with (.02s / (1s / (20MHz / prescaler))) = 6250
  OCR1A = 6250;

  // *** End Timer setup ***
  
  while(1){
    
    // I noticed that with a simple resistor setup the button very cleanly transferred from on to off
    // so we only need to find out when the button was on previously and is now off to know if its been pushed
    
    // Find button state
    buttonState = digitalRead(5);

    // Check if button is off but was just on
    if (!buttonState && prevButtonState){
      
      // if we get here it was just let go, so we need to change the conversion state were in
      
      if (divideCount > 1){
        divideCount = 0;
      }else{
        divideCount = divideCount + 1;
      }
      
    }

    // set the previous state to the current state, as all state related operations are done
    prevButtonState = buttonState;

    // Set ADSC bit to one to start the ADC 
    ADCSRA |= 1 << ADSC;

    // Check if conversion is finished (ADSC flag will return to 0)
    while(ADCSRA & (1 << ADSC)){
      // Wait for conversion to finish
      // Once ADSC bit is reset to 0 its done
    }

    // Have to read ADCL first, or we lose our info in ADCH
    lowVal = ADCL;
    // lowVal contains the 10 bit numbers rightmost 2 bits, ADCH contains the leftmost 8 
    // Shift each to their respective positions, then perform bitwise or to end with correct resulting value
    sensorVal = ADCH << 2 | lowVal >> 6;
    
  } 
}

ISR(TIMER1_COMPA_vect){
  // Getting here means our timer has gone off for servo pulsing (20ms)

  // First check how we should convert our sensor input depending on the button state
  if (!divideCount){
    sensorVal = 380 + ((sensorVal - (1024 / 2)) / 2.50366);
  }else if(divideCount < 2){
    sensorVal = 380 + ((sensorVal - (1024 / 2)) / 5.00733);
  }else{
    sensorVal = 380 + ((sensorVal - (1024 / 2)) / 15.0220);
  }
  
  // Start the pulse
  digitalWrite(8, HIGH);
  
  //170 = 0 (6250 / 40, representing .5s pulse or the minimum for our servo, moved out of vibration area)
  //579 = 180 (6250 / 10 = 625, but I found that this value will rotate slightly greater than 180 degrees so I manually calibrated to 180 degrees and found 560) 

  // While our counter(effectively timer) is less than our sensor value mapped into the above range
  
  while(TCNT1 < sensorVal){
    // continue to pulse for necessary time
    // Can be optimized so the computer can do other things during this pulse
    // Unecessary becuase we have nothing else to do, this delays and functionally skips out some noisy ADC changes during movement
  }

  // Turn the pulse off and return to conversion loop
  digitalWrite(8, LOW);
  
}
