#ifndef RESULT_DATABASE_H
#define RESULT_DATABASE_H

#include "response_queue.h"

typedef enum
{
    RESULT_DATABASE_OK = 0U,
    RESULT_DATABASE_FAIL
} result_database_status_t;


result_database_status_t result_database_open(const char *database_name);

result_database_status_t result_database_insert_command(const app_protocol_command_t *command);

result_database_status_t result_database_update_response(const response_record_t *record);

result_database_status_t result_database_print_summary(void);

void result_database_close(void);

#endif