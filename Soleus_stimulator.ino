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
#include "EEPROMex.h"

#define array_len( x )  ( sizeof( x ) / sizeof( *x ) )

// Define ENS2020 Pin Mappings
const byte ens_ON_pin = 53;
const byte ens_OFF_pin = 49;
const byte ens_MENU_pin = 52;
const byte ens_SEL_pin = 46;
const byte ens_FWD_pin = 50;
const byte ens_BWD_pin = 48;
const byte ens_UP_pin = 51;
const byte ens_DOWN_pin = 47;

// Define microcontroller input/output pin mappings
//const byte stim_received_LED_pin = 27;
const byte trigger_clock_pin = 27; 
const byte end_of_stim_LED_pin  = 31;    // Use to indicate end of trial
const byte AIN1_pin = 5;     // Negative input pin of Analog comparator; Positive pin is internal reference = 1.1V
const byte ledPin = LED_BUILTIN;
int EEG_R128_trigger = 34;
int trigger_duration = 62411; // trigger low for 50 ms; should be > sampling interval (for EEG, sampling interval = 2 ms)

// 3036 - 1Hz; //34286 - 2Hz; //53036 - 5 Hz; //62411 - 20 Hz;//64911 - 100 Hz; // 65411 - 500 Hz  // preload timer 65536-16MHz/256/2Hz
const int trigger_generation_rate = 65411; // 500 Hz sampling with prescalar = 1/256
const int ctc_compare_value = 249;  // 1Khz timer ctc mode, prescalar = 1/64; OCR1A = (16Mhz/prescalar(64)/1KHz) - 1

const int keypress_interval_very_long = 500;
const int keypress_interval_long = 200;
const int keypress_int_short = 150;
volatile boolean stim_recvd = false;
volatile int stim_counter = 0;

// Define EEPROM variables
int Number_of_stim_bouts = 0;
int Number_of_voltage_increments = 0;
double Stim_voltage_mult_factor = 0.00;
double dVolt_per_dIncrement = 0.00;

const char welcome_msg[] PROGMEM = {"--------------Welcome to the Electrical Stimulator Control Interface--------------\n"
"Select one of the two following options.\n"
"1 - Configure stimulator\n"
"2 - Run stimulator"};

char msg_char;
byte user_selection = 0;
boolean valid_user_input = false;
boolean stringComplete = false;
String inputString ="";
int user_input = 0;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);        // Turn on the Serial Port
  
  // reserve 10 bytes for the inputString:
  inputString.reserve(10);
  
  // Configure ENS - Set MENU, SEL, UP and DOWN pins as output and set them to HIGH; rest as input
  pinMode(ens_ON_pin, INPUT_PULLUP);
  pinMode(ens_OFF_pin, INPUT_PULLUP);
  pinMode(ens_FWD_pin, INPUT_PULLUP);
  pinMode(ens_BWD_pin, INPUT_PULLUP);
  
  pinMode(ens_DOWN_pin, OUTPUT);
  digitalWrite(ens_DOWN_pin, HIGH);
  pinMode(ens_UP_pin, OUTPUT);
  digitalWrite(ens_UP_pin, HIGH);
  pinMode(ens_MENU_pin, OUTPUT);
  digitalWrite(ens_MENU_pin, HIGH);
  pinMode(ens_SEL_pin, OUTPUT);
  digitalWrite(ens_SEL_pin, HIGH);

  // Configure Arduino LED, ADC and Analog Comparator pins
  /////Inputs
  pinMode(AIN1_pin, INPUT);

  ////Outputs
  pinMode(trigger_clock_pin, OUTPUT);
  digitalWrite(trigger_clock_pin, HIGH);
  //pinMode(end_of_stim_LED_pin, OUTPUT);
  //digitalWrite(end_of_stim_LED_pin, LOW);
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);
  pinMode(EEG_R128_trigger, OUTPUT);            // Added 8-1-2016  
  digitalWrite(EEG_R128_trigger, HIGH);

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

  // Timer 1 for generating trigger clock @ 100 Hz Added 06-28-2016
  //TCCR1A = 0x00;
  //TCCR1B = 0x00;            // Timer stopped, to start timer - load 1/256 prescalar = 0x04
  //TCNT1 =  trigger_generation_rate;
  //TIMSK1 |= (1 << TOIE1);   // enable timer overflow interrupt

  // Timer 1 in CTC mode
  TCCR1A = 0x00;
  TCCR1B = 0x00;
  OCR1A = ctc_compare_value;
  TIMSK1 |= (1 << OCIE1A);   // Enable ctc interrupt

  //Timer 2 in Fast PWM
  pinMode(9,OUTPUT); // OC2B pin
  pinMode(10,OUTPUT); // OC2A pin
  TCCR2A |= (1 << COM2A1) | (0 << COM2A0) | (1 << COM2B1) | (0 << COM2B0) | (1 << WGM21) | (1 << WGM20);
  TCCR2B |= (0 << WGM22) | (1 << CS22) | (0 << CS21) | (0 << CS20);
  OCR2A = 180;
  OCR2B = 50;  

  // Timer 3 controls the stimulus-onset trigger
  TCCR3A = 0;
  TCCR3B = 0x00;            // Timer stopped
  TCNT3 =  trigger_duration;    
  TIMSK3 |= (1 << TOIE3);   // enable timer overflow interrupt
  
  interrupts();             // enable all software interrupts

  //Read EEPROM variables to initialize parameters
   int EEPROM_address_Number_of_stim_bouts = EEPROM.getAddress(sizeof(int));
   Number_of_stim_bouts = EEPROM.readInt(EEPROM_address_Number_of_stim_bouts);
   int EEPROM_address_Stim_voltage_mult_factor = EEPROM.getAddress(sizeof(double));
   Stim_voltage_mult_factor = EEPROM.readDouble(EEPROM_address_Stim_voltage_mult_factor);
   int EEPROM_address_dVolt_per_dIncrement = EEPROM.getAddress(sizeof(double));
   dVolt_per_dIncrement = EEPROM.readDouble(EEPROM_address_dVolt_per_dIncrement);

  // Added to print long welcome msg
  Serial.println();
  for (int k = 0; k < strlen_P(welcome_msg); k++)
  {
    msg_char =  pgm_read_byte_near(welcome_msg + k);
    Serial.print(msg_char);
  }
  Serial.println();  
  Serial.println();
  while (Serial.available() == 0); // Wait for user input
  user_input = Serial.parseInt();  // Only accepts integers
  Serial.print("You entered : ");
  Serial.print(user_input);

  switch (user_input) {
    case 1: 
            Serial.println(F(", Configure stimulator"));            
            Serial.println(F("This is the current configuration:"));
            Serial.print(F("No. of stim bouts = ")); Serial.println(Number_of_stim_bouts);
            Serial.print(F("Stim volt multX = ")); Serial.println(Stim_voltage_mult_factor);
            Serial.print(F("dVolt_per_dIncrement = ")); Serial.println(dVolt_per_dIncrement);
            
            Serial.print(F("Do you want to change the number of stimulation bouts? (y/n):"));
            user_selection = read_user_response(); Serial.println(char(user_selection));
            if (user_selection == 'y' || user_selection == 'Y'){
                while(!valid_user_input){
                    Serial.print(F("Enter number of stimulation bouts required (5-150): "));
                    while (Serial.available() == 0); // Wait for user input
                    user_input = Serial.parseInt();  // Only accepts integers
                    if (user_input > 5 && user_input < 150){
                      Number_of_stim_bouts = user_input;
                      Serial.println(Number_of_stim_bouts);
                      EEPROM.updateByte(EEPROM_address_Number_of_stim_bouts,Number_of_stim_bouts);
                      valid_user_input = true;
                    }
                    else{
                      Serial.println(F("Invalid input !!"));
                    }
                }
                valid_user_input = false;             
            }
            Serial.print(F("Stimulator configuration complete"));
            
    case 2: 
            Serial.println(F(", Ready to run stimulator"));
            while(!valid_user_input){
                   Serial.println(F("Enter number of voltage increments (10-40): "));
                   while (Serial.available() == 0); // Wait for user input
                   user_input = Serial.parseInt();  // Only accepts integers
                   if (user_input >= 10 && user_input <= 41){
                      Number_of_voltage_increments = user_input;
                      Serial.println(Number_of_voltage_increments);
                      valid_user_input = true;
                    }
                   else{
                      Serial.println(F("Invalid input !!"));
                    }
               }
            valid_user_input = false;
                
            //Turn ON the stimulator
            Serial.print(F("Starting stimulator and trigger clock. Adjusting voltage"));
            //TCCR1B = 0x04; // Generate trigger clock using TIMER 1 - Added 06-28-2016
            TCCR1B |= (1 << WGM12)|(1 << CS11)|(1 << CS10);
            TCNT1  = 0; // Initialize counter
            TIMSK1 |= (1 << OCIE1A);   // enable ctc interrupt
            
            digitalWrite(ens_SEL_pin, LOW);
            delay(keypress_interval_very_long);
            digitalWrite(ens_SEL_pin, HIGH);
            delay(keypress_interval_very_long);
        
            // Increase by required number of increments immediately
            for (int inc = 1; inc <= Number_of_voltage_increments; inc++) {
              Serial.print('.');
              digitalWrite(ens_UP_pin, LOW);
              delay(keypress_int_short);
              digitalWrite(ens_UP_pin, HIGH);
              delay(keypress_int_short);
            }
            Serial.println();
            Serial.println(F("Starting counter")); 
            break;
    default:
            Serial.println(F(", Incorrect Input!! Reset Controller")); // Strangely, three exclamation marks breaks the Mega2560 bootloader
  }
}

void loop() {
  // put your main code here, to run repeatedly:
  ACSR |= (1 << ACI); // Clear pending interrupts
  ACSR |= (1 << ACIE);      // Now enable analog comaprator interrupt
  // Wait for 5 trials
  for (int trial_no = 1; trial_no <= Number_of_stim_bouts; trial_no++) {
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
    digitalWrite(EEG_R128_trigger,LOW);    // Set trigger
    stim_recvd = true;
    stim_counter++;
    //Serial.println("Stimulation bout " + String(stim_counter) + " of " + String(Number_of_stim_bouts) + " received");
    Serial.println(String(stim_counter));
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

//ISR(TIMER1_OVF_vect) {
  //TCNT1 = trigger_generation_rate;
  
ISR(TIMER1_COMPA_vect){
  digitalWrite(trigger_clock_pin,LOW);
  //delay(1);
   delayMicroseconds(100);
  digitalWrite(trigger_clock_pin,HIGH);
}

char read_user_response(){
  boolean valid_response = false;
  char serial_data;

  while (!valid_response){
    while (Serial.available() == 0); // Wait for user input
    serial_data = Serial.read();
    switch (serial_data){
      case 'y': case 'Y': case 'n': case 'N': 
              valid_response = true;
              break;
      case '\n': case '\r':
              valid_response = false;
              break;
      default: 
              Serial.println(F("Invalid input! Enter again: "));
              break;
    }
  }  
}

/*
  SerialEvent occurs whenever a new data comes in the
 hardware serial RX.  This routine is run between each
 time loop() runs, so using delay inside loop can delay
 response.  Multiple bytes of data may be available.
 */
/*void serialEvent() {
  while (Serial.available()) {
    // get the new byte:
    char inChar = (char)Serial.read();
    // add it to the inputString:
    inputString += inChar;
    // if the incoming character is a newline, set a flag
    // so the main loop can do something about it:
    if (inChar == '\n') {
      stringComplete = true;
    }
  }
}*/

ISR(TIMER3_OVF_vect){
  TCNT3 =  trigger_duration;
  TCCR3B = 0x00;    // Stop Timer
  digitalWrite(EEG_R128_trigger,HIGH);      // reset trigger
}


