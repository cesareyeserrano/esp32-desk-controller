# Tools

Support scripts. They are not part of the firmware.

## `serial_talk.py`

Sends a command to `desk_sniffer` and records the bus response.

```
tools/serial_talk.py -c 2 -o docs/capturas/2026-08-21-canal2.log
```

**It exists because shell redirection does not work for writing.** macOS resets
the port configuration on every `open()`, and the baud rate has to be set with
the native `IOSSIOSPEED` ioctl on the **same** descriptor used to read and
write. With `printf '2' > /dev/cu.usbserial-0001` the bytes never reach the
sketch, as confirmed on 2026-08-21 with three different tools.

To **only listen**, the `stty` recipe in
[../docs/capturas/README.md](../docs/capturas/README.md) still works.

Default speed is **115200**, not 921600 or 460800. See
[ADR-026](../docs/DECISIONS.md), which corrects
[ADR-025](../docs/DECISIONS.md).

⚠️ **Send `h` and check that the help text comes back before trusting any
channel test.** The port stopped accepting commands for no identified reason on
2026-08-21, and that made a channel look dead when it had never actually been
tested.

**Close the Arduino IDE first.** Its Serial Monitor grabs the port and only one
process can hold it. Check with `lsof /dev/cu.usbserial-0001`.
