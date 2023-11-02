#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <SPI.h>
#include "SdFat.h"
#include "STM32TimerInterrupt.h"
#include "config.h"

const char compile_date[] = "Compile date: "__DATE__ " " __TIME__;

// Enable print logs
// #define ENABLEPRINTS 1

// SPI 1
SdFat SD;
File myFile;
#define SPI1_CLOCK_MHZ 8 // 4 // 8 // 16
// static SPIClass mySPI1(SPI_MOSI, SPI_MISO, SPI_SCLK, SPI_CS);
// #define SD1_CONFIG SdSpiConfig(SD_CS_PIN, DEDICATED_SPI, SD_SCK_MHZ(SPI1_CLOCK_MHZ), &mySPI1);

// SPI 2
// SPIClass POT;
// #define SPI2_CLOCK_MHZ 4 // 4 // 8 // 16

// BUFFERS
#define BUFFERS_LENGTH 2048
volatile uint16_t buffer_ping[BUFFERS_LENGTH];
volatile uint16_t buffer_pong[BUFFERS_LENGTH];
volatile uint16_t *buffer_read_adc = buffer_ping;
volatile uint16_t *buffer_for_writing_sdcard = buffer_pong;

volatile uint32_t buffers_index = 0;
volatile uint32_t buffers_size = 0;

// PARAMETERS
SampleConfig gParameters = {400, 125};

// GLOBAL FLAGS

volatile bool gBufferIsFull = false;
volatile bool gBufferReadyToStore = false;
volatile bool gAllsamples_collected = false;

#ifdef FINAL
STM32Timer ITimer(TIM2);
#else
STM32Timer ITimer(TIM3);
#endif
// ISR
void TimerHandler(){ 
  //if(buffers_size < gParameters.sample_size){
    if(!gBufferIsFull){
      uint16_t adc_value = analogRead(PIN_ADC);
      buffer_read_adc[buffers_index] = adc_value;
      buffers_index++;
      buffers_size++;

      if(buffers_index >= BUFFERS_LENGTH) gBufferIsFull = true; // set global flag  
    }
  //}else{
  //  gAllsamples_collected = true;
  //}
  // digitalWrite(LED_2,!digitalRead(LED_2));
}

// SETUP
void setup() {
  // config pins
  configPinSPI();
  configPinCMD();
  digitalWrite(STM_ESP_CMD1, LOW);

  analogReadResolution(12); 

  analogWriteFrequency(1); // Set the PWM frequency to 1 Hz 
  pinMode(LED_1, OUTPUT);
  analogWrite(LED_1, 127); // Generate PWM signal with 50% duty cycle (127/255)
  
  pinMode(LED_2, OUTPUT); // indicates the execution of the interrupt (Led PC13)
  
  // config USART 
  Serial.begin(9600);
  MYPRINT("HI, GIMPSI\n");
  MYPRINT("SPI Clock: %d\n", SPI1_CLOCK_MHZ);  
  MYPRINT("%s\n", compile_date);

  // MYPRINT("Creating subfolders\n");
  // SD.mkdir(SAMPLE_DIRECTORY);
  // SD.mkdir(COMPILE_DATE_DIRECTORY);

  // stores the compile_date

  if(initSPI()) {
    // if (SD.exists("/config/CompileSTM.txt")) {
    //   MYPRINT("\nFile exists, deleting it...\n");
    //   if (SD.remove("/config/CompileSTM.txt")) {
    //     MYPRINT("CompileSTM.txt was removed\n");
    //   }
    //   else {
    //     MYPRINT("CompileSTM.txt was NOT removed\n");
    //   }
    //   MYPRINT("File deleted.\n");
    // }
    
    MYPRINT("\nCreating file\n");

    // O_TRUNC to trunc the existing file to length zero (equivalent to overwriting)
    myFile = SD.open("CompileSTM.txt", FILE_WRITE | O_TRUNC);  
    if (myFile) {
      myFile.print(compile_date);
      myFile.close();
    }
    else {
      MYPRINT("Failed to open file to write compile date");
    }
  } else {
    MYPRINT("Restarting STM");
    NVIC_SystemReset();    
  }

#ifdef FINAL
  if (!initPOT()) {
    MYPRINT("Restarting STM");
    NVIC_SystemReset();
  }
#endif
  disableSPI();
  // config Interrupt      
  ITimer.attachInterruptInterval(gParameters.sample_period, TimerHandler);
  ITimer.stopTimer();
  
  MYPRINT("LEAVING SETUP\n");
}

void loop() {
  // Receiving parameters and timestamp by USART (s512p125-/samples/AAAAMMDD_hhmmss.txt)
  static char bufferRX[50];
  static int i=0;
  static int initSample = 0;
  
  while(Serial.available()>0) {
    char c_rx = Serial.read();
    bufferRX[i] = c_rx;
    MYPRINT("%c", bufferRX[i]);
    
    //wait a special char('#') to indentify end of string or lehgnth of vector
    if( '#'==c_rx || i==(sizeof(bufferRX)-1)) {
      bufferRX[i]='\0';
      i=0;
      initSample = 1; // set
      MYPRINT("\nReceived: %s\n",bufferRX);
      break;
    }else{
      i++;
    }
    analogWriteFrequency(10); // Set the PWM frequency to 10 Hz  
    analogWrite(LED_1, 127);
  }

  if(initSample){
    // DEFINING PARAMETERS AND TIMESTAMP

    if (!initSPI()) {
      MYPRINT("Restarting STM");
      NVIC_SystemReset();
    }
    
    MYPRINT("\nDEFINING PARAMETERS AND TIMESTAMP\n");
    digitalWrite(STM_ESP_CMD1, HIGH);
    analogWrite(LED_1, 127);        
    initSample = 0; // reset

   // check for new parameters    
    String parameter_def = parameters_str(gParameters.sample_size, gParameters.sample_period);
    MYPRINT("\nCurrent Parameters: %s\n", parameter_def.c_str());
    if(hasNewParameters_str(bufferRX, parameter_def)){
      updateParameters_str(bufferRX);      
      ITimer.attachInterruptInterval(gParameters.sample_period, TimerHandler);
      ITimer.stopTimer();
      MYPRINT("Parameters updated successfully!\n");
      String newParameter = parameters_str(gParameters.sample_size, gParameters.sample_period);
      MYPRINT("Updated Parameters: %s\n", newParameter.c_str());
    }

    // check for new timestamp
    String dir = parseTimestamp_str(bufferRX);
    MYPRINT("\nDIR: %s\n",dir.c_str());

    // store parameters samples
    storeParameters_str(dir, &(gParameters));    
    
    // STARTING SAMPLING  
    MYPRINT("\nSTARTING SAMPLING\n");
    ITimer.restartTimer();
    unsigned long tstart; 
    unsigned long tend; 

    analogWriteFrequency(5); // Set the PWM frequency to 5 Hz
    analogWrite(LED_1, 127);

    tstart = micros(); 
    volatile int tmp_how_many_samples_to_store = 0;
    while(1) {

      //if(gAllsamples_collected){
      //  ITimer.stopTimer();                    
      //}

      // buffer full
      //if(gBufferIsFull || gAllsamples_collected){
      if(gBufferIsFull){    
        // the change of the buffers and the reset of the index must be done 
        // as soon as possible because the interrupt are still running
        // change the buffer pointer 
        if(buffer_read_adc == buffer_ping){
            buffer_for_writing_sdcard = buffer_ping;
            buffer_read_adc = buffer_pong;
        }else{
            buffer_for_writing_sdcard = buffer_pong;
            buffer_read_adc = buffer_ping;
        }
        
        
        tmp_how_many_samples_to_store = buffers_index;
        MYPRINT("[if(gBufferIsFull)] buffers_index = %d | tmp_how_many_samples_to_store = %d | buffer_size = %d | sampleSize: %d\n", buffers_index, tmp_how_many_samples_to_store, buffers_size, gParameters.sample_size);        
        buffers_index = 0; // reset 


        gBufferIsFull = false; // reset global flag
        gBufferReadyToStore = true; // set global flag 
      }         
      
      // write to SD card  
      if(gBufferReadyToStore){
        // if(!tmp_how_many_samples_to_store)  tmp_how_many_samples_to_store = buffers_index;     
        
        gBufferReadyToStore = false; // set global flag
        
        MYPRINT("buffers_size = %d\n", buffers_size);
        
        MYPRINT("buffers_index = %d | tmp_how_many_samples_to_store = %d\n", buffers_index, tmp_how_many_samples_to_store);
        
        if(buffers_size <= gParameters.sample_size){
          storeSamples_str(dir, buffer_for_writing_sdcard,BUFFERS_LENGTH);
        }
        else{
          // interruption is not necessary anymore. All samples were  collected
          ITimer.stopTimer(); 
          int n_samples_remaining = gParameters.sample_size % BUFFERS_LENGTH;

          MYPRINT("last samples to store = %d\n", n_samples_remaining);

          storeSamples_str(dir, buffer_for_writing_sdcard,n_samples_remaining);
          
          // then, we quit the while(1)
          break;
        }
        // tmp_how_many_samples_to_store = 0;
      }

      //if(gAllsamples_collected){
      //  gAllsamples_collected = false;
      //  break; //end the loop
      //}
                   
    } //while    

    digitalWrite(STM_ESP_CMD1, LOW);    
    tend = micros();    
    MYPRINT("Time to sample and store the data: %dus\n", tend-tstart);
    MYPRINT("Buffers Size: %d\n", buffers_size);
    ITimer.stopTimer();
  // #ifndef FINAL
    disableSPI();
  // #endif
    buffers_index = 0; // reset 
    buffers_size = 0; // reset
    
    resetSamplingFlags();     
    analogWriteFrequency(1); // Set the PWM frequency to 1 Hz
    analogWrite(LED_1, 127);
  }
}

// FUNCTIONS
String parseTimestamp_str(String received_str){
  String timestamp;
  timestamp = received_str.substring(received_str.indexOf("-") + 1);
  MYPRINT("newString: %s", timestamp.c_str());
  return timestamp;
}

int storeSamples_str(String dir, volatile uint16_t* w, int NsamplesToStore){  
  // while(!initSPI());
  uint8_t* w_8bits_ptr = (uint8_t*) w;
  myFile = SD.open(dir, FILE_WRITE);
  
  if (myFile){
    myFile.write(w_8bits_ptr, NsamplesToStore*2);
    
    myFile.close();            
  }else{
    MYPRINT("File is failed to open\n");
    MYPRINT("SAMPLE FAIL\n");
    return SAMPLE_FAIL;
  }
  // disableSPI();
  // MYPRINT("STORE SAMPLES OK\n");
  return SAMPLE_OK;
}

int storeParameters_str(String dir,  SampleConfig* parameters){  
  myFile = SD.open(dir, FILE_WRITE);
  
  if (myFile){
    myFile.write(&(parameters->sample_period), 2);
    myFile.write(&(parameters->sample_size), 4);    
    myFile.close();            
  }else{
    MYPRINT("File is failed to open during pararameters storage\n");
    MYPRINT("SAMPLE FAIL\n");
    return SAMPLE_FAIL;
  }
  return SAMPLE_OK;
}

//check for new parameters
bool hasNewParameters_str(String received_str, String current_parameters){
  int pos = received_str.indexOf(current_parameters);
  // MYPRINT("hasNewParameters_str Index = %d\n",pos);
  return (pos == -1);
}

// Generates the default string format of the parameters
String parameters_str(int sample_size, int sample_period){
  String parameters_str;
  char s[10]; // TODO? pensar qual seria a apacidade maxima
  char p[10];
  char tmpstr[sizeof(s)+sizeof(p)+10];  
  sprintf(tmpstr,"s%dp%d",sample_size,sample_period);  
  return tmpstr;
}

// assigns the new parameter values, given the default string format
bool updateParameters_str(String receivedParameters){
  int sample_size_start = receivedParameters.indexOf('s') + 1;
  int sample_size_end = receivedParameters.indexOf('p', sample_size_start);
  int sample_period_start = sample_size_end + 1;
  int sample_period_end = receivedParameters.indexOf(SEPARATOR, sample_period_start);

  if (sample_size_start > 0 && sample_size_end > 0 && sample_period_start > 0 && sample_period_end > 0) {
    String sample_size_str = receivedParameters.substring(sample_size_start, sample_size_end);
    String sample_period_str = receivedParameters.substring(sample_period_start, sample_period_end);
    // gParameters.sample_size = (sample_size_str.toInt() > BUFFER_MAX ? BUFFER_MAX : sample_size_str.toInt());
    gParameters.sample_size = sample_size_str.toInt();
    gParameters.sample_period = sample_period_str.toInt();
    return true;
  }
  return false;
}

bool initSPI(){
  MYPRINT("[initSPI] Initializing SD card...\n");
  //if(!SD.begin(SD1_CONFIG)){
  if (!SD.begin(SPI_CS, SD_SCK_MHZ(SPI1_CLOCK_MHZ))){
    MYPRINT("[initSPI] initialization failed!\n");
    //while (1);
    return false;    
  }
  MYPRINT("[initSPI] initialization done.");  
  return true;
}

bool initPOT(){
  // MYPRINT("Initializing SPI POT ...\n");  
  // if (!POT.begin(STM_PB12_NSS2, SD_SCK_MHZ(SPI2_CLOCK_MHZ))){
  //   // MYPRINT("initialization failed!\n");
  //   return false;    
  // }
  // MYPRINT("initialization done.");  
  return true;
}

void disableSPI(){
  SD.end();
  SPI.end();
  digitalWrite(SPI_MOSI, HIGH);
  digitalWrite(SPI_SCLK, HIGH);
  digitalWrite(SPI_CS, HIGH);
}

void configPinSPI(){
  pinMode(SPI_MISO, INPUT);
  pinMode(SPI_MOSI, OUTPUT);
  pinMode(SPI_SCLK, OUTPUT);
  pinMode(SPI_CS, OUTPUT);
  digitalWrite(SPI_MOSI, LOW);
  digitalWrite(SPI_SCLK, LOW);
  digitalWrite(SPI_CS, LOW);
}

void configPinCMD(){ 
  pinMode(ESP_STM_CMD1, INPUT); // digitalRead(ESP_STM_CMD1);
  pinMode(STM_ESP_CMD1, OUTPUT); // digitalWrite(STM_ESP_CMD1, HIGH);
}

void configPinAdjustFreqResp(){
  pinMode(STM_PB15_MOSI2, OUTPUT); 
  pinMode(STM_PB13_SCLOK2, OUTPUT);  
  pinMode(STM_PB12_NSS2, OUTPUT);  
  pinMode(STM_PB1, OUTPUT);  
  pinMode(STM_PB0, OUTPUT);  
}

void resetSamplingFlags(){
    gBufferIsFull = false;
    gBufferReadyToStore = false;
    gAllsamples_collected = false; 
    // MYPRINT("Flags: gBufferIsFull = %d | gBufferReadyToStore = %d | gAllsamples_collected = %d\n", gBufferIsFull, gBufferReadyToStore, gAllsamples_collected);
}


