# CANBus-Renode-Lab
### Two STM32F407 nodes on a simulated CAN bus, plus a register-level debugging case study

Node 1 (**Transmitter**) reads a BMP180 temperature sensor over I2C and sends it on CAN2 (ID `0x123`, 500 kbit/s).
Node 2 (**Receiver**) filters that ID, prints the value on USART1 and drives `PD10` high above 20.0 C.
Both run bare-metal HAL firmware in the [Renode](https://renode.io) emulator, joined by a virtual CAN hub.

## Repository layout

```text
transmitter/   main.c, BMP180.c/.h, transmitter.repl
receiver/      main.c, receiver.repl
renode/        canbus.resc
bin/           prebuilt .elf files
```

## CAN frame

| Byte | Content |
|---|---|
| 0 | Command (1 = above 20.0 C) |
| 1-2 | Temperature x10, int16, little-endian |
| 3 | Sensor status (1 = valid) |

# Debug case study: CAN2 never finished initialising

<p align="center">
  <img width="75%" src="https://github.com/user-attachments/assets/6ac71f2c-ae41-4dd6-a44e-0d58491d1876" alt="CAN2 debugging overview">
</p>

<p align="center">
  <img width="45%" src="https://github.com/user-attachments/assets/ae302ea4-2328-41f0-9103-698b7d3daaf3" alt="CAN2 register debugging">
</p>

## Tools

`arm-none-eabi-gdb` attached to Renode's GDB server (port 3333 for the Transmitter).

we debug by attaching and set breakpoints


<p align="center">
  <img width="55%" src="https://github.com/user-attachments/assets/d96baedf-7963-4fc5-a4cb-922d3e50be85" alt="GDB debugging session">
</p>

Addresses = base + offset (CAN2 base `0x40006800`, from `can2.repl`; ).

confirming  where it fails as seen in the image below it didn't initialize

<p align="center">
  <img width="55%" src="https://github.com/user-attachments/assets/f92df539-a2f0-4c9a-ac30-b154dca7471d" alt="CAN2 initialization failure">
</p>

| Address | Register |
|---|---|
| `0x40006800` | MCR (offset 0x00) |
| `0x40006804` | MSR (offset 0x04) |

| Register | Bit | Name | Meaning |
|---|---|---|---|
| MCR | 0 | INRQ | 1 = initialisation requested |
| MCR | 1 | **SLEEP** | 1 = controller asleep |
| MSR | 0 | INAK | 1 = in initialisation mode |
| MSR | 1 | **SLAK** | 1 = controller confirms it is asleep |

The reset value of MCR is `0x00010002`, so the controller **starts asleep** (SLEEP=1). `HAL_CAN_Init` must clear SLEEP and wait for SLAK to drop. If it cannot, init times out.

from the debugging we fixed in the init function `HAL_CAN_MspInit`:

After Rebuilding , we see that correct expected behavior past the CAN initialization has worked efficiently....
