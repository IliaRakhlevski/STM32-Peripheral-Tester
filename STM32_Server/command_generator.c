#include "command_generator.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>


/* Random command generation limits. */
#define COMMAND_GENERATOR_MIN_ITERATIONS    10U
#define COMMAND_GENERATOR_MAX_ITERATIONS    255U
#define COMMAND_GENERATOR_MIN_PATTERN_LEN   50U
#define COMMAND_GENERATOR_MAX_PATTERN_LEN   APP_PROTOCOL_PATTERN_MAX_LEN

/**
 * Identifier assigned to the next generated test command.
 */
static uint32_t command_generator_next_test_id = 1U;

/**
 * Index of the next peripheral to be tested.
 * Used to cycle through all supported peripherals.
 */
static uint8_t command_generator_peripheral_index = 0U;

/**
 * List of peripherals used by the command generator.
 * Commands are generated in round-robin order to ensure
 * that every peripheral is tested regularly.
 */
static const uint8_t command_generator_peripherals[] =
{
    APP_PROTOCOL_PERIPHERAL_TIMER,
    APP_PROTOCOL_PERIPHERAL_UART,
    APP_PROTOCOL_PERIPHERAL_SPI,
    APP_PROTOCOL_PERIPHERAL_I2C,
    APP_PROTOCOL_PERIPHERAL_ADC
};

/**
 * @brief Initializes command generator.
 *
 * Initializes random seed and internal generator state.
 */
void command_generator_init(void)
{
    srand((unsigned int)time(NULL));

    command_generator_next_test_id = 1U;
    command_generator_peripheral_index = 0U;
}

/**
 * @brief Returns random value in inclusive range.
 *
 * @param[in] min_value Minimum returned value.
 * @param[in] max_value Maximum returned value.
 *
 * @return Random value in the requested range.
 */
uint8_t command_generator_random_range(uint8_t min_value, uint8_t max_value)
{
    return (uint8_t)(min_value + (rand() % ((max_value - min_value) + 1U)));
}

/**
 * @brief Builds next test command.
 *
 * Generates a command with incrementing test ID, rotating peripheral,
 * random iteration count and random payload length.
 *
 * @param[out] command Pointer to command structure to fill.
 */
void command_generator_build_next(app_protocol_command_t *command)
{
    uint16_t i;

    if (command == NULL)
    {
        return;
    }

    memset(command, 0, sizeof(*command));

    command->test_id = command_generator_next_test_id++;
    command->peripheral = command_generator_peripherals[command_generator_peripheral_index];

    command_generator_peripheral_index++;

    if (command_generator_peripheral_index >= (sizeof(command_generator_peripherals) / sizeof(command_generator_peripherals[0])))
    {
        command_generator_peripheral_index = 0U;
    }

    command->iterations = command_generator_random_range(COMMAND_GENERATOR_MIN_ITERATIONS, COMMAND_GENERATOR_MAX_ITERATIONS);

    switch (command->peripheral)
    {
        case APP_PROTOCOL_PERIPHERAL_ADC:
        case APP_PROTOCOL_PERIPHERAL_TIMER:
            command->pattern_len = 0U;
            break;

        case APP_PROTOCOL_PERIPHERAL_UART:
        case APP_PROTOCOL_PERIPHERAL_SPI:
        case APP_PROTOCOL_PERIPHERAL_I2C:
            command->pattern_len = command_generator_random_range(COMMAND_GENERATOR_MIN_PATTERN_LEN, COMMAND_GENERATOR_MAX_PATTERN_LEN);
            break;
    }

    for (i = 0U; i < command->pattern_len; i++)
    {
        command->pattern[i] = (uint8_t)(rand() & 0xFFU);
    }
}