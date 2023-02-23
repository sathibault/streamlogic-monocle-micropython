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

### <ins>Build example FPGA design</ins>

StreamLogic uses Amazon single sign-on, so if you do not already have an account you will need to create one.

Log in to the StreamLogic for Moncole environment:
https://fpga.streamlogic.io/monocle/

Once the darkblue-themed workspace opens, **double click** the page tab as shown here:
![Tabs](images/streamlogic-tabs.png)

That will bring up the properties dialog where you can change the name and **ensure that the selected board is Monocle**, as shown here:

![Properties](images/streamlogic-pipeline-props.png)

Click **Done** to close the properties dialog.

Add the "monocle-minimal" example to the workspace by clicking on the "hamburg" menu, in the top-right corner, and following the sub-menus as shown here:
![Import](images/streamlogic-import-monocle.png)

When you click moncole-minimal, the import code is attached to your cursor and you can click to place the code anywhere in the workspace.
![Paste](images/streamlogic-monocle-minimal.png)

To build the example design, click the **Build** button at the bottom of the workspace.  You will see progress messages in the console pane, and when it completes, click the download file as shown here:
![Download](images/streamlogic-build-download.png)

### <ins>Program the Monocle's FPGA</ins>

The StreamLogic command-line helper takes care of build the FPGA bitmap (binary file required to program the FPGA) from the file you just downloaded from the StreamLogic development environment.  Use the following command to build the bitmap:
```
$ python -m sxlogic.monocle build <download filename>
```

It should produce output something like this:
```
Extracting into mono1-monocle_fpga-build ...
Building bitstream ...
SUCCESS! (log in mono1-monocle_fpga-build\hw\build.log)
BUILT mono1-monocle_fpga-build\hw\streamlogic_proj.fs
```

That `.fs` file is the FPGA bitmap.

To program the FPGA, the Monocle must be awake, so take it off the charger and tap to wake it up.  Next run the Gowin Programmer, and follow these steps:
1. Use the Edit->Add Device menu if the GW1N device is not listed.
1. Double-click the Operation column of the device row, and select Embedded Flash Mode, for the Access Mode, and embFlash Erase, Program for the Operation.  Click the ... button to select the `.fs` file we just produced, and click Save.

It should now look something like this:
![Gowin programmer](images/gowin-programmer.png)

Click the green play-like button to program the FPGA, and your done!

## Updating the Monocle's firmware

The pre-installed firmware on the Monocle will not work with the FPGA programs produced by StreamLogic because FPGA <-> MicroController API is not the same.  Follow these instructions to update the Monocle's firmware.

1. Download the latest `.hex` file from the [releases page](https://github.com/sathibault/streamlogic-monocle-micropython/releases).

1. Connect your debugger as described [here](https://docs.brilliantmonocle.com/monocle/monocle/#manually-programming).

1. Flash your device using the command:

```sh
nrfjprog --program *.hex --chiperase -f nrf52 --verify --reset
```
