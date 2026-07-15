//THE DEFAULT SETUP FOR THIS CODE PROVIDES 1200 ADC SAMPLES PER SECOND FOR 5 ANALOG INPUTS (A0-A4)
//THE OUTPUT IS THE DIGITIZED SIGNAL (0-16383; 14-BIT)
//THE OPERATING BAUDRATE IS 500000
#include <FspTimer.h>
#include <math.h>

// EDITABLE VARIABLE DEFINITIONS

char arduino_name[] = "Ardy"; // Set this to something specific that will clearly indicate the purpose of this Arduino

//  Variables for ADC data collection and communication
const uint16_t target_adc_frequency_in_Hz = 1200; // Set this to the desired ADC sampling frequency (adc_pin_count * target_adc_frequency_in_Hz ≤ 7200)
// NOTE: Sampling frequency must be AT LEAST twice the highest frequency of interest in the signal.
const uint8_t adc_pins[] = {A0, A1, A2, A3, A4}; // Include all pins needed for ADC data collection (e.g., all the bioamplifiers connected to analog ports)
const uint16_t adc_reporting_factors[] = {1, 1, 1, 1, 1}; // This is used to reduce the frequency at which ADC values are reported by some factor (one is MAX, e.g., 1200 Hz)
                                                          // Zero will be used to imply no reporting of the value (i.e., the value is used for on-board calculations only))
const uint16_t print_delay_cycle_count = target_adc_frequency_in_Hz / 10; // How much data backlog to build up before reporting values (number of ADC iterations) — useful if you want to process some amount of data onboard before reporting)
char* adc_pin_ids[] = { "A0",
                        "A1",
                        "A2",
                        "A3",
                        "A4"}; // These are identifiers for each of the ADC pins being used; e.g., flexor digitorum superficialis — MUST BE INCLUDED FOR ALL PINS (even those not sent to the serial port)

//  Variables for other data communication
char* integer_output_ids[] = {}; // Include string identifiers for any additional numerical outputs
                                 // These will be transmitted as signed integers (up to 11 characters long, including signs)
char* float_output_ids[] = {}; // Include string identifiers for any additional numerical outputs
                               // These will be transmitted in engineering format with 4 sig figs (up to 11 characters long, including signs)
                               // E.g., -1234000000000 --> "-1.234e12"

// OTHER VARIABLE DEFINITIONS (do not edit these unless you really know what you are doing)
const uint8_t adc_resolution = 14;
const uint16_t buffer_multiplier = 36 + print_delay_cycle_count; // The number of data rows to store in buffer
const uint8_t adc_pin_count = sizeof(adc_pins) / sizeof(adc_pins[0]); // The number of adc pins
uint16_t adc_reporting_counters[adc_pin_count];
volatile uint8_t adc_pin_index = 0; // The index of the current adc pin to read
const uint16_t adc_value_count = adc_pin_count * buffer_multiplier;
volatile int16_t adc_value_buffer[adc_value_count]; // This is a data buffer for the ADC values prior to printing to the Serial port
volatile uint16_t adc_values_write_index = 0; // This is the index of the next element to write

const uint8_t integer_output_count = sizeof(integer_output_ids) / sizeof(integer_output_ids[0]); // The number of integer outputs
const uint8_t float_output_count = sizeof(float_output_ids) / sizeof(float_output_ids[0]); // The number of float outputs

const uint8_t timestamp_value_count = buffer_multiplier;
volatile unsigned long timestamp_value_buffer[timestamp_value_count];
volatile unsigned long start_time = 0;

volatile bool delay_print = true;
const uint8_t print_buffer_multiplier = 3; // Maximum number of cycles to print to serial at once — may need tweaking
char print_buffer[(6*adc_pin_count + 12*(integer_output_count + float_output_count + 1)) * print_buffer_multiplier];
volatile uint16_t print_cycle = 0; // This is the cycle (row/iteration/whatever) of the next elements to print

FspTimer adc_timer;

// EDITABLE FUNCTION DEFINITIONS
int getIntegerOutputFor(uint16_t data_row_index, uint8_t integer_output_index){
  // Edit this function to generate the proper integer value based on the provided indices
  switch (integer_output_index) {
    case 0:
      return 0;
      break;
    default:
      return 0;
      break;
  }
}

float getFloatOutputFor(uint16_t data_row_index, uint8_t float_output_index){ // The inputs are the relevant float output index and the (presumably) relevant adc values
  // Edit this function to generate the proper float value based on the  provided indices
  // Note, you can quickly overload the Arduin here
  // — If your output is behaving funny (e.g., other values are starting to get distorted), you can:
  //   - Reduce the sampling rate (don't go below 1000 Hz for EMG or 200 Hz for ECG)
  //   - Reduce the number of ADC readouts (e.g., reduce the number of muscles being recorded)
  //   - Reduce the reporting rate (i.e., run the calculation less frequently)
  //   - Reduce the ADC resolution (allows for faster sampling at the cost of data resolution)
  switch (float_output_index) {
    case 0:
      return 0;
      break;
    default:
      return 0;
      break;
  }
}

// SOME FUNCTIONS YOU MAY WANT TO IMPLEMENT IN YOUR EDITABLE DEFINITIONS (do not edit these unless you know what you are doing)
float getMovingMean(uint16_t data_row_index, uint8_t data_column_index, uint8_t half_width){
  // The half-width must be less than 255 and should be less than the ADC print delay cycle count (cycles delayed before printing)
  float sum = 0;
  int value_count = 0;
  for (int i = -half_width; i < half_width; i++){
    int row = (buffer_multiplier + data_row_index + i)%buffer_multiplier;
    int value = adc_value_buffer[row * adc_pin_count + data_column_index];
    if (value >= 0){
      sum += value;
      value_count++;
    }
  }
  return sum / value_count;
}

const int16_t midline = pow(2,adc_resolution-1);
float getMidlineRMS(uint16_t data_row_index, uint8_t data_column_index, uint8_t half_width){
  // The half-width must be less than 255 and should be less than half the buffer multiplier
  int sum = 0;
  uint8_t value_count = 0;
  for (int i = -half_width; i < half_width; i++){
    uint16_t row = (buffer_multiplier + data_row_index + i)%buffer_multiplier;
    int16_t value = adc_value_buffer[row * adc_pin_count + data_column_index];
    if (value >= 0){
      value -= midline;
      sum += value*value;
      value_count++;
    }
  }
  return sqrt(sum / value_count);
}

float getMeanCenteredRMS(uint16_t data_row_index, uint8_t data_column_index, uint8_t half_width){
  // The half-width must be less than 255 and should be less than half the buffer multiplier
  // This can take a lot of processing power, since it needs a reasonably large half-width for a stable mean estimate
  // It is not recommended if you are collecting ADC data from more than one channel
  float sum = 0;
  int value_count = 0;
  float moving_mean = getMovingMean(data_row_index, data_column_index, half_width);
  for (int i = -half_width; i < half_width; i++){
    int row = (buffer_multiplier + data_row_index + i)%buffer_multiplier;
    int value = adc_value_buffer[row * adc_pin_count + data_column_index];
    if (value < 0){
      return NAN;
    }
    value -= moving_mean;
    sum += value*value;
    value_count++;
  }
  return sqrt(sum / value_count);
}

// OTHER FUNCTION DEFINITIONS (do not edit these unless you really know what you are doing)

// Convert float to char array (need char array input with at least 12 characters, 11 for float + 1 for null terminator)
void floatToCharArray(float number, char* char_buffer){
  if (isnan(number)){
    strcpy(char_buffer,"nan");
    return;
  }
  int exponent = 0;
  float mantissa = abs(number);
  while (mantissa > 10){
    mantissa /= 10;
    exponent++;
  }
  if (number < 0){
    mantissa = -mantissa;
  }
  dtostrf(mantissa,4,3,char_buffer);
  sprintf(char_buffer+strlen(char_buffer),"e%d",exponent);
}

// Reset relevant variables
void Reset(){

  // Initialize reporting variables
  for (uint8_t i=0; i<adc_pin_count; i++){
    adc_reporting_counters[i] = 0; // Set pin counter to 0
  }

  // Initialize ADC buffer values
  for (uint16_t i=0; i<adc_value_count; i++){
    adc_value_buffer[i] = -1; // Set buffer values to -1; this lets us know if an element has yet to be written to, since ADC values are non-negative
  }

  // Initialize indices
  adc_pin_index = 0;
  adc_values_write_index = 0;
  delay_print = true;
  print_cycle = 0;
  start_time = micros();
}

// Callback function to be executed by the timer interrupt
void onTimerElapsed(timer_callback_args_t* p_args) {
  adc_value_buffer[adc_values_write_index] = analogRead(adc_pins[adc_pin_index]); // current ADC value

  if (adc_pin_index == 0){
    timestamp_value_buffer[adc_values_write_index/adc_pin_count] = micros()-start_time; // current timestamp
  }

  // Shift to next pin
  adc_pin_index++;
  adc_pin_index%=adc_pin_count;
  analogRead(adc_pins[adc_pin_index]); // switches pin prior to next read to account for settling time
  
  // Increment buffer write index
  adc_values_write_index++;
  adc_values_write_index%=adc_value_count;

  if (delay_print && adc_values_write_index > adc_pin_count * print_delay_cycle_count){
    delay_print = false;
  }
}

bool setTimer(float rate)
{
  uint8_t timer_type = GPT_TIMER;
  int8_t tindex = FspTimer::get_available_timer(timer_type);
  if (tindex < 0){
    tindex = FspTimer::get_available_timer(timer_type, true);
  }
  if (tindex < 0){
    return false;
  }
  FspTimer::force_use_of_pwm_reserved_timer();
  if(!adc_timer.begin(TIMER_MODE_PERIODIC, timer_type, tindex, rate, 0.0f, onTimerElapsed)){
    return false;
  }
  if (!adc_timer.setup_overflow_irq()){
    return false;
  }
  if (!adc_timer.open()){
    return false;
  }
  return true;
}

bool loadPrintBuffer()
{
  // Initialize string
  print_buffer[0] = '\0';

  // Fill print buffer
  char temp_buffer[12]; // Temporary buffer (max length for any of the possible string outputs is 12 characters)
  uint16_t adc_values_write_cycle = adc_values_write_index / adc_pin_count; // Determine cycle for next write action
  int16_t printable_cycle_count = adc_values_write_cycle - print_cycle - print_delay_cycle_count; // The last component keeps a gap present for data pre-processing
  if (printable_cycle_count < 0){
    printable_cycle_count += buffer_multiplier; // Write cycle has overflowed and reset before print cycle — adjust cycle delta to compensate
  }
  printable_cycle_count = (printable_cycle_count > print_buffer_multiplier) ? print_buffer_multiplier : printable_cycle_count; // Cap cycle delta to print buffer size
  uint16_t i_max = print_cycle + printable_cycle_count; // Set iteration limit for print cycles
  while (print_cycle < i_max) {
    uint16_t i = print_cycle % buffer_multiplier; // Account for print cycle overflow without messing up while loop
    for (uint8_t j=0; j<adc_pin_count; j++) {
      // Get pin index for value buffer index — this lets assess if we should include the value in our print buffer (i.e., should it be reported)
      if (adc_reporting_factors[j] == 0){
        continue; // This pin value is never reported and should not be included in the print buffer at all
      }
      
      // Update reporting counter and check for relevance of iteration for reporting specific pin index value
      adc_reporting_counters[j]++;
      if (adc_reporting_counters[j] == adc_reporting_factors[j]){
        // Reset counter
        adc_reporting_counters[j] = 0;
        // Report ADC value for current pin in current data row
        sprintf(temp_buffer, "%d", adc_value_buffer[i*adc_pin_count + j]); // Convert 14-bit integer to char array
      } else {
        // Report NaN — makes it clear that the pin is not reporting data here
        strcpy(temp_buffer,"nan"); // This copies "nan" into the character buffer, which will be added to the full print buffer
                                  // Including the NaN maintains the relative positioning of all data — it is important to have a placeholder
      }
      strcat(print_buffer, temp_buffer); // Concatenate integer string to print string
      strcat(print_buffer, ","); // Extend data row
    }
    for (uint8_t j=0; j<integer_output_count; j++) {
      // Create temporary buffer for value string
      float integer_output = getIntegerOutputFor(i,j);
      sprintf(temp_buffer, "%d", integer_output); // Convert 32-bit integer to char array
      strcat(print_buffer, temp_buffer); // Concatenate integer string to print string
      strcat(print_buffer, ","); // Extend data row
    }
    for (uint8_t j=0; j<float_output_count; j++) {
      // Create temporary buffer for value string
      float float_output = getFloatOutputFor(i,j);
      floatToCharArray(float_output, temp_buffer);
      strcat(print_buffer, temp_buffer); // Concatenate integer string to print string
      strcat(print_buffer, ","); // Extend data row
    }
    sprintf(temp_buffer, "%d", timestamp_value_buffer[i]); // Convert 32-bit integer to char array
    strcat(print_buffer, temp_buffer); // Concatenate integer string to print string
    strcat(print_buffer, "|"); // Add | as a marker of the end the data row
    print_cycle++;
  }
  print_cycle %= buffer_multiplier;

  return print_buffer[0] != '\0';
}

void setup()
{
  analogReadResolution(adc_resolution);
  Serial.begin(500000);

  Reset();

  // Start the timer, which will trigger the callback at the desired frequency — each ADC pin will get a 1200 Hz sampling rate
  uint16_t timer_rate = adc_pin_count * target_adc_frequency_in_Hz;
  //Make sure frequency settings are acceptable
  if (timer_rate > 7200){
    timer_rate = 7200;
  }
  setTimer(timer_rate);
}

void loop()
{
  if (Serial.available())
  {
    String line = Serial.readStringUntil('\n');
    if (line == "What is your name?"){
      Serial.println("BMED3110|"+String(arduino_name));
    } else if (line == "What are your data IDs?"){
      String ids_string = "";
      for (uint8_t i=0; i<adc_pin_count; i++) {
        // Check to see if the pin should be reported — if not, then don't send the ID
        if (adc_reporting_factors[i] != 0){
          ids_string += adc_pin_ids[i];
          ids_string += ",";
        }
      }
      for (uint8_t i=0; i<integer_output_count; i++) {
        ids_string += integer_output_ids[i];
        ids_string += ",";
      }
      for (uint8_t i=0; i<float_output_count; i++) {
        ids_string += float_output_ids[i];
        ids_string += ",";
      }
      ids_string += "Arduino Timestamp (us)";
      Serial.println(ids_string);
    } else if (line == "Start"){
      Reset();
      adc_timer.start();
    } else if (line == "Stop"){
      adc_timer.stop();
      Serial.flush();
    } else {
      Serial.println("Unknown Request: " + line);
    }
  }

  if (!delay_print){
    if (loadPrintBuffer()){
      Serial.println(print_buffer);
    }
  }

}