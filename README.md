# StreamLogic+MicroPython for Monocle

This document describes how to use StreamLogic FPGA design for the Monocle together with MicroPython firmware.

Many functions of the Moncole are controlled by the FPGA, and the FPGA exposes an API to the Moncole's microcontroller, so that you can control them from software.  The following section describe the process for using StreamLogic and the corresponding MicroPython firmware.

## Software requirements

You will need to install a number of tools to build the FPGA bitmap and MCU/FPGA programming.

1. For firmware programming, install the [nRF Command Line Tools](https://www.nordicsemi.com/Products/Development-tools/nrf-command-line-tools).

1. For FPGA build and programming, install the [Gowin IDE](https://www.gowinsemi.com/en/support/download_eda/).

1. Install the StreamLogic command-line helper utilities (optional, requires Python3): `pip install sxlogic`

## StreamLogic FPGA design walkthrough

In this section, we'll walk through the steps to build and program the Monocle with a sample design.

StreamLogic uses Amazon single sign-on, so if you do not already have an account you will need to create one.

Log in to the StreamLogic for Moncole environment:
https://fpga.streamlogic.io/monocle/

Once the darkblue-themed workspace opens, **double click** the page tab as shown here:
![Tabs](images/streamlogic-tabs.png)

That will bring up the properties dialog where you can change the name and **ensure that the selected board is Monocle**.
![Properties](images/streamlogic-pipeline-props.png)
Click Done to close the properties dialog.

Add the "monocle-minimal" example to the workspace by clicking on the "hamburg" menu, in the top-right corner, and following the sub-menus as shown here:
![Import](images/streamlogic-import-monocle.png)

When you click moncole-minimal, the import code is attached to your cursor and you can click to place the code anywhere in the workspace.
![Paste](images/streamlogic-monocle-minimal.png)

To build the example design, click the Build button at the bottom of the workspace.  You will see progress messages in the console pane, and when it completes, click the download file as shown here:
![Download](images/streamlogic-build-download.png)


## Updating the Monocle's firmware

1. Download the latest `.hex` file from the [releases page](https://github.com/sathibault/streamlogic-monocle-micropython/releases).

1. Connect your debugger as described [here](https://docs.brilliantmonocle.com/monocle/monocle/#manually-programming).

1. Flash your device using the command:

```sh
nrfjprog --program *.hex --chiperase -f nrf52 --verify --reset
```
