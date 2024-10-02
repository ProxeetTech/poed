
# PoE Daemon (PoED) - README

## Introduction

The PoE Daemon (PoED) is a background service that monitors and manages Power over Ethernet (PoE) ports on your system. It allows you to control budgets, collect power usage data, and communicate with the daemon through a Unix socket. The daemon can also run in test mode, simulating PoE data for testing purposes.

This README provides instructions for building, running, and using the PoED service.

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

| Argument                | Description                                                                 |
|-------------------------|-----------------------------------------------------------------------------|
| `-p, --monitor-period`   | Sets the PoE ports monitoring period in microseconds (default is 1000000 us). |
| `-t`                    | Enables test mode, simulating PoE port data.                                |
| `-g, --get-all`          | Prints all PoE data in JSON format to stdout (requires the daemon to be running). |
| `-h, --help`             | Displays help information for the available command-line options.           |

### Example Usage:

1. **Start the daemon** with the default monitoring period (1 second):

    ```bash
    ./poed
    ```

2. **Start the daemon in test mode**:

    ```bash
    ./poed -t
    ```

3. **Specify a custom monitoring period (e.g., 500ms)**:

    ```bash
    ./poed --monitor-period 500000
    ```

4. **Get all PoE data in JSON format** (requires the daemon to be running):

    ```bash
    ./poed --get-all
    ```

5. **Show help information**:

    ```bash
    ./poed --help
    ```

## Unix Socket Communication

When the Unix socket server is enabled in the configuration (`unix_socket_enable = 1`), you can interact with the PoE daemon using the Unix socket specified in the configuration file. By default, the socket path is `/var/run/poed.sock`.

### Example of Unix Socket Interaction

Note that all the messages for socket communication have 3 fields:
`msg_type` - ***request*** or ***response***;
`data` - payload of the message;
`error_msg` - error message if present;

To request PoE data from the daemon, you can send the message `"get_all"` using the Unix socket. The daemon will respond with all PoE data in JSON format.

You can use a client tool (or write your own) to communicate with the Unix socket. Below is an example of how to send a request using a tool like `socat`:

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
                    "current": 0.0,
                    "enable_flag": true,
                    "index": 0,
                    "mode": "mode",
                    "name": "eth9",
                    "overbudget_flag": false,
                    "power": 0.0,
                    "priority": 1,
                    "state": "6(OPEN)",
                    "voltage": 0.0
                },
                {
                    "budget": 15.0,
                    "current": 0.0,
                    "enable_flag": true,
                    "index": 1,
                    "mode": "mode",
                    "name": "eth10",
                    "overbudget_flag": false,
                    "power": 0.0,
                    "priority": 2,
                    "state": "6(OPEN)",
                    "voltage": 0.0
                },
                {
                    "budget": 15.0,
                    "current": 0.492,
                    "enable_flag": true,
                    "index": 2,
                    "mode": "mode",
                    "name": "eth11",
                    "overbudget_flag": false,
                    "power": 12.1524,
                    "priority": 2,
                    "state": "4(DET_OK)",
                    "voltage": 24.7
                },
                {
                    "budget": 15.0,
                    "current": 0.364,
                    "enable_flag": true,
                    "index": 3,
                    "mode": "mode",
                    "name": "eth12",
                    "overbudget_flag": false,
                    "power": 8.881599999999999,
                    "priority": 2,
                    "state": "4(DET_OK)",
                    "voltage": 24.4
                }
            ],
            "total_budget": 120.0,
            "total_power": 21.034
        },
        {
            "ports": [
                {
                    "budget": 15.0,
                    "current": 0.362,
                    "enable_flag": true,
                    "index": 0,
                    "mode": "mode",
                    "name": "eth13",
                    "overbudget_flag": false,
                    "power": 8.760399999999999,
                    "priority": 2,
                    "state": "4(DET_OK)",
                    "voltage": 24.2
                },
                {
                    "budget": 15.0,
                    "current": 0.0,
                    "enable_flag": true,
                    "index": 1,
                    "mode": "mode",
                    "name": "eth14",
                    "overbudget_flag": false,
                    "power": 0.0,
                    "priority": 2,
                    "state": "6(OPEN)",
                    "voltage": 0.0
                },
                {
                    "budget": 15.0,
                    "current": 0.0,
                    "enable_flag": true,
                    "index": 2,
                    "mode": "mode",
                    "name": "eth15",
                    "overbudget_flag": false,
                    "power": 0.0,
                    "priority": 2,
                    "state": "6(OPEN)",
                    "voltage": 0.0
                },
                {
                    "budget": 15.0,
                    "current": 0.361,
                    "enable_flag": true,
                    "index": 3,
                    "mode": "mode",
                    "name": "eth16",
                    "overbudget_flag": false,
                    "power": 8.8445,
                    "priority": 2,
                    "state": "4(DET_OK)",
                    "voltage": 24.5
                }
            ],
            "total_budget": 120.0,
            "total_power": 17.6049
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
sudo tail -f /var/log/syslog
```

## Troubleshooting

### Common Issues:

1. **Daemon Already Running**:
    - If you try to start the daemon while it's already running, you will receive an error message in the logs.

   Solution: Ensure that no other instance of the PoE daemon is running before starting a new one.

2. **Unix Socket Server Disabled**:
    - If the Unix socket server is disabled in the configuration file, you cannot use the `--get-all` option to retrieve data.

   Solution: Enable the Unix socket server in the configuration file and restart the daemon.

3. **Missing Configuration File**:
    - If the UCI configuration file is missing, the daemon will generate a default configuration.

   Solution: Edit the configuration file `/etc/config/poed` to suit your hardware environment.

### Exiting the Daemon:

To stop the daemon, send a termination signal (e.g., using `kill`):

```bash
sudo killall poed
```