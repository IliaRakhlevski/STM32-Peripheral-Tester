#include "result_database.h"
#include "app_protocol.h"
#include <stdio.h>
#include <time.h>
#include <sqlite3.h>


/* Test execution status values stored in the database. */
#define RESULT_DATABASE_STATUS_PENDING "PENDING"
#define RESULT_DATABASE_STATUS_PASSED  "PASSED"
#define RESULT_DATABASE_STATUS_FAILED  "FAILED"


/* SQL statement used to create the test results table. */
#define RESULT_DATABASE_CREATE_TABLE_SQL                         \
    "CREATE TABLE IF NOT EXISTS test_results ("                  \
    "test_id INTEGER NOT NULL UNIQUE,"                           \
    "peripheral INTEGER NOT NULL,"                               \
    "iterations INTEGER NOT NULL,"                               \
    "payload_len INTEGER NOT NULL,"                              \
    "sent_time INTEGER NOT NULL,"                                \
    "received_time INTEGER,"                                     \
    "result INTEGER,"                                            \
    "status TEXT NOT NULL"                                       \
    ");"

    
/* SQLite database handle. */
static sqlite3 *result_database = NULL;

/**
 * @brief Opens the SQLite database.
 *
 * Creates or opens the specified SQLite database file.
 *
 * @param[in] database_name Database file name.
 *
 * @return RESULT_DATABASE_OK on success,
 *         RESULT_DATABASE_FAIL otherwise.
 */
result_database_status_t result_database_open(const char *database_name)
{
    char *error_message = NULL;

    if ((database_name == NULL) || (sqlite3_open(database_name, &result_database) != SQLITE_OK))
    {
        return RESULT_DATABASE_FAIL;
    }

    /* Create the test results table if it does not already exist. */
    if (sqlite3_exec(result_database,
                     RESULT_DATABASE_CREATE_TABLE_SQL,
                     NULL,
                     NULL,
                     &error_message) != SQLITE_OK)
    {
        printf("Failed to create database table: %s\n", error_message);

        sqlite3_free(error_message);
        sqlite3_close(result_database);
        result_database = NULL;

        return RESULT_DATABASE_FAIL;
    }

    /* Remove test results from the previous application run. */
    if (sqlite3_exec(result_database,
                 "DELETE FROM test_results;",
                 NULL,
                 NULL,
                 &error_message) != SQLITE_OK)
    {
        printf("Failed to clear database table: %s\n", error_message);

        sqlite3_free(error_message);
        sqlite3_close(result_database);
        result_database = NULL;

        return RESULT_DATABASE_FAIL;
    }

    return RESULT_DATABASE_OK;
}

/**
 * @brief Inserts a test command into the database.
 *
 * Creates a new database record for a transmitted test command.
 * The record is initially marked as pending until a response
 * is received from the target.
 *
 * @param[in] command Pointer to transmitted test command.
 *
 * @return RESULT_DATABASE_OK on success,
 *         RESULT_DATABASE_FAIL otherwise.
 */
result_database_status_t result_database_insert_command(const app_protocol_command_t *command)
{
    static const char sql[] =
        "INSERT INTO test_results "
        "(test_id, peripheral, iterations, payload_len, "
        "sent_time, status) "
        "VALUES (?, ?, ?, ?, ?, ?);";

    sqlite3_stmt *statement;

    if (command == NULL)
    {
        return RESULT_DATABASE_FAIL;
    }

    if (sqlite3_prepare_v2(result_database,
                           sql,
                           -1,
                           &statement,
                           NULL) != SQLITE_OK)
    {
        return RESULT_DATABASE_FAIL;
    }

    sqlite3_bind_int64(statement, 1, (sqlite3_int64)command->test_id);
    sqlite3_bind_int(statement,  2, (int)command->peripheral);
    sqlite3_bind_int(statement,  3, (int)command->iterations);
    sqlite3_bind_int(statement,  4, (int)command->pattern_len);
    sqlite3_bind_int64(statement, 5, (sqlite3_int64)time(NULL));
    sqlite3_bind_text(statement, 6, RESULT_DATABASE_STATUS_PENDING, -1, SQLITE_STATIC);

    if (sqlite3_step(statement) != SQLITE_DONE)
    {
        sqlite3_finalize(statement);
        return RESULT_DATABASE_FAIL;
    }

    sqlite3_finalize(statement);

    return RESULT_DATABASE_OK;
}

/**
 * @brief Updates a test result in the database.
 *
 * Updates an existing database record with the received test
 * response. The matching record is identified by the test ID.
 *
 * @param[in] record Pointer to received response record.
 *
 * @return RESULT_DATABASE_OK on success,
 *         RESULT_DATABASE_FAIL otherwise.
 */
result_database_status_t result_database_update_response(const response_record_t *record)
{
    static const char sql[] =
        "UPDATE test_results "
        "SET received_time = ?, "
        "result = ?, "
        "status = ? "
        "WHERE test_id = ?;";

    sqlite3_stmt *statement;
    const char *status;

    if (record == NULL)
    {
        return RESULT_DATABASE_FAIL;
    }

    status = (record->response.result == APP_PROTOCOL_TEST_SUCCESS) ?
             RESULT_DATABASE_STATUS_PASSED :
             RESULT_DATABASE_STATUS_FAILED;

    if (sqlite3_prepare_v2(result_database,
                           sql,
                           -1,
                           &statement,
                           NULL) != SQLITE_OK)
    {
        return RESULT_DATABASE_FAIL;
    }

    sqlite3_bind_int64(statement, 1, (sqlite3_int64)record->received_time);
    sqlite3_bind_int(statement, 2, (int)record->response.result);
    sqlite3_bind_text(statement, 3, status, -1, SQLITE_STATIC);
    sqlite3_bind_int64(statement, 4, (sqlite3_int64)record->response.test_id);

    if (sqlite3_step(statement) != SQLITE_DONE)
    {
        sqlite3_finalize(statement);
        return RESULT_DATABASE_FAIL;
    }

    sqlite3_finalize(statement);

    return RESULT_DATABASE_OK;
}

/**
 * @brief Prints test records matching the specified SQL query.
 *
 * Executes the provided SQL query and prints matching test records.
 * The query is expected to return the following columns:
 * test_id, peripheral, iterations and payload length.
 *
 * @param[in] title Section title to print.
 * @param[in] sql SQL query to execute.
 *
 * @return RESULT_DATABASE_OK on success,
 *         RESULT_DATABASE_FAIL otherwise.
 */
static result_database_status_t result_database_print_tests(
    const char *title,
    const char *sql)
{
    sqlite3_stmt *statement;
    uint8_t tests_found = 0U;

    if ((title == NULL) || (sql == NULL))
    {
        return RESULT_DATABASE_FAIL;
    }

    if (sqlite3_prepare_v2(result_database,
                           sql,
                           -1,
                           &statement,
                           NULL) != SQLITE_OK)
    {
        return RESULT_DATABASE_FAIL;
    }

    while (sqlite3_step(statement) == SQLITE_ROW)
    {
        if (tests_found == 0U)
        {
            printf("\n%s:\n", title);
            printf("----------------------------------------\n");

            tests_found = 1U;
        }

        printf("Test %-6d %-5s: iterations=%-3d payload=%-3d bytes\n",
               sqlite3_column_int(statement, 0),
               app_protocol_peripheral_to_string(
                   (uint8_t)sqlite3_column_int(statement, 1)),
               sqlite3_column_int(statement, 2),
               sqlite3_column_int(statement, 3));
    }

    sqlite3_finalize(statement);

    if (tests_found == 0U)
    {
        printf("\n%s: none\n", title);
    }

    return RESULT_DATABASE_OK;
}

/**
 * @brief Prints test execution summary.
 *
 * Executes SQL queries and prints summary statistics.
 *
 * @return RESULT_DATABASE_OK on success,
 *         RESULT_DATABASE_FAIL otherwise.
 */
result_database_status_t result_database_print_summary(void)
{
    int total;
    int passed;
    int failed;
    int pending;
    int completed;
    double success_rate;

    /* SQL query used to generate the summary statistics. */
    static const char summary_sql[] =
        "SELECT "
        "COUNT(*), "
        "SUM(status='PASSED'), "
        "SUM(status='FAILED'), "
        "SUM(status='PENDING') "
        "FROM test_results;";

    /* SQL query used to retrieve pending tests. */
    static const char pending_sql[] =
        "SELECT test_id, peripheral, iterations, payload_len "
        "FROM test_results "
        "WHERE status = 'PENDING' "
        "ORDER BY test_id "
        "LIMIT 10;";

    /* SQL query used to retrieve failed tests. */
    static const char failed_sql[] =
        "SELECT test_id, peripheral, iterations, payload_len "
        "FROM test_results "
        "WHERE status = 'FAILED' "
        "ORDER BY test_id "
        "LIMIT 10;";

    sqlite3_stmt *statement;

    if (sqlite3_prepare_v2(result_database,
                           summary_sql,
                           -1,
                           &statement,
                           NULL) != SQLITE_OK)
    {
        return RESULT_DATABASE_FAIL;
    }

    if (sqlite3_step(statement) == SQLITE_ROW)
    {
        total = sqlite3_column_int(statement, 0);
        passed = sqlite3_column_int(statement, 1);
        failed = sqlite3_column_int(statement, 2);
        pending = sqlite3_column_int(statement, 3);

        completed = passed + failed;

        if (completed > 0)
            success_rate = ((double)passed * 100.0) / (double)completed;
        else
            success_rate = 0.0;

        printf("\n");
        printf("========================================\n");
        printf("         Test Execution Summary\n");
        printf("========================================\n");
        printf("Total tests : %d\n", total);
        printf("Passed      : %d\n", passed);
        printf("Failed      : %d\n", failed);
        printf("Pending     : %d\n", pending);
        printf("Success rate: %.2f%%\n", success_rate);
        printf("========================================\n");
    }
    else
    {
        sqlite3_finalize(statement);
        return RESULT_DATABASE_FAIL;
    }

    sqlite3_finalize(statement);

    if (result_database_print_tests("Pending tests", pending_sql) != RESULT_DATABASE_OK)
    {
        return RESULT_DATABASE_FAIL;
    }

    if (result_database_print_tests("Failed tests", failed_sql) != RESULT_DATABASE_OK)
    {
        return RESULT_DATABASE_FAIL;
    }

    return RESULT_DATABASE_OK;
}

/**
 * @brief Closes the SQLite database.
 */
void result_database_close(void)
{
    if (result_database != NULL)
    {
        sqlite3_close(result_database);
        result_database = NULL;
    }
}