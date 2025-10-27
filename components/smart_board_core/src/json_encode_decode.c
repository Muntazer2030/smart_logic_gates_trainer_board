#include "cJSON.h"
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
void decode_message(const char *message, char *command, cJSON **content)
{
    cJSON *root = cJSON_Parse(message);
    if (!root)
    {
        printf("Error parsing JSON\n");
        return;
    }

    cJSON *j_command = cJSON_GetObjectItem(root, "command");
    if (j_command && cJSON_IsString(j_command))
    {
        strcpy(command, j_command->valuestring);
        printf("Command: %s\n", command);
    }

    cJSON *j_content = cJSON_GetObjectItem(root, "content");
    if (j_content)
    {
        *content = cJSON_Duplicate(j_content, 1);
    }

    cJSON_Delete(root);
}
