#include <cJSON.h>
#include "string.h"
#include "esp_log.h"
#include "gpios_manager.h"
#include "global_variables.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdio.h>

#include <stdbool.h>
#include <unistd.h> // For usleep/vTaskDelay equivalent

// --- Mockup/Placeholder Includes and Functions ---
// In a real ESP-IDF project, these would be external:

#include "driver/gpio.h"

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
    {"G", INPUT_PORT_G},
    {"H", INPUT_PORT_H}};

#define NUM_INPUT_PINS (sizeof(json_to_input_gpio) / sizeof(port_mapping_t))

// Map JSON output names to GPIO pins
const port_mapping_t json_to_output_gpio[] = {
    {"Z", OUTPUT_PORT_Z},
    {"Y", OUTPUT_PORT_Y},
    {"X", OUTPUT_PORT_X},
    {"W", OUTPUT_PORT_W},
    {"V", OUTPUT_PORT_V},
    {"U", OUTPUT_PORT_U},
    {"T", OUTPUT_PORT_T}};
#define NUM_OUTPUT_PINS (sizeof(json_to_output_gpio) / sizeof(port_mapping_t))

/**
 * @brief Initializes all logic ports (Inputs as OUTPUTs, Outputs as INPUTs).
 */
static void initialize_all_ports()
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

    // 2. Data Retrieval
    cJSON *data_array = cJSON_GetObjectItemCaseSensitive(content, "DATA");

    if (!data_array || !cJSON_IsArray(data_array))
    {
        ESP_LOGE(TAG, "JSON content missing 'DATA' array.");
        return;
    }

    int total_rows = cJSON_GetArraySize(data_array);
    int passed_tests = 0;

    ESP_LOGI(TAG, "Starting Truth Table Test with %d rows...", total_rows);

    // 3. Execution Phase: Iterate through each row in the DATA array
    cJSON *row_object = NULL;
    int row_index = 0;
    cJSON_ArrayForEach(row_object, data_array)
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

        // Wait for the physical circuit to settle (e.g., 5ms)
        usleep(5000);

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
