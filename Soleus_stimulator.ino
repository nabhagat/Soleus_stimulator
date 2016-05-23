/*
 * Program to delivering 125 stim pulses
 * 
 * ENS2020 Pin configuration    Arduino MEGA 256 Pins
 * 1 - GND                                GND (pin 44 unused)
 * 2 - Not connected                      45
 * 3 - SEL                                46
 * 4 - DOWN (decrement)                   47
 * 5 - BWD (Backward)                     48
 * 6 - OFF                                49
 * 7 - FWD (Forward)                      50
 * 8 - UP (increment)                     51
 * 9 - MENU                               52
 * 10 - ON                                53
 * By default pins should be HIGH
 */

#include <avr/pgmspace.h>  // Required for using PROGMEM, Syntax: const dataType variableName[] PROGMEM = {data0, data1, data3...};

#define array_len( x )  ( sizeof( x ) / sizeof( *x ) )

const int ens_ON_pin = 53;
const int ens_OFF_pin = 49;

const int ens_MENU_pin = 52;
const int ens_SEL_pin = 46;

const int ens_FWD_pin = 50;
const int ens_BWD_pin = 48;

const int ens_UP_pin = 51;
const int ens_DOWN_pin = 47;

int stim_received_LED_pin = 27;
int end_of_stim_LED_pin  = 31;    // Use to indicate end of trial
int AIN1_pin = 5;     // Negative input pin of Analog comparator; Positive pin is internal reference = 1.1V
int ledPin = 13;
int Number_of_stim_bouts_required = 10;
const int keypress_interval_very_long = 500;
const int keypress_interval_long = 200;
volatile boolean stim_recvd = false;
volatile int stim_counter = 0;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);        // Turn on the Serial Port

  // Configure ENS - Set MENU, SEL, UP and DOWN pins as output and set them to HIGH; rest as input
  pinMode(ens_ON_pin, INPUT_PULLUP);
  pinMode(ens_OFF_pin, INPUT_PULLUP);
  pinMode(ens_FWD_pin, INPUT_PULLUP);
  pinMode(ens_BWD_pin, INPUT_PULLUP);
  
  /pinMode(ens_DOWN_pin, OUTPUT);
  /digitalWrite(ens_DOWN_pin, HIGH);
  /pinMode(ens_UP_pin, OUTPUT);
  /digitalWrite(ens_UP_pin, HIGH);
  pinMode(ens_MENU_pin, OUTPUT);
  digitalWrite(ens_MENU_pin, HIGH);
  pinMode(ens_SEL_pin, OUTPUT);
  digitalWrite(ens_SEL_pin, HIGH);

  // Configure Arduino LED, ADC and Analog Comparator pins
  /////Inputs
  pinMode(AIN1_pin, INPUT);

  ////Outputs
  pinMode(stim_received_LED_pin, OUTPUT);
  digitalWrite(stim_received_LED_pin, LOW);
  pinMode(end_of_stim_LED_pin, OUTPUT);
  digitalWrite(end_of_stim_LED_pin, LOW);
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);

  //Configure Interrupts - Timers, Analog Comparators
  noInterrupts();     // disable all software interrupts
  ACSR = B01010010;   // Analog comparator control and status register
  /*ACSR =
    (0<<ACD) | // Analog Comparator Disabled
    (1<<ACBG) | // Analog Comparator Bandgap Select: AIN0 is connected to 1.1V internal reference
    (0<<ACO) | // Analog Comparator Output: Off
    (1<<ACI) | // Analog Comparator Interrupt Flag: Clear Pending Interrupt
    (0<<ACIE) | // Analog Comparator Interrupt: Enabled
    (0<<ACIC) | // Analog Comparator Input Capture: Disabled
    (1<<ACIS1) | (0<ACIS0); // Analog Comparator Interrupt Mode: Comparator Interrupt on Falling Output Edge of comparator, by default comparator output should be high
  */
  interrupts();             // enable all software interrupts
    
  Serial.println("Waiting for stim pulses....");
  Serial.println("Stimulation bout " + String(stim_counter) + " of " + String(Number_of_stim_bouts_required) + " received");
}

void loop() {
  // put your main code here, to run repeatedly:
  ACSR |= (1 << ACI); // Clear pending interrupts
  ACSR |= (1 << ACIE);      // Now enable analog comaprator interrupt
  // Wait for 5 trials
  for (int trial_no = 1; trial_no <= Number_of_stim_bouts_required; trial_no++) {
    while (!stim_recvd) {     // 1st measurement
      digitalWrite(end_of_stim_LED_pin, HIGH);
      delay(100);
      digitalWrite(end_of_stim_LED_pin, LOW);
      delay(100);
    }
    delay(2000);
    stim_recvd = false;
    
    ACSR |= (1 << ACIE); // Need to ensure that sencond stimulus doesn't appear with 2 seconds of first stimulus
    ACSR |= (1 << ACI);  // One trial complete- Now enable analog comparator interrupt and clear pending interrupts
    
  }

  // Turn OFF the stimulator
  digitalWrite(ens_MENU_pin, LOW);
  delay(keypress_interval_very_long);
  digitalWrite(ens_MENU_pin, HIGH);
  digitalWrite(end_of_stim_LED_pin, HIGH);
  Serial.println("End of stimulation."); 
  while(1);
}

ISR(ANALOG_COMP_vect) {
  ACSR |= (0 << ACIE) | (1 << ACI); // Disable analog comparator interrupt until this pin is captured

  if (!stim_recvd){
    stim_recvd = true;
    stim_counter++;
    Serial.println("Stimulation bout " + String(stim_counter) + " of " + String(Number_of_stim_bouts_required) + " received");
  }
  

  /*
  if (stim_counter >= Number_of_stim_bouts_required) {
    // Turn OFF stimulation 
    digitalWrite(end_of_stim_LED_pin, HIGH);
    digitalWrite(ens_MENU_pin, LOW);
    delay(keypress_interval_very_long);
    digitalWrite(ens_MENU_pin, HIGH);
    delay(3000);
    digitalWrite(end_of_stim_LED_pin, LOW);
    Serial.println("End of stimulation.");
  }*/

}
