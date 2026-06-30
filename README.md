# STM32 Peripheral Tester

## [Real Time College](https://rt-ed.co.il/) - a multi-disciplinary Real-Time O.S. and Embedded Software Solutions Center, providing consulting, development, integration, training and support solutions.<br/>

## ARM Embedded Systems Final Project

Course project developed as part of the ARM Embedded Systems course.
The project implements an automated hardware validation system for the STM32F756ZG Nucleo development board. A Linux UDP server generates test commands, sends them to the STM32 board over Ethernet, collects execution results, and displays real-time statistics.
The STM32 firmware executes independent peripheral tests for UART, SPI, I²C, ADC, and Timer modules under FreeRTOS. Multiple tests can run concurrently, demonstrating parallel peripheral verification using a client-server architecture.

---

## Technology Stack

### Hardware
- STM32 Nucleo-F756ZG

### Frameworks & Libraries
- STM32Cube HAL
- FreeRTOS (CMSIS-RTOS v2)
- lwIP
- CMSIS

### Development Tools
- STM32CubeIDE
- STM32CubeMX
- GCC Arm Embedded Toolchain

### Languages
- C
- C++ (server-side components, if applicable)

<br>

## Features

- Parallel execution of independent peripheral tests
- FreeRTOS-based firmware architecture
- UDP communication over Ethernet
- Interrupt (IT) and DMA operating modes
- Automatic PASS/FAIL verification
- Random test data generation
- CRC verification for large data packets
- Runtime execution statistics
- SQLite database for test result tracking

<br>

## Architecture

The system follows a client-server architecture: the Linux server generates UDP test commands, while the STM32 firmware dispatches them to independent peripheral test modules and sends the results back.

![Architecture](docs/images/architecture.png)

<br>

## Hardware Setup

The project was validated on an STM32 Nucleo-F756ZG development board. The peripherals are interconnected using jumper wires to enable UART, SPI, I²C, ADC, and timer self-tests.

![Hardware Setup](docs/images/running_system.jpg)

<br>

## Example Execution

The Linux server continuously generates test commands and sends them to the STM32 board. The firmware executes the requested peripheral tests and returns PASS/FAIL responses through UDP while printing diagnostic information to the serial console.

![Execution Start](docs/images/start_running.png)

The application prints a final execution summary including the total number of executed tests, passed tests, failed tests, pending responses, and the overall success rate.

![Execution Summary](docs/images/end_running_summary.png)

<br>

## Test Results Database

All executed tests are automatically stored in an SQLite database.

Each record contains:

- Test ID
- Peripheral under test
- Number of iterations
- Payload size
- Start timestamp
- Completion timestamp
- Test result (PASS / FAIL)

This allows:
- reviewing historical test results;
- filtering tests by peripheral;
- analyzing execution time;
- collecting long-term reliability statistics.

The SQLite database stores the following information for every executed test:

| Field | Description |
|-------|-------------|
| test_id | Unique test identifier |
| peripheral | Tested peripheral (UART, SPI, I²C, ADC, TIMER) |
| iterations | Number of iterations |
| payload_len | Payload size in bytes |
| sent_time | Command transmission timestamp |
| received_time | Response reception timestamp |
| result | PASS / FAIL |
| status | Response state (pending, received, failed) |

<br>

![SQLite Database](docs/images/results_database.png)

<br>

## Build

### STM32 Firmware

Open the `STM32PeripheralTester` project in **STM32CubeIDE**, select the desired build configuration (**Debug** or **Release**), then build, flash, and run the firmware.

### Linux Server

Build the Linux application:

```bash
cd STM32_Server
make
```

The executable `stm32_server` will be generated in the same directory.

Open the STM32 serial console:

```bash
sudo minicom -D /dev/ttyACM0 -b 115200
```

Run the server:

```bash
./stm32_server
```


## Author

Ilia Rakhlevski
