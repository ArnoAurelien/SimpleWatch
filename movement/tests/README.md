# Travel clock checks

The default firmware has three everyday views: France (`FR`), Finland (`FI`), and the France-local date. Time is displayed in 24-hour format; the date uses weekday, day without a leading zero, two-digit month, and four-digit year. France and Finland automatically follow the current European summer-time rule: last Sunday of March through last Sunday of October, with both transitions at 01:00 UTC ([Council reference](https://data.consilium.europa.eu/doc/document/ST-14195-2025-INIT/en/pdf)).

Each view has its own implementation: `france_clock_face.c`, `finland_clock_face.c`, and `full_date_face.c`. They share time conversion and button/unlock helpers in `travel_clock.c`. Display identity is independent of carousel position.

MODE cycles the three views. ALARM has no everyday action. LIGHT illuminates the LED, including during the unlock sequence. The normal light duration and colors still apply. The original ten-minute low-energy timeout is enabled, and faces preserve the configured sleep interval. In low-energy mode, time views update once a minute with seconds blank; the date remains visible. Any button wakes the watch. The same MODE press cycles the view on release, LIGHT illuminates the display on press, and ALARM only wakes without changing the view or lighting the LED. A waking MODE hold can start the existing settings unlock sequence. The wake button is recorded before normal callbacks are restored, so even a quick tap released during wake-up is applied once.

Blanking numeric seconds is inherited from the original `simple_clock_face.c`: low-energy time updates occur once a minute. The original also uses a ticking LCD animation; these country faces leave the seconds area blank.

MODE and LIGHT are not RTC wake pins on Lite, so all three buttons use the SAM L22's asynchronous EIC edge detectors. Only these three button inputs remain enabled; unused sense/filter paths and the EIC generic clock are disabled, along with periodic RTC ticks and the other normal sleep peripherals. The CPU still enters STANDBY with the LCD and minute RTC alarm active. See the [Microchip SAM L22 datasheet](https://ww1.microchip.com/downloads/aemDocuments/documents/MCU32/ProductDocuments/DataSheets/SAM-L22-Family-Data-Sheet-DS60001465.pdf), section 26.6.4.2. This changes the original RTC-only wake circuitry: physical wake reliability for each button and sleep current must be measured on the watch to validate the two-year battery goal.

To open the unchanged time/date editor, hold MODE for at least three seconds and release, then hold LIGHT for at least three seconds and release, then hold ALARM for at least three seconds and release. Complete the sequence within fifteen seconds of the initial MODE press. An incorrect button, too-short hold, overlapping press, or timeout resets the attempt. Release all buttons before retrying. A MODE press shorter than three seconds cycles views when no sequence is already underway. Releasing the final ALARM before entering the editor also prevents its hold from accidentally changing the time.

In the editor, LIGHT selects the field, ALARM adjusts it (hold to repeat), and MODE exits to France. The `ZO` field still describes the fixed UTC offset of the time entered in the RTC. Hardware defaults to `ZO = 0` (UTC), so enter UTC, or select the matching offset if entering local time: `1` for France winter time, `2` for France summer / Finland winter time, or `3` for Finland summer time. The RTC continues using that fixed offset; the displayed country times change automatically at summer-time boundaries. The emulator initially derives `ZO` from the browser's current offset.

The project defaults to Sensor Watch Lite (`COLOR=RED`) and the standard LCD. RED selects inverted LED polarity and disables unsupported RTC frequency correction; no alternate LCD mapping is enabled.

Build the firmware:

```sh
make -C movement/make COLOR=RED -j4
```

The resulting flashable file is `movement/make/build/watch.uf2`.

Build the simulator:

```sh
emmake make -C movement/make COLOR=RED -j4
python3 -m http.server 8000 --directory movement/make/build-sim
```

When switching from a previous GREEN build, add `-B` to the build command once to rebuild all objects with the RED flags; changing `COLOR` alone does not invalidate the existing object files.

Open http://localhost:8000/watch.html. Keyboard shortcuts: `m` for MODE, `l` for LIGHT, `a` for ALARM.

Run deterministic tests for exact hold/window boundaries, wrong order, overlaps, reset/retry, coalesced foreground ticks, all view layouts, face identity after reordering, preserved sleep settings, sleeping display updates, leap day, year rollover, summer-time transitions and non-UTC RTC settings:

```sh
emmake make -C movement/tests COLOR=RED test
```

The browser suite uses real emulator keyboard events and fake time to check view rotation, LED behavior, ALARM, unlock failures, unlock success from all views, and the editor's existing controls. It also crosses the full ten-minute timeout, verifies minute updates and each wake button on every view, checks same-press navigation/LED actions and fast taps released before wake-up completes, and tests held LIGHT, unlock directly from sleep, and overlapping wake presses. With Playwright 1.48+ and Chromium installed, and the server running:

```sh
node movement/tests/travel_clock_emulator.cjs
```

Set `WATCH_URL` to use a different server address. `NODE_PATH` can point to a temporary Playwright installation. The deterministic suite stubs hardware operations; browser tests run the complete simulator. Physical button feel and LED brightness still require a watch.
