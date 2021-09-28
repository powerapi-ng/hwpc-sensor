# Hardware Performance Counters (HwPC) Sensor

HardWare Performance Counter (HWPC) Sensor is a tool that monitor the Intel CPU
performance counter and the power consumption of CPU.

Hwpc-sensor use the RAPL (Running Average Power Limit) technology to monitor CPU
power consumption. This technology is only available on Intel Sandy Bridge
architecture or higher.

The sensor use the perf API of the Linux kernel. It is only available on Linux
and need to have root access to be used.

The sensor couldn’t be used in a virtual machine, it must access (via Linux
kernel API) to the real CPU register to read performance counter values.

# About

HwPC sensor is an open-source project developed by the [Spirals research
group](https://team.inria.fr/spirals) (University of Lille 1 and Inria).

The documentation is available [here](http://powerapi.org)
