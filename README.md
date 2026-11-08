# CANBus-Renode-Lab
### Two STM32F407 nodes on a simulated CAN bus, plus a register-level debugging case study

Node 1 (**Transmitter**) reads a BMP180 temperature sensor over I2C and sends it on CAN2 (ID `0x123`, 500 kbit/s).
Node 2 (**Receiver**) filters that ID, prints the value on USART1 and drives `PD10` high above 20.0 C.
Both run bare-metal HAL firmware in the [Renode](https://renode.io) emulator, joined by a virtual CAN hub.

## Repository layout

```
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
<img width="906" height="413" alt="image" src="https://github.com/user-attachments/assets/6ac71f2c-ae41-4dd6-a44e-0d58491d1876" />

## Symptom<img width="438" height="464" alt="image" src="https://github.com/user-attachments/assets/ae302ea4-2328-41f0-9103-698b7d3daaf3" />

The Receiver printed its banner, then stopped. No frames were received. The CPU was parked in `Error_Handler()` because `HAL_CAN_Start()` returned an error.

## Tools
`arm-none-eabi-gdb` attached to Renode's GDB server (port 3333 for the Transmitter).

## Step 0: prepare Renode
Comment out the final `start` line in `canbus.resc` so the CPU is held, then run `include`. Renode's GDB server starts the emulation when gdb issues `continue`.

## Step 1: attach and set breakpoints
```
arm-none-eabi-gdb receiver/Debug/can2.elf
(gdb) target remote localhost:3334
(gdb) break HAL_CAN_Init
(gdb) break HAL_CAN_Start
(gdb) break Error_Handler
(gdb) continue
```



Addresses = base + offset (CAN2 base `0x40006800`, from `can2.repl`; offsets from RM0090).

| Address | Register |
|---|---|
| `0x40006800` | MCR (offset 0x00) |
| `0x40006804` | MSR (offset 0x04) |
| `0x40023840` | RCC_APB1ENR (RCC base 0x40023800 + 0x40) |



How to read the values:

| Register | Bit | Name | Meaning |
|---|---|---|---|
| MCR | 0 | INRQ | 1 = initialisation requested |
| MCR | 1 | **SLEEP** | 1 = controller asleep |
| MSR | 0 | INAK | 1 = in initialisation mode |
| MSR | 1 | **SLAK** | 1 = controller confirms it is asleep |
| APB1ENR | 25 / 26 | CAN1EN / CAN2EN | peripheral clocks (CAN2 needs BOTH) |

The reset value of MCR is `0x00010002`, so the controller **starts asleep** (SLEEP=1). `HAL_CAN_Init` must clear SLEEP and wait for SLAK to drop. If it cannot, init times out.

Record your values here:

```
MCR  = 0x________   SLEEP = _
MSR  = 0x________   SLAK  = _
APB1ENR bits 26:25 = __
```
Screenshot: `docs/img/02-registers-before.png`

## Step 3: step through init and check the result


| Item | Meaning |
|---|---|
| `$r0` / return value | 0 = HAL_OK, 1 = HAL_ERROR |
| `hcan2.State` | 1 = READY, 5 = ERROR |
| `ErrorCode` | `0x20000` TIMEOUT, `0x80000` NOT_READY, `0x100000` NOT_STARTED |

If SLEEP stays 1 / SLAK stays 1 and ErrorCode is `0x20000`, the controller never woke up, so `HAL_CAN_Start` refuses to run. Screenshot: `docs/img/03-registers-after.png`

## Step 4: confirm where it fails
```
(gdb) continue
(gdb) bt
(gdb) frame 1
(gdb) list
```
`bt` shows `Error_Handler` called from `main.c` at the `HAL_CAN_Start` check. Screenshot: `docs/img/04-backtrace.png`

## Step 5: root cause and fix
Fill in what the registers showed. The usual causes, in order of likelihood:

1. **CAN clocks off.** CAN2 is a slave of CAN1 and needs `CAN1EN` and `CAN2EN` set in `RCC_APB1ENR`. Fix in `HAL_CAN_MspInit` (or the init function):
   ```c
   __HAL_RCC_CAN1_CLK_ENABLE();
   __HAL_RCC_CAN2_CLK_ENABLE();
   ```
2. **Init order or timeout.** The filter or start call ran before init finished.
3. **Model detail in Renode.** Check `logLevel -1 sysbus.can2` for which write was ignored.

## Step 6: verify the fix
Rebuild, rerun, repeat steps 1-3. Expected:

```
MCR.SLEEP = 0, MSR.SLAK = 0, hcan2.State = 1 (READY), ErrorCode = 0
```
Then `continue`. The Receiver prints `[OK] CAN2 started` and `Message received`. Screenshot: `docs/img/05-fixed.png`, `docs/img/06-receiver-output.png`

## Bus-level check
```
(Renode) logLevel -1 canHub
```
shows each frame from the Transmitter (`Id=291` is `0x123`) arriving at the hub.

---

## Register debugging cheat sheet
```
x/wx  <addr>              one 32-bit word, hex
x/wt  <addr>              binary (best for flags)
x/5wx <base>              five registers from the base
print/x *(CAN_TypeDef*)0x40006800    all named fields
print/x hcan2.Instance->MSR
print (reg >> bit) & 1    test one bit
```
`address = base + offset`, and registers are 4 bytes apart.
