# Retired tools, 2026-08-23

These scripts implemented travel control **in Python, over the serial port**,
back when the firmware could not yet travel on its own. The firmware overtook
them: since phase 4, travel lives inside the ESP32 (`startTravel` and
`superviseTravel`, with limits, verified braking and retargeting) and is
commanded over MQTT (`ir:N`, `continuo_subir`, `parar`).

**Do not run them against the current firmware.** The adversarial review of
2026-08-23 (round 2) found that they now **compete with the firmware's state
machine**: each one brakes on its own account, the firmware reads those taps as
movement, and both end up issuing crossed brakes. On top of that their brake was
800 ms, which moves the desk about 1 cm, and they could manufacture a false
"limit reached".

They are kept as historical reference. Their closed-loop logic is what was
validated on 2026-08-22 and later ported into the firmware.

To move the desk today:

    mosquitto_pub -t escritorio_jiecang/altura_objetivo/set -m 95

or the Home Assistant buttons.
