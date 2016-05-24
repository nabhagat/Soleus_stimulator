#include <EEPROMex.h>

//int address = 0;
const int maxAllowedWrites = 100;
const int Default_No_of_stim_bouts = 10;
const double Stim_voltage_mult_factor = 15.017;
const double dVolt_per_dIncrement = 2.5;

void setup()
{
    Serial.begin(115200);  
    Serial.println();
    Serial.println("-----------------------------------");     
    Serial.println("Following adresses have been issued");     
    Serial.println("-----------------------------------");      
    Serial.println("begin adress \t\t\t Variable");

    // Always get the adresses first and in the same order
    //int addressByte      = EEPROM.getAddress(sizeof(byte));
    //int addressInt       = EEPROM.getAddress(sizeof(int));
    //int addressDouble    = EEPROM.getAddress(sizeof(double));    

    
    // Set maximum allowed writes to maxAllowedWrites. 
    // More writes will only give errors when _EEPROMex_DEBUG is set
    EEPROM.setMaxAllowedWrites(maxAllowedWrites);

    // No. of stim bouts
    int addressInt = EEPROM.getAddress(sizeof(int));
    EEPROM.updateByte(addressInt,Default_No_of_stim_bouts);   
    Serial.print(addressInt);      Serial.print(" \t\t\t No. of stim bouts = "); Serial.println(Default_No_of_stim_bouts);

    // Stim voltage muliplication factor
    int addressDouble    = EEPROM.getAddress(sizeof(double));
    EEPROM.updateByte(addressDouble,Stim_voltage_mult_factor);
    Serial.print(addressDouble);      Serial.print(" \t\t\t Stim volt multX = "); Serial.println(Stim_voltage_mult_factor);

    // Slope of change in Volt per unit change in Increment
    addressDouble    = EEPROM.getAddress(sizeof(double));
    EEPROM.updateByte(addressDouble,dVolt_per_dIncrement);
    Serial.print(addressDouble);      Serial.print(" \t\t\t dVolt_per_dIncrement = "); Serial.println(dVolt_per_dIncrement);

    /*
    Serial.println("Trying to exceed number of writes...");        
    for(int i=1;i<=20; i++)
    {
        if (!EEPROM.writeDouble(address,1000)) { return; }    
    }*/


    
    Serial.println();   
}

void loop() { }
