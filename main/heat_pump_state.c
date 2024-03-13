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

void set_energy_consumption_state(StateData* state, int timestamp, enum EnergyConsumptionState state_response){
    if (state != NULL) {
        state->timestamp = timestamp;
        state->state = state_response;
    }
}

StateData *get_current_energy_consumptionState(void){
    return current_state;
}

enum EnergyConsumptionState match_state_from_tokens_object(int new_state){
    switch(new_state){
    case 0: return UNKNOWN;
    case 1: return NORMAL;
    case 2: return MEDIUM;
    case 3: return OFF;
    case 4: return MAX;
    default: return UNKNOWN;
    }
}

StateData *identify_new_state(jsmntok_t *tokens){
    return current_state;
}

int process_heap_pump_energy_state_response(const char* server_response, jsmntok_t *tokens){

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

int json_key_is_equal(const char *json, jsmntok_t *token, const char *key) {
    if (token->type == JSMN_STRING && (int) strlen(key) == token->end - token->start &&
        strncmp(json + token->start, key, token->end - token->start) == 0) {
        return 0;
    }
    return -1;
}

//TODO: change this function to initialize a StateData when needed and return the current when it has no change !
void identify_new_state_print_mock_parser(jsmntok_t *tokens, int range, const char *json) {
    // Iterate through all tokens
    for (int i = 0; i < range; i++) {

        // Check for the StateChanges key
        if (tokens[i].type == JSMN_STRING && json_key_is_equal(json, &tokens[i], "StateChanges") == 0) {
            // StateChanges found -> check the array type as next token
            if (tokens[i + 1].type == JSMN_ARRAY) {
                int array_index = i + 1;
                int obj_content_index = array_index + 1;

                //loop through the array size (i = "StateChanges")
                for (int j = 0; j < tokens[array_index].size; j++) {
                    // get the index of the objects inside the array - in this case we have 4 tokens for each + 1 first token [object itself] 
                    obj_content_index = obj_content_index + (tokens[array_index].size*tokens[obj_content_index].size+1)*j;

                    long long timestamp = -1;
                    int state = -1;

                    if (tokens[obj_content_index + 3].type == JSMN_STRING && json_key_is_equal(json, &tokens[obj_content_index + 3], "Timestamp") == 0) {
                        timestamp = atoll(json + tokens[obj_content_index + 4].start);
                    }
                    if (tokens[obj_content_index + 1].type == JSMN_STRING && json_key_is_equal(json, &tokens[obj_content_index + 1], "State") == 0) {
                        state = atoi(json + tokens[obj_content_index + 2].start);
                    }
                    ESP_LOGI("SERVER RESPONSE PARSER", "Timestamp: %lld, State: %d\n", timestamp, state);
                }
            } else {
                ESP_LOGI("SERVER RESPONSE PARSER", "StateChanges is not an array\n");
            }
            // Stop the parsing process since we processed the StageChanges Array
            break;
        }
    }
}
