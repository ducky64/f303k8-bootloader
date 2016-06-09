# f303k8-bootloader
Software I2C bootloader for the ST Nucleo F303K8

## IDE Configuration

### Debugging Application Code

Application code is loaded at a different place in memory, so the debugger must start the processor there. Using Eclipse with OpenOCD, in Debug Configuration -> Startup -> Run/Restart Commands, add `j Reset_Handler` to have it start at the application reset point.
