// This #include statement was automatically added by the Particle IDE.
#include "SparkJson/SparkJson.h"

const int ledD7 = D7;
/*
 The following are configuration parameters:
 The names should match the first 3 characters of the 'desired' state in the AWS-IOT
   bli[nk]: 0/1; a transition from 0->1 or 1->0 results in the blue LED to blink
   wak[eInterval]: how long the particle goes to sleep for; range: 5 to 600 seconds
   dur[ation]: how long the particle blinks the LED; range: 5 to 60 seconds
   rat[e]: the number of blinks per secong; range: 2 to 9
*/

const char * DESIRED_STATE_ID_ARRAY[] = {"blink", "wakeInterval", "duration", "rate"};
const int  CONFIGURED_INTEGER_PARAMETER_MIN[] = {0, 5, 2, 1};
const int  CONFIGURED_INTEGER_PARAMETER_MAX[] = {1, 600, 30, 9};
const unsigned int BLINK_OFFSET = 0;
const unsigned int WAKEINTERVAL_OFFSET = 1;
const unsigned int DURATION_OFFSET = 2;
const unsigned int RATE_OFFSET = 3;

const char * FUNCTION_NAME = "desired";  //advertised function name
const char * FUNCTION_ADVERTISEMENT = "function";

bool received_desired_state = FALSE;

typedef struct _PERSISTENT_STATE_ {
  int validity_constant;
  int values[RATE_OFFSET+1];
  int previous_blink_state;
} PERSISTENT_STATE ;

PERSISTENT_STATE persistent_state_cache;

StaticJsonBuffer<384> jsonBuffer;  //for json encode, decode
char println_buffer[199];

const int EEPROM_VALID_INDICATOR = 0x1FEDA096; //an arbitrary magic number that indicates the data in the EEPROM is valid
const int EEPROM_CONTENT_ADDRESS = 0;

const unsigned int WAIT_FOR_AWS_IOT = 8000; //milliseconds to wait to retrieve desired state
const unsigned int WAIT_FOR_AWS_IOT_POLL_INTERVAL = 500; //milliseconds between checking for new desired state
int wait_count = 0;
const unsigned int WAIT_FOR_PARTICLE_FLASH_COMMAND = 13000; //window to allow Over The Air updates

STARTUP( early_setup() );  //STARTUP is an enhancement over Arduino's 'setup' allowing quicker initialization for some items.

void early_setup() {
    pinMode(ledD7, OUTPUT);
    EEPROM.get(EEPROM_CONTENT_ADDRESS, persistent_state_cache);
    if(persistent_state_cache.validity_constant != EEPROM_VALID_INDICATOR) {
        for(unsigned int i = 0; i < (sizeof DESIRED_STATE_ID_ARRAY / sizeof *DESIRED_STATE_ID_ARRAY); i+=1) {
          persistent_state_cache.values[i] = CONFIGURED_INTEGER_PARAMETER_MIN[i];
        }
        persistent_state_cache.previous_blink_state = 0;
        persistent_state_cache.validity_constant = EEPROM_VALID_INDICATOR;
    }
}

void setup() {
  Serial.begin(9600);   // open serial over USB
  Particle.function(FUNCTION_NAME, DesiredState);
  Particle.publish(FUNCTION_ADVERTISEMENT, FUNCTION_NAME, 10, PRIVATE); //the bridge on the server will use this name the state update function call
  delay(WAIT_FOR_PARTICLE_FLASH_COMMAND);
 }

void loop() {
    wait_count += 1;
    if (received_desired_state) {
        if (persistent_state_cache.values[BLINK_OFFSET] != persistent_state_cache.previous_blink_state) do_task();
        persistent_state_cache.previous_blink_state = persistent_state_cache.values[BLINK_OFFSET]; //update previous state to new state

        // report state to AWS-IOT
        JsonObject& root = jsonBuffer.createObject();
        for(unsigned int i = 0; i < (sizeof DESIRED_STATE_ID_ARRAY / sizeof *DESIRED_STATE_ID_ARRAY); i+=1) {
            root[DESIRED_STATE_ID_ARRAY[i]] = persistent_state_cache.values[i];
        }
        root["exampleValue"] = 12345; //an example of reported state including more info than the desired state.

        root.printTo(println_buffer, sizeof(println_buffer));
        Particle.publish("stateReport", println_buffer, 10, PRIVATE);

        EEPROM.put(EEPROM_CONTENT_ADDRESS, persistent_state_cache); //update persistent state
    }
    if ((wait_count > WAIT_FOR_AWS_IOT/WAIT_FOR_AWS_IOT_POLL_INTERVAL) || received_desired_state) {
        System.sleep(SLEEP_MODE_DEEP, persistent_state_cache.values[WAKEINTERVAL_OFFSET]);
    } else delay(WAIT_FOR_AWS_IOT_POLL_INTERVAL);
}

void do_task() {
    // replace the code below with the task your IOT device should perform
    for (unsigned int i = 0; i <= (persistent_state_cache.values[RATE_OFFSET] * persistent_state_cache.values[DURATION_OFFSET] * 2); i+=1) {
      if (i & 1) digitalWrite(ledD7, HIGH);
      else digitalWrite(ledD7, LOW);
      delay(500/persistent_state_cache.values[RATE_OFFSET]);
    }
}

// the following Particle function is called by the bridge server after it gets the desired state from AWS-IoT
// Because particle function calls are limited to 63 characters, the bridge shortens the names to 3 characters,
//   hence only the first 3 characters of the keys are compared.
int DesiredState(String state) {
    int return_code = 0;
    char value[15];
    char short_key[4] = {'\0'};

    // convert String to character array for JSON library
    char * state_text_string = new char [state.length() + 1U];
    memcpy(state_text_string,
       state.c_str(),
       state.length());
    state_text_string[state.length()] = '\0';  // Add the nul terminator

    JsonObject& root = jsonBuffer.parseObject(state_text_string);
    if (!root.success())
    {
        Particle.publish("ERROR: json_parsing_failed for: ", state_text_string, 10, PRIVATE);
        return_code = -99;
    } else {
        // update persistent_state_cache.values with desired state recieved from AWS IOT
        for(unsigned int i = 0; i < (sizeof DESIRED_STATE_ID_ARRAY / sizeof *DESIRED_STATE_ID_ARRAY); i+=1) {
            strncpy(short_key, DESIRED_STATE_ID_ARRAY[i], 3);
            if (root.containsKey(short_key)) {
                int valueInt = root[short_key].as<int>();
                sprintf(value, "%s: %d", short_key, valueInt);
                // check the value is within the min/max range for the configuration parameter:
                if ((valueInt < CONFIGURED_INTEGER_PARAMETER_MIN[i]) || (valueInt > CONFIGURED_INTEGER_PARAMETER_MAX[i])) return_code = -i; //out of range
                else persistent_state_cache.values[i] = valueInt;
            }
        };
        //Particle.publish(DESIRED_STATE_ID_ARRAY[i], value, 10, PRIVATE);
        if (return_code < 0) Particle.publish("DEBUG: illegal value in desiredState ", value, 10, PRIVATE);
        received_desired_state = TRUE;
    }
    return return_code;  // if the return code is less than zero, it indicates which parameter was out of range
}
