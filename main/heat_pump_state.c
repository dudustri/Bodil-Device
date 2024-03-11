#include "heat_pump_state.h"

jsmn_parser parser;
jsmntok_t tokens[TOKEN_SIZE];
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

enum EnergyConsumptionState matchStateFromTokensObject(int new_state){
    switch(new_state){
    case 0: return UNKNOWN;
    case 1: return NORMAL;
    case 2: return MEDIUM;
    case 3: return OFF;
    case 4: return MAX;
    default: return UNKNOWN;
    }
}

StateData *identifyNewState(jsmntok_t *tokens){
    return current_state;
}

int processHeapPumpEnergyStateResponse(const char* server_response, jsmntok_t *tokens){

    jsmn_init(&parser);
    // TODO:
    //parse the http response from the server -> using jasmine -> https://components.espressif.com/components/espressif/jsmn
    int parser_status = jsmn_parse(&parser, server_response, strlen(server_response), tokens, 128);
    //identify the current period in the response (most recent value set) comparing with the last_update and the current time
    if(parser_status < 0){
        ESP_LOGE("Jasmine JSON Parser", "Failed to parse JSON: %d\n", parser_status);
    }
    // TODO Figure out this later
    // StateData *new_state = identifyNewState(tokens);
    // if(new_state == NULL){
    //     ESP_LOGI("Energy State Update", "No state with a valid timestamp was identified. The state will not be changed.");
    //     return 1;
    // }
    if (parser_status < 1 || tokens[0].type != JSMN_OBJECT) {
        printf("Object expected\n");
        return 1;
    }

    jsmntok_t key = tokens[1];
    unsigned int length = key.end - key.start;
    char keyString[length + 1];    
    memcpy(keyString, &server_response[key.start], length);
    keyString[length] = '\0';

    ESP_LOGI("Energy State Update", "New state: %s", keyString);
    // TODO: Figure out this later
    //set the current state to the new one 
    // setEnergyConsumptionState(current_state, new_state->timestamp, matchStateFromTokensObject(new_state->state));
    return 0;

}