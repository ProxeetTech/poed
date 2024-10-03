
# PoE Daemon (poed) - README

### Author 
Aleksei Vasilenko, a.vasilenko@proxeet.com

## Introduction

The PoE Daemon (***poed***) is a background service that monitors and manages Power over Ethernet (PoE) ports on your system. It allows you to control budgets, collect power usage data, and communicate with the daemon through a Unix socket. The daemon can also run in test mode, simulating PoE data for testing purposes.

This README provides instructions for building, running, and using the **poed** service.

## Features

- **PoE Budget Control**: Monitors and controls power usage on PoE ports, turning off ports when their power exceeds the configured limits.
- **Unix Socket Communication**: You can interact with the daemon using Unix sockets to request data about the PoE ports in JSON format.
- **Test Mode**: A simulated environment where no actual PoE devices are required.
- **Logging**: Logs system events and power data to `syslog` with configurable log levels.

## Requirements

- C++11 or higher
- `libuci` - Library to interact with UCI configuration in OpenWRT environments
- POSIX-compliant environment for Unix socket communication

## Building

To build the PoE daemon, ensure that the necessary dependencies are installed (such as `libuci`, `nlohmann::json`, and `clipp`).

```bash
mkdir build
cd build
cmake ..
make
```

This will generate the `poed` binary.

## Configuration

The PoE daemon reads its configuration from the UCI file located at `/etc/config/poed`. If the configuration file does not exist, the daemon will automatically generate a default configuration. You can modify the configuration to suit your hardware environment.

The configuration defines controllers, their power budgets, port configurations, and system settings, including the Unix socket server's behavior.

### Example Configuration:

```sh
config general
    option log_level 'debug'
    option unix_socket_enable '1'
    option unix_socket_path '/var/run/poed.sock'

config controller
    option path '/sys/bus/i2c/devices/i2c-8/8-002c'
    option ports '4'
    option total_power_budget '120'

config port
    option name 'eth1'
    option controller '0'
    option port_number '1'
    option power_budget '15'
    option mode 'AUTO'
    option priority '1'
```

## Usage

### Command-line Arguments

| Argument               | Description                                                                       |
|------------------------|-----------------------------------------------------------------------------------|
| `-p, --monitor-period` | Sets the PoE ports monitoring period in microseconds (default is 1000000 us).     |
| `-t`                   | Enables test mode, simulating PoE port data.                                      |
| `-d`                   | Run in background as a daemon                                                     |
| `-g, --get-all`        | Prints all PoE data in JSON format to stdout (requires the daemon to be running). |
| `-h, --help`           | Displays help information for the available command-line options.                 |

### Example Usage:

1. Start the daemon with the default monitoring period (1 second) in current terminal:

    ```bash
    ./poed
    ```

2. Start the daemon in test mode in current terminal:

    ```bash
    ./poed -t
    ```

3. Specify a custom monitoring period (e.g., 500ms) in current terminal:

    ```bash
    ./poed --monitor-period 500000
    ```

4. Get all PoE data in JSON format to stdout (requires the daemon to be running):

    ```bash
    ./poed --get-all
    ```

5. Show help information:

    ```bash
    ./poed --help
    ```

6. Run daemon in the background:

    ```bash
    ./poed -d
    ```

## Unix Socket Communication

When the Unix socket server is enabled in the configuration (`unix_socket_enable = 1`), you can interact with the PoE daemon using the Unix socket specified in the configuration file. By default, the socket path is `/var/run/poed.sock`.

### Example of Unix Socket Interaction

Note that all the messages for socket communication have 3 fields:
`msg_type` - ***request*** or ***response***;
`data` - payload of the message;
`error_msg` - error message if present;

To request PoE data from the daemon, you can send the message `"get_all"` using the Unix socket. The daemon will respond with all PoE data in JSON format.

***Fast debug:***
You can use a client tool to communicate with the Unix socket. Below is an example of how to send a request using a tool like `socat`:

```bash
echo '{"msg_type": "request", "data": "get_all"}' | socat - UNIX-CONNECT:/var/run/poed.sock
```

The daemon will return a JSON response containing the current PoE status of all ports.

```json
{
   "data": [
      {
         "ports": [
            {
               "budget": 15.0,
               "current": 0.126,
               "enable_flag": true,
               "index": 0,
               "load_class": "6(0)", //0(Unknown), 1(1), 2(2), 3(3), 4(4), 5(5), 6(0), 7(Current limit)
               "mode": "AUTO", //AUTO, 48V, 24V, OFF
               "name": "eth9",
               "overbudget_flag": false,
               "power": 6.0858,
               "priority": 1,
               "state": "4(DET_OK)", //0(NONE), 1(DCP), 2(HIGH_CAP), 3(RLOW), 4(DET_OK), 5(RHIGH), 6(OPEN), 7(DCN)
               "voltage": 48.3
            },
            {
               "budget": 15.0,
               "current": 0.134,
               "enable_flag": true,
               "index": 1,
               "load_class": "6(0)",
               "mode": "AUTO",
               "name": "eth10",
               "overbudget_flag": false,
               "power": 6.4052,
               "priority": 2,
               "state": "4(DET_OK)",
               "voltage": 47.8
            },
            {
               "budget": 15.0,
               "current": 0.075,
               "enable_flag": true,
               "index": 2,
               "load_class": "6(0)",
               "mode": "AUTO",
               "name": "eth11",
               "overbudget_flag": false,
               "power": 3.5999999999999996,
               "priority": 2,
               "state": "4(DET_OK)",
               "voltage": 48.0
            },
            {
               "budget": 15.0,
               "current": 0.131,
               "enable_flag": true,
               "index": 3,
               "load_class": "6(0)",
               "mode": "AUTO",
               "name": "eth12",
               "overbudget_flag": false,
               "power": 6.301100000000001,
               "priority": 2,
               "state": "4(DET_OK)",
               "voltage": 48.1
            }
         ],
         "total_budget": 120.0,
         "total_power": 22.392100000000003
      },
      {
         "ports": [
            {
               "budget": 15.0,
               "current": 0.128,
               "enable_flag": true,
               "index": 0,
               "load_class": "6(0)",
               "mode": "AUTO",
               "name": "eth13",
               "overbudget_flag": false,
               "power": 6.041600000000001,
               "priority": 2,
               "state": "4(DET_OK)",
               "voltage": 47.2
            },
            {
               "budget": 15.0,
               "current": 0.143,
               "enable_flag": true,
               "index": 1,
               "load_class": "6(0)",
               "mode": "AUTO",
               "name": "eth14",
               "overbudget_flag": false,
               "power": 6.7353,
               "priority": 2,
               "state": "4(DET_OK)",
               "voltage": 47.1
            },
            {
               "budget": 15.0,
               "current": 0.106,
               "enable_flag": true,
               "index": 2,
               "load_class": "6(0)",
               "mode": "AUTO",
               "name": "eth15",
               "overbudget_flag": false,
               "power": 5.194,
               "priority": 2,
               "state": "4(DET_OK)",
               "voltage": 49.0
            },
            {
               "budget": 15.0,
               "current": 0.134,
               "enable_flag": true,
               "index": 3,
               "load_class": "6(0)",
               "mode": "AUTO",
               "name": "eth16",
               "overbudget_flag": false,
               "power": 6.4856,
               "priority": 2,
               "state": "4(DET_OK)",
               "voltage": 48.4
            }
         ],
         "total_budget": 120.0,
         "total_power": 24.4565
      }
   ],
   "error_msg": "",
   "msg_type": "response"
}

```

The `data` field of `get_all` request in the example contains JSON, with 2 arrays with 4 ports in each as there are 2 poe controllers with 4 ports each

## Logging

The PoE daemon uses `syslog` for logging. The logging level is configurable via the UCI configuration file. The available log levels are:
- `debug`
- `info`
- `notice`
- `warning`
- `err`
- `crit`
- `alert`
- `emerg`

To view the logs, use the following command (on a typical Linux system):

```bash
tail -f /var/log/syslog
cat /var/log/syslog | grep poed
```

Or on OpenWRT:
```bash
cat logread | grep poed
```

## Troubleshooting

### Exiting the Daemon:

To stop the daemon, send a termination signal (e.g., using `kill`), from root user:

```bash
killall poed
```

### Debug in test mode with simulated PoE data:
Run in test mode in the background
```bash
poed -t -d
```

Get PoE data to stdout (with test mode flag as well)

```bash
poed -t -g
```

Use it for example for debugging on VM where is no PoE driver present. 
Use test flag here because if the platform has no PoE driver - the config validation will fail, and in test mode config validation is skipping as well