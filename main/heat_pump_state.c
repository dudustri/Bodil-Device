#include "heat_pump_state.h"

jsmn_parser parser;
jsmntok_t token[TOKEN_SIZE];
StateData* current_state = NULL;

StateData *heat_pump_state_init(void){
    current_state = (StateData*)malloc(sizeof(StateData));
    if (current_state != NULL) {
        current_state->state = UNKNOWN;
        current_state->timestamp = 0;
    }
    return current_state;
}

void heat_pump_state_destroy(StateData* state){
    free(state);
}

void setEnergyConsumptionState(StateData* state, int timestamp, enum EnergyConsumptionState state_response){
    if (state != NULL) {
        state->timestamp = timestamp;
        state->state = state_response;
    }
}

StateData *getCurrentEnergyConsumptionState(void){
    return current_state;
}

enum EnergyConsumptionState matchStateFromTokenObject(int new_state){
    switch(new_state){
    case 0: return MAX;
    case 1: return MEDIUM;
    case 2: return LOW;
    case 3: return OFF;
    default: return UNKNOWN;
    }
}

StateData *identifyNewState(jsmntok_t *token){
    return current_state;
}

int processHeapPumpEnergyStateResponse(const char* server_response, jsmntok_t *token){

    jsmn_init(&parser);
    // TODO:
    //parse the http response from the server -> using jasmine -> https://components.espressif.com/components/espressif/jsmn
    int parser_status = jsmn_parse(&parser, server_response, strlen(server_response), token, 128);
    //identify the current period in the response (most recent value set) comparing with the last_update and the current time
    if(parser_status < 0){
        ESP_LOGE("Jasmine JSON Parser", "Failed to parse JSON: %d\n", parser_status);
    }
    StateData *new_state = identifyNewState(token);
    if(new_state == NULL){
        ESP_LOGI("Energy State Update", "No state with a valid timestamp was identified. The state will not be changed.");
        return 1;
    }
    if (parser_status < 1 || token[0].type != JSMN_OBJECT) {
        printf("Object expected\n");
        return 1;
    }
    //set the current state to the new one received
    setEnergyConsumptionState(current_state, new_state->timestamp, matchStateFromTokenObject(new_state->state));
    return 0;

}