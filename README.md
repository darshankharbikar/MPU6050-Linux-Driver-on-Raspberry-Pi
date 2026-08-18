# MPU6050 Linux Driver on Raspberry Pi

A Linux kernel driver project for interfacing an **MPU6050 6-axis IMU sensor** with a **Raspberry Pi 4B** over I2C.

The project demonstrates how to build a custom Linux I2C driver, describe the sensor using a Device Tree overlay, create a Linux character device, and read raw accelerometer and gyroscope data from user space.

---

## Project Overview

The MPU6050 contains:

* 3-axis accelerometer
* 3-axis gyroscope
* I2C interface
* Digital Motion Processor (DMP)

This project focuses on the **I2C + Linux kernel driver path**:

```text
                 Raspberry Pi 4B
              +-------------------+
              |                   |
              |   Linux Kernel    |
              |        |          |
              |   I2C Subsystem   |
              |        |          |
              | MPU6050 Driver    |
              |        |          |
              | /dev/mpu6050_demo |
              +--------|----------+
                       |
                      I2C
                       |
                +------+------+
                |   MPU6050   |
                |             |
                | Accel XYZ   |
                | Gyro XYZ    |
                +-------------+
```

The kernel driver communicates with the sensor through the Linux I2C/SMBus APIs and exposes the sensor data through a character device.

---

## Features

* Linux kernel I2C driver
* Device Tree based device description
* MPU6050 `WHO_AM_I` verification
* Sensor wake-up/configuration
* Raw accelerometer X/Y/Z readings
* Raw gyroscope X/Y/Z readings
* Linux character device
* `/dev/mpu6050_demo` interface
* Kernel `probe()` and `remove()` lifecycle
* Out-of-tree kernel module build using Kbuild

The driver registers the following Device Tree compatible strings:

```text
custom,mpu6050
invensense,mpu6050
```

The corresponding driver match table is implemented in the kernel driver.

---

## Hardware

### Required Components

| Component       | Description                      |
| --------------- | -------------------------------- |
| Raspberry Pi 4B | Target Linux platform            |
| MPU6050         | 6-axis accelerometer + gyroscope |
| Jumper wires    | I2C/power connections            |
| microSD card    | Raspberry Pi Linux OS            |

---

## MPU6050 I2C Connection

Typical MPU6050 connections:

| MPU6050 | Raspberry Pi 4B |
| ------- | --------------- |
| VCC     | 3.3V            |
| GND     | GND             |
| SDA     | GPIO2 / SDA1    |
| SCL     | GPIO3 / SCL1    |

The Device Tree overlay configures the MPU6050 at I2C address:

```text
0x68
```

The overlay targets the Raspberry Pi I2C controller `i2c1` and enables it.

> **Important:** Verify your particular MPU6050 breakout board's voltage requirements before connecting it to the Raspberry Pi.

---

## Repository Structure

```text
mpu6050-linux-driver/
│
├── Makefile
├── mpu6050_demo.c
├── mpu6050-demo-overlay.dts
└── README.md
```

### Files

#### `mpu6050_demo.c`

Linux kernel I2C driver.

Responsibilities:

```text
I2C device matching
       ↓
probe()
       ↓
WHO_AM_I verification
       ↓
Sensor initialization
       ↓
Character device registration
       ↓
/dev/mpu6050_demo
       ↓
read()
       ↓
Raw sensor data
```

The driver defines the MPU6050 register map, including `WHO_AM_I`, power management, accelerometer and gyroscope registers.

#### `mpu6050-demo-overlay.dts`

Device Tree overlay describing the MPU6050:

```dts
mpu6050@68 {
    compatible = "custom,mpu6050";
    reg = <0x68>;
    status = "okay";
};
```

#### `Makefile`

Builds the driver as an out-of-tree Linux kernel module using Kbuild.

```make
obj-m += mpu6050_demo.o
```

The build uses:

```text
/lib/modules/$(uname -r)/build
```

---

# Software Requirements

Recommended environment:

* Raspberry Pi 4B
* Raspberry Pi OS / Linux
* Linux kernel headers
* GCC
* GNU Make
* Device Tree compiler
* I2C tools

Install common dependencies:

```bash
sudo apt update
sudo apt install build-essential raspberrypi-kernel-headers i2c-tools device-tree-compiler
```

---

# Enable I2C

Enable I2C on the Raspberry Pi:

```bash
sudo raspi-config
```

Navigate to:

```text
Interface Options
    → I2C
        → Enable
```

Reboot:

```bash
sudo reboot
```

Verify that the I2C device exists:

```bash
ls /dev/i2c-*
```

Expected:

```text
/dev/i2c-1
```

---

# Verify MPU6050 Hardware

Run:

```bash
sudo i2cdetect -y 1
```

You should normally see the MPU6050 at:

```text
0x68
```

Example:

```text
     0 1 2 3 4 5 6 7 8 9 a b c d e f
00:          -- -- -- -- -- -- -- --
10: -- -- -- -- -- -- -- -- -- -- -- --
20: -- -- -- -- -- -- -- -- -- -- -- --
30: -- -- -- -- -- -- -- -- -- -- -- --
40: -- -- -- -- -- -- -- -- -- -- -- --
50: -- -- -- -- -- -- -- -- -- -- -- --
60: -- -- -- -- -- -- -- -- 68 -- -- --
70: -- -- -- -- -- -- -- -- -- -- -- --
```

If `0x68` does not appear, check:

1. Power supply
2. GND
3. SDA connection
4. SCL connection
5. I2C enablement
6. MPU6050 address configuration

---

# Device Tree Overlay

The project contains:

```text
mpu6050-demo-overlay.dts
```

The overlay enables the Raspberry Pi I2C controller and creates an MPU6050 device at address `0x68`.

Compile the overlay:

```bash
dtc -@ -I dts -O dtb \
    -o mpu6050-demo-overlay.dtbo \
    mpu6050-demo-overlay.dts
```

Copy it to the Raspberry Pi overlay directory:

```bash
sudo cp mpu6050-demo-overlay.dtbo /boot/overlays/
```

Depending on your Raspberry Pi OS version, the boot firmware configuration may be located under `/boot/firmware/`.

Add the overlay to the appropriate `config.txt`:

```text
dtoverlay=mpu6050-demo-overlay
```

Reboot:

```bash
sudo reboot
```

---

# Build the Kernel Driver

Clone the repository:

```bash
git clone <your-repository-url>
cd mpu6050-linux-driver
```

Build:

```bash
make
```

The Makefile invokes the kernel build system using:

```text
/lib/modules/$(uname -r)/build
```

The resulting module should be:

```text
mpu6050_demo.ko
```

---

# Load the Driver

Load the kernel module:

```bash
sudo insmod mpu6050_demo.ko
```

Check kernel messages:

```bash
dmesg | tail -n 30
```

A successful probe should report the MPU6050 `WHO_AM_I` value.

The driver expects:

```text
WHO_AM_I = 0x68
```

The probe function reads this register before continuing sensor initialization.

Check the module:

```bash
lsmod | grep mpu6050
```

---

# Device Node

After successful driver initialization:

```bash
ls -l /dev/mpu6050_demo
```

The driver dynamically allocates a character-device number, registers a `cdev`, creates a class, and creates the `/dev/mpu6050_demo` device node.

Expected:

```text
/dev/mpu6050_demo
```

---

# Sensor Initialization

During `probe()`, the driver performs the following operations:

### 1. Read `WHO_AM_I`

```text
Register: 0x75
Expected: 0x68
```

### 2. Wake up MPU6050

The driver writes:

```text
0x00 → PWR_MGMT_1
```

This clears the sensor sleep bit.

### 3. Configure Sensor

The driver configures:

```text
Sample rate
Digital low-pass configuration
Gyroscope range
Accelerometer range
```

The configured ranges are:

```text
Gyroscope:     ±250 °/s
Accelerometer: ±2 g
```

as reflected by the driver configuration values.

---

# Reading Sensor Data

The character driver's `read()` operation retrieves fresh sensor data.

The data structure contains:

```c
struct mpu6050_sensor_data {
    s16 accel_x;
    s16 accel_y;
    s16 accel_z;
    s16 gyro_x;
    s16 gyro_y;
    s16 gyro_z;
};
```

The driver reads:

```text
Accelerometer:
0x3B - 0x40

Gyroscope:
0x43 - 0x48
```

## and copies the resulting structure to user space.

# Test Program

A user-space test application can open:

```text
/dev/mpu6050_demo
```

and read:

```text
Accel X
Accel Y
Accel Z
Gyro X
Gyro Y
Gyro Z
```

Conceptually:

```c
int fd = open("/dev/mpu6050_demo", O_RDONLY);

struct mpu6050_sensor_data data;

read(fd, &data, sizeof(data));

printf("Accel: %d %d %d\n",
       data.accel_x,
       data.accel_y,
       data.accel_z);

printf("Gyro: %d %d %d\n",
       data.gyro_x,
       data.gyro_y,
       data.gyro_z);
```

---

# Data Flow

The complete data path is:

```text
MPU6050
   │
   │ I2C
   ▼
Linux I2C Controller
   │
   ▼
Linux I2C Core
   │
   ▼
mpu6050_demo Driver
   │
   ├── probe()
   ├── I2C register reads
   └── read()
   │
   ▼
Character Device
   │
   ▼
/dev/mpu6050_demo
   │
   ▼
User-space Application
```

---

# Linux Driver Architecture

This project demonstrates several important Linux driver concepts:

```text
                 Device Tree
                      │
                      ▼
              I2C Device Matching
                      │
                      ▼
                   probe()
                      │
        ┌─────────────┴─────────────┐
        │                           │
        ▼                           ▼
   Sensor Setup              Character Device
        │                           │
        │                    /dev/mpu6050_demo
        │                           │
        └─────────────┬─────────────┘
                      ▼
                    read()
                      │
                      ▼
                 User Space
```

The driver uses:

* `struct i2c_driver`
* `struct i2c_client`
* Device Tree matching
* I2C SMBus APIs
* `struct cdev`
* `file_operations`
* `copy_to_user()`
* `probe()`
* `remove()`

The driver registers its I2C driver using `module_i2c_driver()`.

---

# Useful Debug Commands

### Check I2C devices

```bash
sudo i2cdetect -y 1
```

### Check kernel logs

```bash
dmesg | grep MPU6050
```

or:

```bash
dmesg | tail -n 50
```

### Check driver

```bash
lsmod | grep mpu6050
```

### Check device node

```bash
ls -l /dev/mpu6050_demo
```

### Check I2C devices in sysfs

```bash
ls /sys/bus/i2c/devices/
```

### Check kernel driver binding

```bash
ls -l /sys/bus/i2c/drivers/
```

---

# Unload Driver

```bash
sudo rmmod mpu6050_demo
```

Verify:

```bash
dmesg | tail -n 20
```

The driver's `remove()` callback puts the MPU6050 into sleep mode and destroys the character-device resources.

---

# Clean Build

```bash
make clean
```

---

# Troubleshooting

## `mpu6050_demo.ko` fails to build

Check kernel headers:

```bash
ls -l /lib/modules/$(uname -r)/build
```

If the directory does not exist, install the appropriate Raspberry Pi kernel headers.

---

## `i2cdetect` does not show `0x68`

Check:

```text
VCC
GND
SDA
SCL
```

Then verify:

```bash
sudo raspi-config
```

and ensure I2C is enabled.

---

## `/dev/mpu6050_demo` does not exist

Check:

```bash
dmesg | grep MPU6050
```

Possible causes:

* Device Tree overlay not loaded
* Incorrect I2C address
* Sensor not detected
* `WHO_AM_I` failure
* Driver module not loaded

---

## Driver loads but probe is not called

Check the I2C device:

```bash
ls /sys/bus/i2c/devices/
```

Check the Device Tree:

```bash
sudo dtoverlay -l
```

Also verify that the overlay contains:

```dts
compatible = "custom,mpu6050";
reg = <0x68>;
```

and that the driver contains the corresponding compatible string.

---

# Learning Objectives

This project is useful for learning:

### Linux Kernel

* Kernel module development
* Kernel APIs
* `probe()` / `remove()`
* Character devices
* `file_operations`
* User/kernel-space data transfer

### I2C

* I2C device addressing
* Linux I2C subsystem
* SMBus register access
* Sensor register maps

### Device Tree

* Device Tree overlays
* `compatible`
* `reg`
* I2C device description
* Driver/device matching

### Embedded Linux

```text
Hardware
   ↓
Device Tree
   ↓
Linux Kernel
   ↓
I2C subsystem
   ↓
Device Driver
   ↓
Character Device
   ↓
User Application
```

---

# Project Limitations

This is a **learning/demo driver**, not a production MPU6050 driver.

Current implementation exposes raw sensor values through a character device.

It does not currently implement:

* Interrupt-driven data acquisition
* FIFO support
* DMP support
* Hardware timestamping
* Sensor calibration
* Orientation estimation
* Complementary filter
* Kalman filter
* `ioctl()` interface
* `poll()`/`select()` support
* Runtime power management
* Multiple MPU6050 instances
* Industrial I/O (IIO) subsystem integration

The driver also uses a global device pointer for its character-device open path, so it is intended as a simple single-device demonstration rather than a multi-device production implementation.

---

# Possible Extensions

A natural progression for this project is:

```text
Level 1
Raw MPU6050 data
        ↓
Level 2
User-space monitoring application
        ↓
Level 3
Convert raw values → physical units
        ↓
Level 4
Gyroscope + accelerometer calibration
        ↓
Level 5
Complementary filter
        ↓
Level 6
Calculate pitch / roll
        ↓
Level 7
Real-time visualization
        ↓
Level 8
Interrupt + FIFO based acquisition
        ↓
Level 9
Linux IIO driver
        ↓
Level 10
Edge-AI / gesture-recognition application
```

---

# Example Applications

Once the basic driver is working, the project can be extended into:

* Motion detector
* Tilt detector
* Digital level
* Gesture controller
* Robot orientation sensor
* Self-balancing robot
* Motion logger
* IMU data visualizer
* Fall/motion detection
* Raspberry Pi robotics project

---

# Technical Stack

| Layer                | Technology               |
| -------------------- | ------------------------ |
| Hardware             | Raspberry Pi 4B          |
| Sensor               | MPU6050                  |
| Bus                  | I2C                      |
| OS                   | Linux                    |
| Kernel               | Linux kernel             |
| Driver               | Custom I2C kernel module |
| Hardware Description | Device Tree Overlay      |
| User Interface       | Character device         |
| Device Node          | `/dev/mpu6050_demo`      |
| Build                | Kbuild / Make            |

---

# License

The kernel driver declares:

```text
GPL
```

See the `MODULE_LICENSE()` declaration in `mpu6050_demo.c`.

---

# Author

**Embedded Linux / Kernel Driver Learning Project**

---

# Project Goal

The primary goal of this project is to understand the complete path from a physical sensor to a Linux user-space application:

```text
MPU6050
   ↓
I2C
   ↓
Device Tree
   ↓
Linux I2C Core
   ↓
Custom Kernel Driver
   ↓
Character Device
   ↓
User Space
```

This makes the project suitable as a practical **Raspberry Pi + Linux Device Driver + I2C + Device Tree** learning project.
