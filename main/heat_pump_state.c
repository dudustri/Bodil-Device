#define INCLUDE_JSMN
#define HP_STATE_MANAGER
#define INTERNAL_HP_STATE
#include "heat_pump_state.h"

jsmn_parser parser;
jsmntok_t tokens[RESPONSE_DATA_SIZE];
static StateData current_state;

StateData *heat_pump_state_init(void)
{
    current_state.state = NORMAL;
    current_state.timestamp = 0;
    return &current_state;
}

bool set_energy_consumption_state(StateData *state, unsigned long long timestamp, enum EnergyConsumptionState state_response)
{
    if (state != NULL && 0 < state_response && state_response <= 4)
    {
        state->timestamp = timestamp;
        state->state = state_response;

        return true;
    }
    return false;
}

StateData * get_current_energy_consumption_state(void)
{
    return &current_state;
}

char *match_state_from_tokens_object(int new_state)
{
    switch (new_state)
    {
    case 0:
        return "UNKNOWN_ENERGY_STATE";
    case 1:
        return "NORMAL";
    case 2:
        return "MEDIUM";
    case 3:
        return "OFF";
    case 4:
        return "MAX";
    default:
        return "UNKNOWN_ENERGY_STATE";
    }
}

int json_key_is_equal(const char *json, jsmntok_t *token, const char *key)
{
    if (token->type == JSMN_STRING && (int)strlen(key) == token->end - token->start &&
        strncmp(json + token->start, key, token->end - token->start) == 0)
    {
        return 0;
    }
    return 1;
}

bool identify_and_set_state(jsmntok_t *tokens, int range, const char *json, StateData *current_state)
{
    const char *TAG_IASS = "Identify and Set State";
    // Iterate through all tokens
    for (int i = 0; i < range; i++)
    {
        // Check for the StateChanges key
        if (tokens[i].type == JSMN_STRING && json_key_is_equal(json, &tokens[i], "StateChanges") == 0)
        {
            // StateChanges found -> check the array type as next token
            if (tokens[i + 1].type == JSMN_ARRAY)
            {
                int array_index = i + 1;
                int obj_content_index = array_index + 1;

                // loop through the array size (i = "StateChanges")
                for (int j = 0; j < tokens[array_index].size; j++)
                {
                    // get the index of the objects inside the array - in this case we have 4 tokens for each + 1 first token [object itself]
                    obj_content_index = obj_content_index + (tokens[array_index].size * tokens[obj_content_index].size + 1) * j;

                    unsigned long long timestamp = -1;
                    int state = -1;

                    if (tokens[obj_content_index + 3].type == JSMN_STRING && json_key_is_equal(json, &tokens[obj_content_index + 3], "Timestamp") == 0)
                    {
                        timestamp = atoll(json + tokens[obj_content_index + 4].start);
                    }
                    if (tokens[obj_content_index + 1].type == JSMN_STRING && json_key_is_equal(json, &tokens[obj_content_index + 1], "State") == 0)
                    {
                        state = atoi(json + tokens[obj_content_index + 2].start);
                    }

                    ESP_LOGI(TAG_IASS, "Unpacked token ~ key: %d / sub: %d-> Timestamp: %llu, State: %d", i, j, timestamp, state);

                    // The timestamp received is always the start one
                    // Always get the first one if there are more than one and check if it is valid! Ordered by nature of the response!
                    if (timestamp != -1 && state != -1 && current_state->timestamp <= timestamp)
                    {
                        if (current_state->state != state)
                        {
                            if (set_energy_consumption_state(current_state, timestamp, state))
                            {
                                if (state != 1)
                                {
                                    set_led_state(BODIL_ON_CONTROL);
                                }
                                else
                                {
                                    set_led_state(READY_LED);
                                };
                                ESP_LOGI(TAG_IASS, "A new energy state was set! State: %s", match_state_from_tokens_object(current_state->state));
                            }
                            else
                            {
                                ESP_LOGW(TAG_IASS, "The state received is not valid -> %d [%s]. No changes were applied!", state, match_state_from_tokens_object(UNKNOWN_ENERGY_STATE));
                                return false;
                            }
                        }
                        else
                        {
                            ESP_LOGI(TAG_IASS, "The state received is the same as the current one... State: %s", match_state_from_tokens_object(current_state->state));
                        }
                        return true;
                    }
                    else
                    {
                        ESP_LOGI(TAG_IASS, "No state with a valid timestamp was identified. The state will not be changed.");
                        return false;
                    }
                }
            }
            else
            {
                ESP_LOGI(TAG_IASS, "State Parser ~ StateChanges is not an array %.*s", tokens[i + 1].end - tokens[i + 1].start, json + tokens[i + 1].start);
            }
            // Stop the parsing process since we processed the StageChanges Array
            return false;
        }
    }
    ESP_LOGI(TAG_IASS, "State Parser ~ StateChanges was not found in the response");
    return false;
}

bool process_heat_pump_energy_state_response(const char *server_response)
{
    const char *TAG_JSMNPAR = "Jasmin JSON Parser";
    jsmn_init(&parser);

    int parser_status = jsmn_parse(&parser, server_response, strlen(server_response), tokens, RESPONSE_DATA_SIZE);

    if (parser_status < 0)
    {
        ESP_LOGE(TAG_JSMNPAR, "Failed to parse JSON: %d", parser_status);
        ESP_LOGE(TAG_JSMNPAR, "Received server_response to be processed %s - %d size", server_response, strlen(server_response));
        return false;
    }

    if (parser_status < 1 || tokens[0].type != JSMN_OBJECT)
    {
        ESP_LOGE(TAG_JSMNPAR, "Object expected");
        return false;
    }

    if (!identify_and_set_state(tokens, parser_status, server_response, &current_state))
    {
        ESP_LOGW(TAG_JSMNPAR, "Unsucessfull state data validation.");
        return false;
    }
    return true;
}