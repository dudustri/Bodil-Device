#include "heat_pump_state.h"

jsmn_parser parser;
jsmn_init(&parser);
jsmntok_t token[128];

enum EnergyConsumptionState current_state = UNKNOWN;
int last_update = 0;

void setEnergyConsumptionState(EnergyConsumptionState state_response){
    current_state = state_response;
}

EnergyConsumptionState getEnergyConsumptionState(void){
    return current_state;
}

int processHeapPumpEnergyStateResponse(char[] server_response){
    // TODO:
    //parse the http response from the server -> using jasmine -> https://components.espressif.com/components/espressif/jsmn
    state_list = jsmn_parse(&parser, server_response, strlen(s), token, 128);
    //identify the current period in the response (most recent value set) comparing with the last_update and the current time
    identifyNewState();
    //set the current state to the new one received
    setEnergyConsumptionState(state_list);
    return 0;
}

int identifyNewState(){
    return 0;
}

