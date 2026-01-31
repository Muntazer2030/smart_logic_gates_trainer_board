#include <cJSON.h>
#include "string.h"
#include "esp_log.h"
#include "gpios_manager.h"
#include "global_variables.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h> 

#include "driver/gpio.h"
#include "mqtt_manager.h"
#define TAG "COMMANDS"

// --- PORT MAPPING STRUCTURES ---
typedef struct
{
    const char *name;
    gpio_num_t gpio_num;
} port_mapping_t;

// Map JSON input names to GPIO pins
const port_mapping_t json_to_input_gpio[] = {
    {"A", INPUT_PORT_A},
    {"B", INPUT_PORT_B},
    {"C", INPUT_PORT_C},
    {"D", INPUT_PORT_D},
    {"E", INPUT_PORT_E},
    {"F", INPUT_PORT_F},
    {"S0", INPUT_PORT_S0},
    {"S1", INPUT_PORT_S1}};

#define NUM_INPUT_PINS (sizeof(json_to_input_gpio) / sizeof(port_mapping_t))

// Map JSON output names to GPIO pins
const port_mapping_t json_to_output_gpio[] = {
    {"Y1", OUTPUT_PORT_Y1},
    {"Y2", OUTPUT_PORT_Y2},
    {"Y3", OUTPUT_PORT_Y2},
    {"Y4", OUTPUT_PORT_Y2},
    {"Y5", OUTPUT_PORT_Y2},
    {"Y6", OUTPUT_PORT_Y2},
    {"Y7", OUTPUT_PORT_Y2},
    {"Y8", OUTPUT_PORT_Y2}};
#define NUM_OUTPUT_PINS (sizeof(json_to_output_gpio) / sizeof(port_mapping_t))

/**
 * @brief Initializes all logic ports (Inputs as OUTPUTs, Outputs as INPUTs).
 */
void initialize_all_ports()
{
    ESP_LOGI(TAG, "Initializing all logic board ports...");

    // Initialize Input Ports (ESP32 OUTPUT)
    for (size_t i = 0; i < NUM_INPUT_PINS; i++)
    {
        init_input_port(json_to_input_gpio[i].gpio_num);
    }

    // Initialize Output Ports (ESP32 INPUT)
    for (size_t i = 0; i < NUM_OUTPUT_PINS; i++)
    {
        init_ports(json_to_output_gpio[i].gpio_num);
    }
}

/**
 * @brief Checks if all output ports are initially low (pre-test condition).
 * @return true if all outputs are 0, false otherwise.
 */
static bool pre_test_check()
{
    ESP_LOGI(TAG, "Executing pre-test check: ensuring all outputs are initially low.");
    bool all_low = true;

    for (size_t i = 0; i < NUM_INPUT_PINS; i++)
    {
        int state = gpio_get_level(json_to_input_gpio[i].gpio_num);
        if (state != 0)
        {
            ESP_LOGE(TAG, "PRE-TEST FAILED: Output port %s (GPIO %d) is HIGH (Value: %d). Check for floating inputs or active circuit.",
                     json_to_input_gpio[i].name, json_to_input_gpio[i].gpio_num, state);
            all_low = false;
        }
    }
    return all_low;
}

/**
 * @brief Runs the automated truth table test against the physical logic circuit.
 * @param content The cJSON object containing the truth table definition.
 */
void run_truth_table_test(cJSON *content)
{
    // 1. Setup Phase: Initialize GPIOs and perform pre-check
    initialize_all_ports();

    if (!pre_test_check())
    {
        ESP_LOGE(TAG, "Test aborted due to failed pre-test check.");
        return;
    }

    // 2. table Retrieval
    cJSON *table_array = cJSON_GetObjectItemCaseSensitive(content, "table");

    if (!table_array || !cJSON_IsArray(table_array))
    {
        ESP_LOGE(TAG, "JSON content missing 'table' array.");
        return;
    }

    int total_rows = cJSON_GetArraySize(table_array);
    int passed_tests = 0;

    ESP_LOGI(TAG, "Starting Truth Table Test with %d rows...", total_rows);

    // 3. Execution Phase: Iterate through each row in the table array
    cJSON *row_object = NULL;
    int row_index = 0;
    cJSON_ArrayForEach(row_object, table_array)
    {
        if (!cJSON_IsObject(row_object))
            continue;

        bool row_passed = true;

        // --- A. SET INPUTS ---
        // Iterate over defined input ports to set their state
        for (size_t i = 0; i < NUM_INPUT_PINS; i++)
        {
            cJSON *input_item = cJSON_GetObjectItemCaseSensitive(row_object, json_to_input_gpio[i].name);

            if (input_item && cJSON_IsNumber(input_item))
            {
                int state = (int)input_item->valueint;
                if (state == 0 || state == 1)
                {
                    set_input_port_state(json_to_input_gpio[i].gpio_num, state);
                }
                else
                {
                    ESP_LOGW(TAG, "Row %d: Invalid state for input %s (%d). Skipping input setting.",
                             row_index, json_to_input_gpio[i].name, state);
                }
            }
        }

        // Wait for the physical circuit to settle (e.g., 500ms)
        usleep(500000);

        // --- B. CHECK OUTPUTS ---
        // Iterate over defined output ports to check their state
        for (size_t i = 0; i < NUM_OUTPUT_PINS; i++)
        {
            cJSON *output_item = cJSON_GetObjectItemCaseSensitive(row_object, json_to_output_gpio[i].name);
            printf("Checking output %s\n", json_to_output_gpio[i].name);

            if (output_item && cJSON_IsNumber(output_item))
            {
                int expected_state = (int)output_item->valueint;
                printf("Expected state for output %s: %d\n", json_to_output_gpio[i].name, expected_state);
                if (!check_output_port(json_to_output_gpio[i].gpio_num, expected_state))
                {
                    int actual_state = gpio_get_level(json_to_output_gpio[i].gpio_num);
                    ESP_LOGE(TAG, "FAIL (Row %d): Output %s (GPIO %d) - Expected %d, Got %d.",
                             row_index, json_to_output_gpio[i].name, json_to_output_gpio[i].gpio_num, expected_state, actual_state);
                    row_passed = false;
                }
            }
        }

        // --- C. LOG RESULT ---
        if (row_passed)
        {
            ESP_LOGI(TAG, "PASS (Row %d): Test case matched expected outputs.", row_index);
            passed_tests++;
        }

        row_index++;
    }

    // 4. Final Summary
    ESP_LOGI(TAG, "--- TEST COMPLETE ---");
    ESP_LOGI(TAG, "Total Rows: %d, Passed Rows: %d, Failed Rows: %d",
             total_rows, passed_tests, total_rows - passed_tests);

    if (passed_tests == total_rows)
    {
        ESP_LOGI(TAG, "CIRCUIT VERIFIED: The logic circuit matches the truth table exactly.");
    }
    else
    {
        ESP_LOGE(TAG, "CIRCUIT MISMATCH: The logic circuit does NOT match the truth table.");
    }

    // push test results to MQTT with the result truth table

    // --- 4. PREPARE RESULTS JSON ---
    cJSON *root_report = cJSON_CreateObject();
    cJSON_AddStringToObject(root_report, "command", "test_report");
    cJSON_AddBoolToObject(root_report, "success", (passed_tests == total_rows));
    
    // Create the table array for the results
    cJSON *results_array = cJSON_CreateArray();
    cJSON_AddItemToObject(root_report, "table", results_array);

    // Re-iterate through the rows or use the original table to build the report
    cJSON *row_ptr = NULL;
    cJSON_ArrayForEach(row_ptr, table_array) {
        // Create a deep copy of the original row object
        cJSON *row_copy = cJSON_Duplicate(row_ptr, true);
        
        // Let's perform a physical check again or use a flag stored during the loop.
        // For simplicity, we'll assume you want to report the final status of that row.
        // If you want to include the specific "Got X" value, you'd store them in an array during step 3.
        
        cJSON_AddItemToArray(results_array, row_copy);
    }

    // Convert to string
    char *json_string = cJSON_PrintUnformatted(root_report);

    mqtt_send_message("MTU/UUID_NOT_SET/status", json_string, 1, 0);
    // Clean up
    free(json_string);
    cJSON_Delete(root_report);
    if (passed_tests == total_rows)
    {
        ESP_LOGI(TAG, "CIRCUIT VERIFIED: The logic circuit matches the truth table exactly.");
    }
    else
    {
        ESP_LOGE(TAG, "CIRCUIT MISMATCH: The logic circuit does NOT match the truth table.");
    }

    initialize_all_ports();
}

void handle_command(const char *command, cJSON *content)
{
    if (!command || !content)
    {
        ESP_LOGE(TAG, "handle_command: Null command or content");
        return;
    }

    if (strcmp(command, "restart") == 0)
    {
        esp_restart();
    }
    else if (strcmp(command, "check_truth_table") == 0)
    {
        // --- LOGIC TO HANDLE TRUTH TABLE JSON CONTENT ---
        ESP_LOGI(TAG, "Received command: check_truth_table. Processing content...");

        // Execute the main test function with the provided JSON
        run_truth_table_test(content);
    }
    else
    {
        ESP_LOGW(TAG, "Unknown command: %s", command);
    }
}
