# Modbus RX timeout – spec for isolated firmware test

## Purpose

Verify when the **~2 s** serial read timeout applies: only when the sensor is disconnected (no response), or also after a response is received.

## Expected behaviour (design)

- **`rx_timeout_ticks`** (2000 ticks ≈ 2 s) in `modbussensor.c` is the **maximum** time a single `serial_read()` call may block waiting for the next byte.
- **Sensor connected**: The stack requests a fixed number of response bytes. As soon as all bytes are received, `serial_read()` returns. We do **not** wait 2 s after receiving the response. Read duration should be on the order of frame time (request + response at 4800 baud), typically well under 2 s.
- **Sensor disconnected**: No (or insufficient) bytes arrive. `serial_read()` blocks until `rx_timeout_ticks` elapses, then returns with whatever was read (partial or zero). So a failed read can take up to ~2 s per read call (and the Modbus stack may make multiple read calls per transaction).

## Interfaces involved

| Component        | Role |
|-----------------|------|
| `serial_read()` | Byte-by-byte read; returns when `count` bytes received **or** when `FLEX_TickGet() > end_ticks` (2000 ticks from start of call). |
| `FLEX_TickGet()`| 1000 ticks per second (Flex SDK). |
| Modbus stack    | Calls `serial_read()` one or more times per Read Input Registers transaction. |

So: **2 s delay applies only when waiting for bytes that never come (e.g. sensor disconnected).** With sensor connected, we return as soon as the response is complete.

## Isolated firmware test

Use a minimal build that runs **only** Modbus read timing, with clear prompts so the operator can test both scenarios.

### Build / config

- Same codebase as main app; use `MODBUS_DIAGNOSTIC_TEST` or a dedicated define (e.g. `MODBUS_RX_TIMEOUT_TEST 1`) so the firmware runs a small, dedicated test instead of the full application.
- Test code should:
  - Call `Modbus_Init()` once.
  - Perform exactly **one** temperature read and measure duration (e.g. `t0 = FLEX_TickGet()`, read, `t1 = FLEX_TickGet()`, print `t1 - t0` ticks and ms).
  - Print instructions for the operator (connect vs disconnect sensor).

### Test cases

| # | Sensor state   | Operator action                    | Expected duration (ticks / ms) | Pass criteria |
|---|----------------|------------------------------------|---------------------------------|---------------|
| 1 | **Connected**  | Plug sensor, power on, run test    | Clearly **&lt; 2000** ticks     | Read OK, duration &lt; ~2000 ticks (~2 s) |
| 2 | **Disconnected** | No sensor (or unplug before run) | Up to **~2000** ticks per read attempt | Read fails; duration on the order of 2000 ticks (or sum of multiple read timeouts if stack retries) |

### Procedure

1. **Build** with `MODBUS_RX_TIMEOUT_TEST 1`: use meson `-Dmodbus_rx_timeout_test=true` or set the define in `src/main.c`.
2. **Flash** and open serial console (e.g. `./flash-and-listen.sh /dev/cu.usbmodem1101`).
3. **Test 1 – Connected**  
   - Connect sensor (cable in, powered by board).  
   - Reset or run so the firmware does one Modbus read and prints duration.  
   - Record: result (OK/fail), duration in ticks and ms.  
   - **Pass**: Read OK and duration &lt; 2000 ticks.
4. **Test 2 – Disconnected**  
   - Disconnect sensor (or leave unplugged).  
   - Reset or run so the firmware does one Modbus read and prints duration.  
   - Record: result (OK/fail), duration in ticks and ms.  
   - **Pass**: Read fails and duration is on the order of 2000 ticks (or multiple timeouts).

### Notes

- If the stack issues several `serial_read()` calls per transaction, a “no response” case can exceed 2 s total (e.g. 2 s × number of read calls). The spec only requires that we do **not** add an extra 2 s delay after a successful response.
- For exact tick-to-ms conversion use the SDK rule: 1000 ticks = 1 s.

---

## First response after power-up: skip 00 00, then read again

### Observed behaviour

- After power-up and `Modbus_Init()`, the **first** Modbus read often returns **00 00** (0.0 °C) even when the sensor is connected and will later return a valid temperature. Longer settle delays (tested up to 2000 ms) do not reliably eliminate this.
- **Recommended**: Treat the first 00 00 response as "sensor not ready" and **read again** (retry). The second (or later) read then typically returns the real value. This is implemented in `Modbus_Request_Receive_Temperature()` in `modbussensor.c` (skip first 00 00, retry; accept 00 00 on retries as valid 0.0 °C).

### Minimal settle time and timeout

- **Settle time**: Delay from sensor power-on (and `Modbus_Init()`) until the first read. Use at least ~200 ms (`MODBUS_MIN_SETTLE_MS`); 400–800 ms is a safe default. Calibration over 400–2000 ms showed the first *single* read still often returns 00 00; the useful behaviour is "skip first 00 00 and read again", not "wait long enough that the first read is always valid".
- **Short timeouts**: To keep **duration &lt; 2000 ticks** when the sensor is connected, use:
  - **Inter-byte timeout** in `serial_read()`: after receiving a full frame (e.g. ≥ 9 bytes for Read Input Registers response), if no further byte arrives within a short window (e.g. 50–100 ms), return immediately instead of waiting for the full 2 s. This gives fast reads (~200–300 ms) once the sensor is responding.
  - **Minimum bytes before inter-byte**: Require at least 9 bytes (full response) before applying the short inter-byte timeout, so we do not return on 8-byte TX echo.
- **Finding minimal values**: The RX timeout test firmware runs a **settle calibration** (several power-on-to-first-read delays) and **timed reads** with short timeouts. Use it to confirm: (1) connected reads complete in &lt; 2000 ticks; (2) disconnected reads hit the long timeout (~2000 ticks). Tune inter-byte timeout and settle within that behaviour.
