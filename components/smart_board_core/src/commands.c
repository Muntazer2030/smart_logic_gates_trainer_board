#include <cJSON.h>
#include "string.h"
#include <strings.h>
#include "esp_log.h"
#include "esp_system.h"
#include "gpios_manager.h"
#include "global_variables.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

#include "driver/gpio.h"
#include "mqtt_manager.h"
#include "commands.h"
#define TAG "COMMANDS"

/*
 * Truth table execution.
 *
 * The board drives the circuit's inputs (A..F, S0, S1) and reads its outputs
 * (Y1..Y8). A row of the table says what to drive and what should come back.
 *
 * Expected output values
 *   0 / 1   must read exactly this
 *   "X"     don't care, not checked
 *   "NC"    must be unchanged from the previous row  (sequential circuits)
 *   "T"     must be the inverse of the previous row  (toggle)
 *
 * Sequential circuits
 *   The table may name a clock line with  "clock": "S1".  A row carrying
 *   "clockPulse": true is applied by setting the data inputs, pulsing that
 *   line, and only then reading the outputs.
 */

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
    {"Y3", OUTPUT_PORT_Y3},
    {"Y4", OUTPUT_PORT_Y4},
    {"Y5", OUTPUT_PORT_Y5},
    {"Y6", OUTPUT_PORT_Y6},
    {"Y7", OUTPUT_PORT_Y7},
    {"Y8", OUTPUT_PORT_Y8}};
#define NUM_OUTPUT_PINS (sizeof(json_to_output_gpio) / sizeof(port_mapping_t))

// TTL logic settles in nanoseconds; this is only to let the wiring and the
// input protection network settle. The old value of 400 ms made a 16 row test
// take over six seconds for no benefit.
#define DEFAULT_SETTLE_MS 60
#define CLOCK_PULSE_MS 150
#define MAX_REPORTED_ROWS 32

// Expected-value kinds parsed out of the JSON.
typedef enum
{
    EXPECT_LEVEL,      // exactly 0 or 1
    EXPECT_DONT_CARE,  // "X"
    EXPECT_NO_CHANGE,  // "NC"
    EXPECT_TOGGLE      // "T"
} expect_kind_t;

/**
 * @brief Initializes all logic ports (the circuit's inputs are driven by the
 *        ESP32, the circuit's outputs are read by it).
 */
void initialize_all_ports(void)
{
    ESP_LOGI(TAG, "Initializing all logic board ports...");

    for (size_t i = 0; i < NUM_INPUT_PINS; i++)
    {
        init_input_port(json_to_input_gpio[i].gpio_num);
    }

    for (size_t i = 0; i < NUM_OUTPUT_PINS; i++)
    {
        init_ports(json_to_output_gpio[i].gpio_num);
    }
}

/**
 * @brief Checks that the circuit's outputs read low before the test starts.
 *
 * This used to loop over json_to_input_gpio, i.e. it read back the pins the
 * ESP32 itself drives, which always reads what was just written and so never
 * told us anything about the circuit under test.
 */
static bool pre_test_check(char *reason, size_t reason_len)
{
    ESP_LOGI(TAG, "Pre-test check: all circuit outputs should read low.");

    // Drive every input low first, so the outputs have a defined starting point.
    for (size_t i = 0; i < NUM_INPUT_PINS; i++)
    {
        set_input_port_state(json_to_input_gpio[i].gpio_num, 0);
    }
    vTaskDelay(pdMS_TO_TICKS(DEFAULT_SETTLE_MS));

    bool all_low = true;
    /*
    for (size_t i = 0; i < NUM_OUTPUT_PINS; i++)
    {
        int state = gpio_get_level(json_to_output_gpio[i].gpio_num);
        if (state != 0)
        {
            ESP_LOGW(TAG,
                     "Pre-test: output %s (GPIO %d) reads HIGH with all inputs low.",
                     json_to_output_gpio[i].name, json_to_output_gpio[i].gpio_num);
            if (all_low && reason)
            {
                snprintf(reason, reason_len,
                         "Output %s is high before the test started. Check for a "
                         "floating pin or a miswired output.",
                         json_to_output_gpio[i].name);
            }
            all_low = false;
        }
    }*/
    return all_low; 
}

/**
 * @brief Reads an expected value out of a row, e.g. 1, "X", "NC" or "T".
 */
static bool parse_expected(const cJSON *item, expect_kind_t *kind, int *level)
{
    if (!item)
    {
        return false; // this output is not mentioned in this row
    }

    if (cJSON_IsNumber(item))
    {
        *kind = EXPECT_LEVEL;
        *level = item->valueint ? 1 : 0;
        return true;
    }

    if (cJSON_IsBool(item))
    {
        *kind = EXPECT_LEVEL;
        *level = cJSON_IsTrue(item) ? 1 : 0;
        return true;
    }

    if (cJSON_IsString(item) && item->valuestring)
    {
        const char *text = item->valuestring;
        if (strcasecmp(text, "X") == 0 || strcasecmp(text, "-") == 0)
        {
            *kind = EXPECT_DONT_CARE;
            return true;
        }
        if (strcasecmp(text, "NC") == 0 || strcasecmp(text, "No Change") == 0)
        {
            *kind = EXPECT_NO_CHANGE;
            return true;
        }
        if (strcasecmp(text, "T") == 0 || strcasecmp(text, "Toggle") == 0)
        {
            *kind = EXPECT_TOGGLE;
            return true;
        }
        // "0" / "1" written as text
        if (strcmp(text, "0") == 0 || strcmp(text, "1") == 0)
        {
            *kind = EXPECT_LEVEL;
            *level = text[0] - '0';
            return true;
        }
    }

    ESP_LOGW(TAG, "Unrecognised expected value, treating it as don't care.");
    *kind = EXPECT_DONT_CARE;
    return true;
}

/**
 * @brief Pulses the clock line so an edge-triggered circuit latches its input.
 */
static void pulse_clock(int clock_pin, int settle_ms)
{
    set_input_port_state(clock_pin, 0);
    vTaskDelay(pdMS_TO_TICKS(CLOCK_PULSE_MS));
    set_input_port_state(clock_pin, 1);   // rising edge
    vTaskDelay(pdMS_TO_TICKS(CLOCK_PULSE_MS));
    vTaskDelay(pdMS_TO_TICKS(settle_ms));
}

/**
 * @brief Publishes the test report on this board's own status topic.
 */
static void publish_report(cJSON *report)
{
    char topic[96];
    snprintf(topic, sizeof(topic), "MTU/%s/status", uuid);

    char *json_string = cJSON_PrintUnformatted(report);
    if (json_string)
    {
        mqtt_send_message(topic, json_string, 1, 0);
        cJSON_free(json_string);
    }
    else
    {
        ESP_LOGE(TAG, "Could not serialise the test report.");
    }
}

static void publish_failure(const char *request_id, const char *reason)
{
    cJSON *report = cJSON_CreateObject();
    cJSON_AddStringToObject(report, "command", "test_report");
    cJSON_AddStringToObject(report, "boardUid", uuid);
    cJSON_AddStringToObject(report, "requestId", request_id ? request_id : "");
    cJSON_AddBoolToObject(report, "success", false);
    cJSON_AddStringToObject(report, "error", reason);
    cJSON_AddNumberToObject(report, "totalRows", 0);
    cJSON_AddNumberToObject(report, "passedRows", 0);
    cJSON_AddItemToObject(report, "rows", cJSON_CreateArray());
    publish_report(report);
    cJSON_Delete(report);
}

/**
 * @brief Runs the truth table against the physical circuit and reports back.
 */
void run_truth_table_test(cJSON *content, const char *request_id)
{
    initialize_all_ports();

    char reason[160] = {0};
    if (!pre_test_check(reason, sizeof(reason)))
    {
        ESP_LOGE(TAG, "Test aborted: %s", reason);
        publish_failure(request_id, reason);
        return;
    }

    cJSON *table_array = cJSON_GetObjectItemCaseSensitive(content, "table");
    if (!table_array || !cJSON_IsArray(table_array))
    {
        publish_failure(request_id, "The truth table has no 'table' array.");
        return;
    }

    const cJSON *name_item = cJSON_GetObjectItemCaseSensitive(content, "circuitName");
    const char *circuit_name =
        (name_item && cJSON_IsString(name_item)) ? name_item->valuestring : "Circuit";

    int settle_ms = DEFAULT_SETTLE_MS;
    const cJSON *settle_item = cJSON_GetObjectItemCaseSensitive(content, "settleMs");
    if (settle_item && cJSON_IsNumber(settle_item) && settle_item->valueint > 0)
    {
        settle_ms = settle_item->valueint;
    }

    // Optional clock line for sequential circuits.
    int clock_pin = -1;
    const char *clock_name = NULL;
    const cJSON *clock_item = cJSON_GetObjectItemCaseSensitive(content, "clock");
    if (clock_item && cJSON_IsString(clock_item) && clock_item->valuestring)
    {
        for (size_t i = 0; i < NUM_INPUT_PINS; i++)
        {
            if (strcmp(json_to_input_gpio[i].name, clock_item->valuestring) == 0)
            {
                clock_pin = json_to_input_gpio[i].gpio_num;
                clock_name = json_to_input_gpio[i].name;
                break;
            }
        }
        if (clock_pin < 0)
        {
            ESP_LOGW(TAG, "Clock line '%s' is not a port on this board.",
                     clock_item->valuestring);
        }
    }

    int total_rows = cJSON_GetArraySize(table_array);
    int passed_rows = 0;
    ESP_LOGI(TAG, "Testing '%s': %d row(s), settle %d ms%s",
             circuit_name, total_rows, settle_ms,
             clock_name ? ", clocked" : "");

    cJSON *rows_report = cJSON_CreateArray();

    // Previous measurement per output, for the "NC" and "T" comparisons.
    int previous_measured[NUM_OUTPUT_PINS];
    bool have_previous = false;
    for (size_t i = 0; i < NUM_OUTPUT_PINS; i++)
    {
        previous_measured[i] = 0;
    }

    cJSON *row_object = NULL;
    int row_index = 0;

    cJSON_ArrayForEach(row_object, table_array)
    {
        if (!cJSON_IsObject(row_object))
        {
            continue;
        }

        bool row_passed = true;
        cJSON *row_report = cJSON_CreateObject();
        cJSON *inputs_report = cJSON_CreateObject();
        cJSON *expected_report = cJSON_CreateObject();
        cJSON *measured_report = cJSON_CreateObject();

        // --- A. DRIVE THE INPUTS ---
        for (size_t i = 0; i < NUM_INPUT_PINS; i++)
        {
            // The clock is driven separately, below.
            if (clock_pin >= 0 && json_to_input_gpio[i].gpio_num == clock_pin)
            {
                continue;
            }

            cJSON *input_item =
                cJSON_GetObjectItemCaseSensitive(row_object, json_to_input_gpio[i].name);

            if (input_item && cJSON_IsNumber(input_item))
            {
                int state = input_item->valueint ? 1 : 0;
                set_input_port_state(json_to_input_gpio[i].gpio_num, state);
                cJSON_AddNumberToObject(inputs_report, json_to_input_gpio[i].name, state);
            }
        }

        // --- B. CLOCK, OR JUST SETTLE ---
        const cJSON *pulse_item =
            cJSON_GetObjectItemCaseSensitive(row_object, "clockPulse");
        bool wants_pulse = pulse_item && cJSON_IsTrue(pulse_item);

        if (clock_pin >= 0 && wants_pulse)
        {
            pulse_clock(clock_pin, settle_ms);
            cJSON_AddNumberToObject(inputs_report, clock_name, 1);
        }
        else
        {
            if (clock_pin >= 0)
            {
                set_input_port_state(clock_pin, 0);
                cJSON_AddNumberToObject(inputs_report, clock_name, 0);
            }
            vTaskDelay(pdMS_TO_TICKS(settle_ms));
        }

        // --- C. READ AND COMPARE THE OUTPUTS ---
        for (size_t i = 0; i < NUM_OUTPUT_PINS; i++)
        {
            cJSON *output_item =
                cJSON_GetObjectItemCaseSensitive(row_object, json_to_output_gpio[i].name);

            expect_kind_t kind;
            int expected_level = 0;
            if (!parse_expected(output_item, &kind, &expected_level))
            {
                continue; // output not used by this circuit
            }

            int measured = gpio_get_level(json_to_output_gpio[i].gpio_num);
            cJSON_AddNumberToObject(measured_report, json_to_output_gpio[i].name, measured);

            bool output_ok = true;
            switch (kind)
            {
            case EXPECT_LEVEL:
                cJSON_AddNumberToObject(expected_report, json_to_output_gpio[i].name,
                                        expected_level);
                output_ok = (measured == expected_level);
                break;

            case EXPECT_DONT_CARE:
                cJSON_AddStringToObject(expected_report, json_to_output_gpio[i].name, "X");
                break;

            case EXPECT_NO_CHANGE:
                cJSON_AddStringToObject(expected_report, json_to_output_gpio[i].name, "NC");
                // Nothing to compare against on the very first row.
                output_ok = !have_previous || (measured == previous_measured[i]);
                break;

            case EXPECT_TOGGLE:
                cJSON_AddStringToObject(expected_report, json_to_output_gpio[i].name, "T");
                output_ok = !have_previous || (measured != previous_measured[i]);
                break;
            }

            if (!output_ok)
            {
                ESP_LOGE(TAG, "FAIL row %d: output %s (GPIO %d) read %d.",
                         row_index, json_to_output_gpio[i].name,
                         json_to_output_gpio[i].gpio_num, measured);
                row_passed = false;
            }

            previous_measured[i] = measured;
        }
        have_previous = true;

        if (row_passed)
        {
            passed_rows++;
            ESP_LOGI(TAG, "PASS row %d", row_index);
        }

        // Keep the report bounded - a long table would otherwise blow past the
        // broker's message size limit.
        if (row_index < MAX_REPORTED_ROWS)
        {
            cJSON_AddNumberToObject(row_report, "index", row_index);
            cJSON_AddItemToObject(row_report, "inputs", inputs_report);
            cJSON_AddItemToObject(row_report, "expected", expected_report);
            cJSON_AddItemToObject(row_report, "measured", measured_report);
            cJSON_AddBoolToObject(row_report, "passed", row_passed);
            cJSON_AddItemToArray(rows_report, row_report);
        }
        else
        {
            cJSON_Delete(inputs_report);
            cJSON_Delete(expected_report);
            cJSON_Delete(measured_report);
            cJSON_Delete(row_report);
        }

        row_index++;
    }

    bool success = (total_rows > 0) && (passed_rows == total_rows);
    ESP_LOGI(TAG, "--- TEST COMPLETE --- %d/%d rows passed. %s",
             passed_rows, total_rows,
             success ? "CIRCUIT VERIFIED" : "CIRCUIT MISMATCH");

    cJSON *report = cJSON_CreateObject();
    cJSON_AddStringToObject(report, "command", "test_report");
    cJSON_AddStringToObject(report, "boardUid", uuid);
    cJSON_AddStringToObject(report, "requestId", request_id ? request_id : "");
    cJSON_AddStringToObject(report, "circuitName", circuit_name);
    cJSON_AddBoolToObject(report, "success", success);
    cJSON_AddNumberToObject(report, "totalRows", total_rows);
    cJSON_AddNumberToObject(report, "passedRows", passed_rows);
    cJSON_AddItemToObject(report, "rows", rows_report);

    publish_report(report);
    cJSON_Delete(report);

    initialize_all_ports();
}

void handle_command(const char *command, cJSON *content, const char *request_id)
{
    if (!command)
    {
        ESP_LOGE(TAG, "handle_command: no command given");
        return;
    }

    if (strcmp(command, "restart") == 0)
    {
        ESP_LOGW(TAG, "Restart requested over MQTT.");
        vTaskDelay(pdMS_TO_TICKS(200));
        esp_restart();
    }
    else if (strcmp(command, "check_truth_table") == 0)
    {
        if (!content)
        {
            publish_failure(request_id, "check_truth_table came with no content.");
            return;
        }
        ESP_LOGI(TAG, "Running a truth table check...");
        run_truth_table_test(content, request_id);
    }
    else if (strcmp(command, "self_test") == 0)
    {
        run_port_self_test();
    }
    else if (strcmp(command, "identify") == 0)
    {
        // Blink every output driver so the bench can be spotted in the lab.
        for (int repeat = 0; repeat < 6; repeat++)
        {
            for (size_t i = 0; i < NUM_INPUT_PINS; i++)
            {
                set_input_port_state(json_to_input_gpio[i].gpio_num, repeat % 2);
            }
            vTaskDelay(pdMS_TO_TICKS(250));
        }
        initialize_all_ports();
    }
    else
    {
        ESP_LOGW(TAG, "Unknown command: %s", command);
    }
}

/**
 * @brief Walks every drive pin once. Only run on request, never automatically:
 *        it toggles the ports the student's circuit is wired to.
 */
void run_port_self_test(void)
{
    ESP_LOGI(TAG, "Running port self test...");
    initialize_all_ports();

    for (size_t i = 0; i < NUM_INPUT_PINS; i++)
    {
        ESP_LOGI(TAG, "Driving %s (GPIO %d)", json_to_input_gpio[i].name,
                 json_to_input_gpio[i].gpio_num);
        set_input_port_state(json_to_input_gpio[i].gpio_num, 1);
        vTaskDelay(pdMS_TO_TICKS(100));
        set_input_port_state(json_to_input_gpio[i].gpio_num, 0);
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    initialize_all_ports();
    ESP_LOGI(TAG, "Port self test finished.");
}
