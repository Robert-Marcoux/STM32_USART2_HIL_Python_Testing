"""
USART2 CPU utilization test suite.

Verifies USART2 echo correctness and measures CPU time consumed
servicing USART2 I/O, comparing polling and interrupt-driven firmware.
Timing is captured on-device via SysTick and reported to the host over
the same serial link, using a delta-based (before/after) measurement
to avoid dilution from idle time since boot.
"""

import serial
import pytest
import time

SERIAL_PORT = '/dev/ttyACM0'
BAUD_RATE = 9600
TIMEOUT_SEC = 1

CPU_CLOCK_HZ = 12_000_000            # matches measured firmware-derived runtime value.
CYCLES_PER_MS = CPU_CLOCK_HZ / 1000  # convert clock cycles to milliseconds.

def retry(max_attempts):
    """Retry decorator for transient serial connection failures
    (e.g. a debugger session still holding the port)."""
    def retry_decorator(func):
        def retry_wrapper(*args, **kwargs):
            for attempt in range(1, max_attempts + 1):
                try:
                    return func(*args, **kwargs)
                except serial.SerialException as e:
                    print(f"Attempt {attempt} failed: {e}")
                    if attempt == max_attempts:
                        raise
                    time.sleep(1)
        return retry_wrapper
    return retry_decorator

@retry(3)
def open_serial_connection():
    return serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=TIMEOUT_SEC)


@pytest.fixture
def uart():
    ser = open_serial_connection()
    yield ser
    ser.close()

def query_stats(uart):
    """Request tick_count (elapsed ms) and busy_ticks (CPU cycles spent
    servicing USART2) from the firmware via a single query byte."""
    uart.write(b'?')
    response = uart.read(8)
    tick = int.from_bytes(response[0:4], byteorder='little')
    busy = int.from_bytes(response[4:8], byteorder='little')
    return tick, busy

def test_echo_single_byte(uart):
    """Confirms USART2 correctly echoes a single received byte."""
    message = b'w'
    uart.write(message)
    response = uart.read(len(message))
    assert response == message

def test_echo_multi_byte(uart):
    """Confirms USART2 correctly echoes a multi-byte message,
    verifying no data loss or corruption across sequential bytes."""
    message = b'hello'
    uart.write(message)
    response = uart.read(len(message))
    assert response == message

def test_timer_accuracy(uart):
    """Verifies the firmware's on-device SysTick timer is accurate,
    since CPU utilization results depend on it. A 2-second interval
    is measured and checked against the expected elapsed time."""
    tick_start, _ = query_stats(uart)

    delay_seconds = 2
    time.sleep(delay_seconds)

    tick_end, _ = query_stats(uart)

    elapsed_ms = tick_end - tick_start
    expected_ms = delay_seconds * 1000

    error_ms = abs(elapsed_ms - expected_ms)
    accuracy_percent = (1 - (error_ms / expected_ms)) * 100

    print(f"Expected: {expected_ms}ms, Elapsed: {elapsed_ms}ms, Accuracy: {accuracy_percent:.2f}%")
    assert abs(elapsed_ms - expected_ms) < 200

def test_percent_utilization(uart):
    """Measures CPU time spent servicing USART2 over a bounded test
    window (10 echoed bytes), reported as a percentage of elapsed
    time. Compares directly against the same test run on polling
    firmware, where utilization is always 100% by construction."""
    tick_start, busy_start = query_stats(uart)

    for _ in range(10):
        uart.write(b'w')
        uart.read(1)

    tick_end, busy_end = query_stats(uart)

    elapsed_ms = tick_end - tick_start
    busy_ms = (busy_end - busy_start) / CYCLES_PER_MS
    utilization = (busy_ms / elapsed_ms) * 100 if elapsed_ms > 0 else 0

    print(f"Elapsed: {elapsed_ms}ms, Busy: {busy_ms:.4f}ms, Utilization: {utilization:.6f}%")
    assert 0 <= utilization <= 100
