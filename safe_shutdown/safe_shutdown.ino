#include <Wire.h>
#define I2C_SLAVE_ADDRESS 0x4
#define MAX_TASKS 2
#define MAX_BATTERY_READS 4
#define CHECK_STATE_INTERVAL 500
#define CHECK_STATE_DELAY 5000
#define BATTERY_READ_INTERVAL 1000
#define BATTERY_READ_DELAY 1000

typedef void(*TaskFunction)(); // Function pointer

/* PIN DESIGNATIONS
** INTERNAL **
* pinBattery:           PA1
* pinSwitch:
* pinKeepAlive:         PB3

** RASPBERRY PI GPIO **
* Keep-Alive from Pi:   PA3
* Shutdown to Pi:       PA7
* I²C SDA:              PB0
* I²C SCL:              PB2

** ISP **
* MOSI:                 PA4
* MISO:                 PA2
* SCK:                  PA5
*/

int pinBattery = A1;
int pinSwitch = 7;
int pinKeepAlive = 3;
int gpioKeepAlive = A3;
int gpioShutdown = A7;
int pinSDA = 0;
int pinSCL = 3;

int pinMOSI = A4;
int pinMISO = A2;
int pinSCK = A5;

// ----- BATTERY -----

uint8_t vIndex = 0;
int voltages[MAX_BATTERY_READS] = { 1 };
int vTotal = MAX_BATTERY_READS;

// ----- STATE -----
typedef enum state {BOOTUP, RUNNING, SHUTDOWN} State;

typedef struct {
  State current_state;
  float average_voltage;
} SystemState;

SystemState system_state;

// ----- TASKS -----
typedef struct {
  TaskFunction func;
  int count;
  int max_count;
  uint16_t interval_millis;
  uint64_t previous_millis;
} Task;

Task all_tasks[MAX_TASKS];
volatile uint8_t num_tasks = 0;

int createTask(TaskFunction function, int interval, int delay, int repeat) {
  if (num_tasks == MAX_TASKS) { // Too many tasks?
    // Find one which is complete & overwrite it
    for (int i = 0; i < num_tasks; i++) {
      if (all_tasks[i].count >= all_tasks[i].max_count) {
        all_tasks[i].func = function;
        all_tasks[i].max_count = repeat;
        all_tasks[i].count = 0;
        all_tasks[i].interval_millis = interval;
        all_tasks[i].previous_millis = millis() - interval + delay;
        return 1; // Success
      }
    }
    return 0; // Failure
  }
  else {
    // Or add a new task
    all_tasks[num_tasks].func = function;
    all_tasks[num_tasks].max_count = repeat;
    all_tasks[num_tasks].count = 0;
    all_tasks[num_tasks].interval_millis = interval;
    all_tasks[num_tasks].previous_millis = millis() - interval + delay;
  }
  num_tasks += 1;
  return 1; // Success
}

void executeTasks() {
  if (num_tasks == 0) { return; }
  for (int i = 0; i <= num_tasks; i++) {
    // Execute infinite tasks and those whose max_count has not been reached
    if ((all_tasks[i].max_count == -1) || (all_tasks[i].count < all_tasks[i].max_count)) {
      if (all_tasks[i].previous_millis + all_tasks[i].interval_millis <= millis()) {
        // Reset the elapsed time
        all_tasks[i].previous_millis = millis();
        // Don't count infinite tasks
        if (all_tasks[i].max_count > -1) { all_tasks[i].count += 1; }
        // Run the task
        all_tasks[i].func();
      }
    }
  }
}

// ----- FUNCTIONS -----

/* Reads the pin voltage and stores
 * the average of thelast 5 reads in
 * SystemState.battery_voltage
 */
void readBatteryVoltage() {
  //Serial.println("Reading battery...");
  // Increment the voltages[] index
  vIndex++;
  if (vIndex >= MAX_BATTERY_READS) {
    vIndex = 0;
  }
  // Subtract the oldest value
  vTotal -= voltages[vIndex];

  // Store the latest value
  voltages[vIndex] = analogRead(pinBattery);
  vTotal += voltages[vIndex];

  
  

  // Some debugging output. This can be removed from the final sketch.

  float v = voltages[vIndex] * (5.00 / 1023.00);
  char str_v[8];
  char str_a[8];
  dtostrf(v, 4, 2, str_v);
  sprintf(str_a, "%d", voltages[vIndex]);
  char buffer[24] = "Battery: ";
  strcat(buffer, str_v);
  strcat(buffer, "v (");
  strcat(buffer, str_a);
  strcat(buffer, ")");
  Serial.println(buffer);
  system_state.average_voltage = v;

}

/* Checks the state of the power switch
 */
void checkState() {
  //Serial.println("Checking switch...");

  int switch_state = digitalRead(pinSwitch);
  if (switch_state == 1) { // subject to change (inverse)
    system_state.current_state = SHUTDOWN;
  }
  else {
    system_state.current_state = RUNNING;
  }
}

// ----- I2C -----
/* Writes the SystemState struct to the I2C bus
 */
void tws_requestEvent() {
   const byte * p = (const byte*) &system_state;
   unsigned int i;
   for (i = 0; i < sizeof system_state; i++)
         Wire.write(*p++);
}

/* Used to take instructions from the I2C master python script
 * eg. change polling frequency of battery Reads
 * eg. enable/disable power switch
 */
void tws_receiveEvent(uint8_t howMany) {
//  while(Wire.available()) {
////    int data = Wire.read();
//    byte * p = (byte*) &value;
//    unsigned int i;
//    for (i = 0; i < sizeof value; i++)
//      *p++ = Wire.read();
//    return i;
//  }

}

// ----- START -----
void setup() {
  Serial.begin(9600);
  Serial.println("Running...");
  system_state.current_state = BOOTUP;

  // Initialise the pins
  pinMode(pinBattery, INPUT);
  pinMode(pinSwitch, INPUT);
  analogReference(DEFAULT);
  //pinMode(pinKeepAlive, OUTPUT);
  //digitalWrite(pinKeepAlive, HIGH);
  Serial.println("Pins OK");

  // Create some tasks
  int task_result = createTask(readBatteryVoltage, BATTERY_READ_INTERVAL, BATTERY_READ_DELAY, -1);
  int task_result2 = createTask(checkState, CHECK_STATE_INTERVAL, CHECK_STATE_DELAY, -1);

  if (task_result + task_result2 == 2) {
    Serial.println("Tasks OK");
  }
  else {
    Serial.println("Problem creating one or more tasks!");
  }

  // Setup the I2C bus
  Wire.begin(I2C_SLAVE_ADDRESS);
  Wire.onReceive(tws_receiveEvent);
  Wire.onRequest(tws_requestEvent);
  Serial.println("I2C OK");
}

void loop() {
  executeTasks();
//  Wire_stop_check();
  
}
