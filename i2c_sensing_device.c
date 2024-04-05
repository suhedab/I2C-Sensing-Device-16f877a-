#include <16F877A.h>
#device ADC=16
#use delay(clock=20000000)
#use i2c(MASTER, SDA=PIN_C4, SCL=PIN_C3, FAST=100000)
#use rs232(baud=9600, UART1, ERRORS)

#define LM75A_ADDRESS 0x90

#define SHT21_I2C_ADDRESS 0b1000000
#define SHT21_HUMIDITY_COMMAND 0xE5

#define CCS811_I2C_ADDRESS 0X5A
#define CCS811_ALG_RESULT_DAT 0x02

#define FILTER_SIZE 5         // Size of the moving median filter
float data;

void read_SensorData(int16 address) {
   int16 tempH, tempL, temp;
   float temperature;
   
   int16 humH, humL;
   float humidity;
   
   int16 co2_value;
   int data_co2[2];
   
   if (address == LM75A_ADDRESS)       // TEMPERATURE MEASUREMENT
   {
      i2c_start();
      i2c_write(LM75A_ADDRESS | 0x01);    // I2C reading address
      
      tempH = i2c_read();                 // MSB bits
      tempL = i2c_read();                 // lsb bits
      i2c_stop();
      
      temp = make16(tempH, tempL);    //Combine MSB and LSB as 16-bit data
      
      // if Sign bit is 1 temperature value is negative, 
      //if it is not temperature value is positive.
      // Formula of the temp value changing according either negative or positive
      if (temp & 0x8000) 
      {
         temp |= 0xF000;                             // set the sign bit       
         int16 complement = ~temp + 1;               // 2's complement        
         temperature = -(complement * 0.125)/32;    // From datasheet         
      }else 
      {
         temperature = (temp >> 5) * 0.125;          // From datasheet
      }
      
      data = temperature;

   }else if(address == SHT21_I2C_ADDRESS)       // HUMIDITY MEASUREMENT
   {
       i2c_start();       
       i2c_write(SHT21_I2C_ADDRESS << 1);       // Write
       i2c_write(SHT21_HUMIDITY_COMMAND);       // Humidity measurement command from datasheet
    
       i2c_start();
       i2c_write((SHT21_I2C_ADDRESS << 1) | 1);       // Read
       
       humH = i2c_read();        // MSB
       humL = i2c_read();        // LSB
       i2c_stop();
   
       humidity = make16(humH, humL);          //Combine MSB and LSB as 16-bit data
       
       humidity = -6.0 + (125.0 * (humidity / 65536.0) );    // humidity formula
   
       data = humidity;
       
   }else if (address == CCS811_I2C_ADDRESS)       // CO2 MEASUREMENT
   {
      i2c_start();
      i2c_write(CCS811_I2C_ADDRESS);            // Write
      i2c_write(CCS811_ALG_RESULT_DAT);         // only register contains the calculated eCO2
      
      i2c_start();
      i2c_write(CCS811_I2C_ADDRESS | 0x01);     // Read
      
      data_co2[0] = i2c_read(0);                    // first byte
      data_co2[1] = i2c_read(0);                    // second byte
      i2c_stop();

      co2_value = make16(data_co2[0], data_co2[1]);      // combine two data to make 16-bit data

      data = co2_value; 
   }
   
}

float filter(){
   
   int16 filtered_data [FILTER_SIZE];
   
   filtered_data[FILTER_SIZE-1] = data;      // filtered_data[4] = data
   
   for(int i = FILTER_SIZE-1; i>0; i--)
   {
       if(filtered_data[i] < filtered_data[i - 1]) 
       {
         data = filtered_data[i];
         filtered_data[i] = filtered_data[i - 1];
         filtered_data[i - 1] = data;
       }
   }
   
   data = filtered_data[FILTER_SIZE / 2];      // median
   
   return data;
}

void main() 
{
   float temperature, humidity, co2Value;
   set_tris_b(0xFF);
   
   while(TRUE) {
      
      // Measuring temperature from LM75A
      read_SensorData(LM75A_ADDRESS);
      temperature = filter();
      delay_ms(100);
      
      // Measuring humidity from SHT21
      read_SensorData(SHT21_I2C_ADDRESS);
      humidity = filter();
      delay_ms(100);
      
      // Measuring CO2 from CCS811
      read_SensorData(CCS811_I2C_ADDRESS);
      co2Value = filter();
      
      // Print outputs to RS232
      printf("\fLM75A Temperature: %.2f C\r\n", temperature);
      printf("\r\nSHT21 Humidity: %.2f %%\r\n", humidity);
      printf("\r\nCCS811 CO2: %.1f ppm\r\n", co2Value);
      
      delay_ms(1000); // a second delay
   }
}

