# Supported RPC Methods

## Overview

In this article we will discuss about the supported TR-069 standard RPCs.
The following tables provides a summary of all methods, and indicates the conditions under which implementation of each RPC method defined in Annex A is `REQUIRED` or `OPTIONAL`.

### Methods for CPE responding

| Method name              | CPE requirement | Supported |
| ------------------------ | --------------- | --------- |
| `GetRPCMethods`          | `REQUIRED`      | `Yes`     |
| `SetParameterValues`     | `REQUIRED`      | `Yes`     |
| `GetParameterValues`     | `REQUIRED`      | `Yes`     |
| `GetParameterNames`      | `REQUIRED`      | `Yes`     |
| `SetParameterAttributes` | `REQUIRED`      | `Yes`     |
| `GetParameterAttributes` | `REQUIRED`      | `Yes`     |
| `AddObject`              | `REQUIRED`      | `Yes`     |
| `DeleteObject`           | `REQUIRED`      | `Yes`     |
| `Reboot`                 | `REQUIRED`      | `Yes`     |
| `Download`               | `REQUIRED`      | `Yes`     |
| `ScheduleDownload`       | OPTIONAL        | `Yes`     |
| `Upload`                 | OPTIONAL        | `Yes`     |
| `FactoryReset`           | OPTIONAL        | `Yes`     |
| `GetQueuedTransfers`     | OPTIONAL        | No        |
| `GetAllQueuedTransfers`  | OPTIONAL        | No        |
| `CancelTransfer`         | OPTIONAL        | `Yes`     |
| `ScheduleInform`         | OPTIONAL        | `Yes`     |
| `ChangeDUState`          | OPTIONAL        | `Yes`     |
| `SetVouchers`            | OPTIONAL        | No        |
| `GetOptions`             | OPTIONAL        | No        |


### Methods for CPE calling

| Method name                       | CPE requirement | Supported |
| --------------------------------- | --------------- | --------- |
| `GetRPCMethods`                   | OPTIONAL        | `Yes`     |
| `Inform`                          | `REQUIRED`      | `Yes`     |
| `TransferComplete`                | `REQUIRED`      | `Yes`     |
| `AutonomousTransferComplete`      | OPTIONAL        | `Yes`     |
| `DUStateChangeComplete`           | OPTIONAL        | `Yes`     |
| `AutonomousDUStateChangeComplete` | OPTIONAL        | `Yes`     |
| `RequestDownload`                 | OPTIONAL        | No        |
| `Kicked`                          | OPTIONAL        | No        |
