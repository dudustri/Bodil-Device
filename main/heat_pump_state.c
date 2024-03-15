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
    if (state != NULL && 0 <= state_response && state_response <= 4) {
        state->timestamp = timestamp;
        state->state = state_response;
    }
}

StateData *get_current_energy_consumptionState(void){
    return current_state;
}

char * match_state_from_tokens_object(int new_state){
    switch(new_state){
    case 0: return "UNKNOWN";
    case 1: return "NORMAL";
    case 2: return "MEDIUM";
    case 3: return "OFF";
    case 4: return "MAX";
    default: return "UNKNOWN";
    }
}

bool json_key_is_equal(const char *json, jsmntok_t *token, const char *key) {
    if (token->type == JSMN_STRING && (int) strlen(key) == token->end - token->start &&
        strncmp(json + token->start, key, token->end - token->start) == 0) {
        return true;
    }
    return false;
}

//TODO: change this function to initialize a StateData when needed and return the current when it has no change !
bool identify_and_set_state(jsmntok_t *tokens, int range, const char *json, StateData* current_state) {
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

                    unsigned long long timestamp = -1;
                    int state = -1;

                    if (tokens[obj_content_index + 3].type == JSMN_STRING && json_key_is_equal(json, &tokens[obj_content_index + 3], "Timestamp") == 0) {
                        timestamp = atoll(json + tokens[obj_content_index + 4].start);
                    }
                    if (tokens[obj_content_index + 1].type == JSMN_STRING && json_key_is_equal(json, &tokens[obj_content_index + 1], "State") == 0) {
                        state = atoi(json + tokens[obj_content_index + 2].start);
                    }

                    ESP_LOGI("SERVER RESPONSE PARSER", "Item %d -> Timestamp: %llu, State: %d\n", i, timestamp, state);

                    // Always get the first one if there are more than one and check if it is valid! Ordered by nature of the response!
                    if (timestamp != -1 && state != -1 &&current_state->timestamp < timestamp && (unsigned long)time(NULL) < timestamp){
                        set_energy_consumption_state(current_state, timestamp, state);
                        return true;
                    }
                }
            } else {
                ESP_LOGI("SERVER RESPONSE PARSER", "StateChanges is not an array\n");
            }
            // Stop the parsing process since we processed the StageChanges Array
            return false;
        }
        ESP_LOGI("SERVER RESPONSE PARSER", "StateChanges was not found in the response\n");
        return false;
    }
    ESP_LOGI("SERVER RESPONSE PARSER", "Empty server response!\n");
    return false;
}


bool process_heap_pump_energy_state_response(const char* server_response, jsmntok_t *tokens, StateData* state){

    jsmn_init(&parser);

    int parser_status = jsmn_parse(&parser, server_response, strlen(server_response), tokens, TOKEN_SIZE);

    if(parser_status < 0){
        ESP_LOGE("Jasmine JSON Parser", "Failed to parse JSON: %d\n", parser_status);
    }

    if (parser_status < 1 || tokens[0].type != JSMN_OBJECT) {
        ESP_LOGE("Jasmine JSON Parser", "Object expected\n");
        return 1;
    }

    if (!identify_and_set_state(tokens, parser_status, server_response, state)){
        ESP_LOGI("Energy State Update", "No state with a valid timestamp was identified. The state will not be changed.");
        return false;
    }
    ESP_LOGI("Energy State Update", "A new state was set! State: %s\n", match_state_from_tokens_object(state->state));
    return true;
}