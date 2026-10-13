# Travel clock checks

The default firmware has three everyday views: France (`FR`), Finland (`FI`), and the France-local date. Time is displayed in 24-hour format; the date uses weekday, day without a leading zero, two-digit month, and four-digit year. France and Finland automatically follow the current European summer-time rule: last Sunday of March through last Sunday of October, with both transitions at 01:00 UTC ([Council reference](https://data.consilium.europa.eu/doc/document/ST-14195-2025-INIT/en/pdf)).

MODE cycles the three views. ALARM has no everyday action. LIGHT illuminates the LED, including during the unlock sequence. The normal light duration and colors still apply. The original deep-sleep timeout is enabled, and faces preserve the configured sleep interval. In low-energy mode, time views update once a minute with seconds blank; the date remains visible. ALARM wakes the watch using Movement's existing wake behavior.

To open the unchanged time/date editor, hold MODE for at least three seconds and release, then hold LIGHT for at least three seconds and release, then hold ALARM for at least three seconds and release. Complete the sequence within fifteen seconds of the initial MODE press. An incorrect button, too-short hold, overlapping press, or timeout resets the attempt. Release all buttons before retrying. A MODE press shorter than three seconds cycles views when no sequence is already underway. Releasing the final ALARM before entering the editor also prevents its hold from accidentally changing the time.

In the editor, LIGHT selects the field, ALARM adjusts it (hold to repeat), and MODE exits to France. The `ZO` field still describes the fixed UTC offset of the time entered in the RTC. Hardware defaults to `ZO = 0` (UTC), so enter UTC, or select the matching offset if entering local time: `1` for France winter time, `2` for France summer / Finland winter time, or `3` for Finland summer time. The RTC continues using that fixed offset; the displayed country times change automatically at summer-time boundaries. The emulator initially derives `ZO` from the browser's current offset.

Build the simulator (select the color matching your physical board when building firmware):

```sh
emmake make -C movement/make COLOR=GREEN -j4
python3 -m http.server 8000 --directory movement/make/build-sim
```

Open http://localhost:8000/watch.html. Keyboard shortcuts: `m` for MODE, `l` for LIGHT, `a` for ALARM.

Run deterministic tests for exact hold/window boundaries, wrong order, overlaps, reset/retry, coalesced foreground ticks, all view layouts, preserved sleep settings, sleeping display updates, leap day, year rollover, summer-time transitions and non-UTC RTC settings:

```sh
emmake make -C movement/tests COLOR=GREEN test
```

The browser suite uses real emulator keyboard events and fake time to check view rotation, LED behavior, ALARM, unlock failures, unlock success from all views, and the editor's existing controls. With Playwright 1.48+ and Chromium installed, and the server running:

```sh
node movement/tests/travel_clock_emulator.cjs
```

Set `WATCH_URL` to use a different server address. `NODE_PATH` can point to a temporary Playwright installation. The deterministic suite stubs hardware operations; browser tests run the complete simulator. Physical button feel and LED brightness still require a watch.
