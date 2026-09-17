## STM32 USART2: Polling vs. Interrupt-Driven CPU Utilization

Firmware:
Examples of polling and interrupt-driven firmware implementing the USART2 Peripheral on the STM32C031C6 (Cortex-M0+) microcontroller. A bare-metal programming approach is used, setting and monitoring control and status registers directly via memory and bit mapping without use of a hardware abstraction layer.       

Testing:
This firmware is tested with a Python script that confirms basic functionality and measures CPU utilization, using instrumented variants of the firmware.


## HIL Test Results:

Polling:
```
(venv) r-marcoux@xps13:~/Programming/Python_Testing/USART2$ pytest -v -s test_usart2_CPU_utilization_v1.py
=================================================== test session starts ====================================================
platform linux -- Python 3.12.3, pytest-9.1.1, pluggy-1.6.0 -- /home/r-marcoux/Programming/Python_Testing/venv/bin/python3
cachedir: .pytest_cache
rootdir: /home/r-marcoux/Programming/Python_Testing/USART2
collected 4 items                                                                                                            

test_usart2_CPU_utilization_v1.py::test_echo_single_byte PASSED
test_usart2_CPU_utilization_v1.py::test_echo_multi_byte PASSED
test_usart2_CPU_utilization_v1.py::test_timer_accuracy Expected: 2000ms, Elapsed: 2019ms, Accuracy: 99.05%
PASSED
test_usart2_CPU_utilization_v1.py::test_percent_utilization Elapsed: 42ms, Busy: 42.0000ms, Utilization: 100.000000%
PASSED

==================================================== 4 passed in 2.11s =====================================================
```

Interrupt-Driven:
```
(venv) r-marcoux@xps13:~/Programming/Python_Testing/USART2$ pytest -v -s test_usart2_CPU_utilization_v1.py
==================================================== test session starts ===================================================
platform linux -- Python 3.12.3, pytest-9.1.1, pluggy-1.6.0 -- /home/r-marcoux/Programming/Python_Testing/venv/bin/python3
cachedir: .pytest_cache
rootdir: /home/r-marcoux/Programming/Python_Testing/USART2
collected 4 items                                                                                                            

test_usart2_CPU_utilization_v1.py::test_echo_single_byte PASSED
test_usart2_CPU_utilization_v1.py::test_echo_multi_byte PASSED
test_usart2_CPU_utilization_v1.py::test_timer_accuracy Expected: 2000ms, Elapsed: 2014ms, Accuracy: 99.30%
PASSED
test_usart2_CPU_utilization_v1.py::test_percent_utilization Elapsed: 35ms, Busy: 0.2996ms, Utilization: 0.855952%
PASSED

==================================================== 4 passed in 2.11s =====================================================
```

## Project Intent

This project's primary intent is to demonstrate bare-metal firmware implementation and basic HIL testing of firmware using Python’s pytest testing framework.

To demonstrate testing, measuring the impact on CPU utilization between polling and interrupt-driven methods seemed like a natural extension that essentially reaffirms, with direct measurement, the well-known performance impact and rationale for the interrupt-driven paradigm.


## File Structure

    Firmware/
        USART2_VCP_Polling_v1/              			— baseline polling implementation*
        USART2_VCP_Polling_Instrumented_v1/ 			— adds SysTick core exception timer*
        USART2_VCP_Interrupt_v1/            			— interrupt-driven RX*
        USART2_VCP_Interrupt_Instrumented_v1/ 		    — adds SysTick core exception timer
                                                 		  and interrupt specific timing* 
                                              
    Python_Tests/
        test_usart2_CPU_utilization_v1.py   			— HIL test suite using pytest*
        requirements.txt                     		    — Python dependencies

Each firmware variant is a single `main.c` file, originally developed within STM32CubeIDE. These files can be copied, built and loaded into the Nucleo-C031C6 microcontroller using this environment.

*see more detailed descriptions in each program file.


## Instrumentation, Testing and Measurement Methodology

Both firmware versions utilize a simple query protocol triggered by a specific QUERY_BYTE ascii '?' character. When detected, the normal echo response is replaced by data that provides timing information tracked within the running firmware; namely “tick_count” for milliseconds of elapsed run 
time and “busy_ticks” for raw clock cycles.
For polling, busy_ticks is a derived value equal to elapsed time scaled to clock cycles, because the polling loop never idles. For interrupt-driven firmware, busy_ticks is measured directly from clock cycles consumed during execution of the interrupt specifically, and can thus be distinguished from total elapsed time.

Polling:
Due to the nature of polling, elapsed run time and raw clock cycles show the same measurement of overall program execution time, and thus a CPU utilization 100%.  

Interrupt-Driven:
For this case, the raw clock cycles consumed within the interrupt handler can be tracked by reading the SYST_CVR countdown register at the beginning and end of the handler, and calculating a delta that reflects a significant drop in CPU utilization relative to overall run time.


## Known Limitations

- Elapsed time (tick_count) is measured in millisecond increments, rather than derived from clock cycles read from the SYST_CVR countdown register, because it provides a sufficient level of granularity for its purpose and reduces the likelihood of variable overflow during prolonged run times.

- Interrupt time (busy_ticks) is derived from clock cycles read from the SYST_CVR countdown register, and as a 32-bit value may overflow after accumulating over extended run time. The risk of overflow in this context is not considered a practical concern given the window of time being measured.

- If a SYST_CVR rollover were to occur mid-interrupt, the resulting sample is detected (cvr_entry < cvr_exit) and discarded rather than approximated, since correctly reconstructing elapsed time across a rollover would require tracking the reload value explicitly. This case is not expected to occur in practice, given how briefly the interrupt handler executes relative to a full SysTick period.   


## Running the Tests

Ensure the target firmware is flashed and running standalone (not attached to an active debug session, which will hold the serial port).
```
    cd Python_Tests
    python3 -m venv venv
    source venv/bin/activate
    pip install -r requirements.txt

Instrumented Firmware Tests:
    pytest -v -s test_usart2_CPU_utilization_v1.py

Non-Instrumented Firmware Tests (USART2_VCP_Polling_v1 and USART2_VCP_Interrupt_v1):
    pytest -v -s test_usart2_CPU_utilization_v1.py -k "echo"
```

The '-k "echo"' option is needed for non-instrumented firmware because they were not meant to implement the timing query protocol.
