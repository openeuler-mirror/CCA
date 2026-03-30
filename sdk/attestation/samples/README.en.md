# Attestation Samples

This is a simple demo to show CCA attestation, consisting of a client and a server. The server runs in CVM while the client runs locally, e.g., user machine. Here the client simulated a user with a local verifier and key management.

## Preparation

1. Compile the attestation sample codes

```shell
cmake -S . -B build
cmake --build build
```

The built server and client will be generated in the `build` directory. Please note that they are provided for reference only. 

In this sample, the server get device certificate and attestation report, while the client parses and verifies them. If a CVM boots via firmware (Grub boot), the server collects event logs to support attestation. The server and client use TCP for data transmission.

## Usage

The server supports parameters as below. 

```shell
$ ./server -h
Usage: server [options]
Options:
	-i, --ip <ip>                      Listening IP address
	-p, --port <port>                  Listening tcp port
	-h, --help                         Print Help (this message) and exit

```

The client supports parameters as below.

```shell
Usage: client [options]
Options:
	-i, --ip <ip>                      Listening IP address
	-p, --port <port>                  Listening tcp port
	-m, --measurement <measurement>    Initial measurement for cVM
	-f, --firmware <json>              Enable firmware verification with JSON reference file
	-e, --eventlog                     Dump event log
	-h, --help                         Print Help (this message) and exit
```

Example Commands

- Run attestation samples for direct kernel boot:

```shell
./server -i 127.0.0.1 -p 12345
```

```shell
./client -i 127.0.0.1 -p 12345 -m 38d644db0aeddedbf9e11a50dd56fb2d0c663f664d63ad62762490da41562108 
```

- Run attestation samples for grub boot:

```shell
./server -i 127.0.0.1 -p 12345
```

```shell
./client -i 127.0.0.1 -p 12345 -m 38d644db0aeddedbf9e11a50dd56fb2d0c663f664d63ad62762490da41562108 -f image_reference_measurement.json
```

## Workflow

### Interaction between Client & Server

Below outlines the interaction between the client and the server:

```mermaid
sequenceDiagram
  participant Client as Client
  participant Server as Server

  Client ->> Client: Export Reference Values of Measurements
  Client ->> Server: Send DEVICE_CERT_MSG_ID request
  Server ->> Client: Return Device Certificate
  Client ->> Server: Send ATTEST_MSG_ID with random Challenge
  Server ->> Client: Return Attestation Token
  alt Grub Boot (Firmware-Only Boot)
    Client ->> Server: Send CCEL_ACPI_TABLE_ID request
    Server ->> Client: Return CCEL ACPI Table Data
    Client ->> Server: Send CCEL_EVENT_LOG_ID request
    Server ->> Client: Return CC Event Log Data
  end
  Client ->> Client: Verify Device Certificate and Attestation Token
  Client ->> Server: Send Verification Result (VERIFY_SUCCESS_MSG_ID / VERIFY_FAILED_MSG_ID)
  alt Full Disk Encryption Enabled
    Client ->> Server: Send Rootfs Key File Data
  end
```

### Verify Attestation Token

The detailed process for "Verify Device Certificate and Attestation Token" is shown as follows. Please refer to `verify_token` in client.c to see code details.

```mermaid
flowchart TD
    A[Verify Certificate Chain]
    A --> B[Verify Token Signature]
    B --> C[Check Challenge and RIM]
    C -- Direct Kernel Boot --> D[Pass Verification]
    C -- Grub Boot --> E[Parse & Replay Event Log]
    E --> F[Compare Token REMs with Replayed REMs]
    F --> G[Extract & Verify Firmware States]
    G --> D
```
