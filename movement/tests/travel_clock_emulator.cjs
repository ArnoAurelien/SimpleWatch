// Run with Playwright 1.48+ installed and the built emulator served on port 8000.
const assert = require('node:assert/strict');
const { chromium } = require('playwright');

(async () => {
    const browser = await chromium.launch({ headless: true });
    try {
        const page = await browser.newPage({ timezoneId: 'UTC' });
        const errors = [];
        page.on('pageerror', error => errors.push(error.message));
        await page.clock.install({ time: new Date('2026-07-01T12:34:56Z') });
        await page.clock.pauseAt(new Date('2026-07-01T12:34:56Z'));
        // Yield between fake timer callbacks without Chromium's real timeout clamp.
        // Preserve task boundaries; every emulated RTC callback still runs.
        await page.addInitScript(() => {
            // Emscripten stores interval IDs in a 32-bit C integer.
            // Playwright defaults to IDs above 1e12, which prevents clearing them.
            if (globalThis.__pwClock?.controller) globalThis.__pwClock.controller._uniqueTimerId = 1000;
            const embedder = globalThis.__pwClock?.controller?._embedder;
            if (!embedder) return;
            const original = embedder.setTimeout;
            const queue = [];
            const channel = new MessageChannel();
            channel.port1.onmessage = () => { const item = queue.shift(); if (!item.cancelled) item.task(); };
            embedder.setTimeout = (task, delay) => {
                if (delay !== undefined) return original(task, delay);
                const item = { task, cancelled: false };
                queue.push(item); channel.port2.postMessage(0);
                return () => { item.cancelled = true; };
            };
        });
        await page.goto(process.env.WATCH_URL || 'http://127.0.0.1:8000/watch.html');
        await page.waitForTimeout(500);
        await page.clock.runFor(100);
        await page.waitForFunction(() => document.querySelector('#hour_2_B')?.style.opacity === '1');
        const advance = ms => page.clock.runFor(ms);
        async function press(key, ms = 100) {
            await page.keyboard.down(key); await advance(ms);
            await page.keyboard.up(key); await advance(1100);
        }
        async function screen() {
            return page.evaluate(() => {
                const pattern = prefix => [...document.querySelectorAll(`[id^="${prefix}_"]`)]
                    .filter(e => e.style.opacity === '1').map(e => e.id.slice(-1)).sort().join('');
                const digits = {ABCDEF:'0',BC:'1',ABDEG:'2',ABCDG:'3',BCFG:'4',ACDFG:'5',ACDEFG:'6',ABC:'7',ABCDEFG:'8',ABCDFG:'9','':' '};
                const number = prefix => ['2','1'].map(n => digits[pattern(`${prefix}_${n}`)] ?? '?').join('');
                return {header:pattern('mode_2')+'/'+pattern('mode_1'),day:number('day'),
                    main:number('hour')+number('minute')+number('second'),
                    colon:document.querySelector('#dot_up').style.opacity === '1',
                    light:Number(document.querySelector('#light').style.opacity)};
            });
        }
        const france = await screen();
        assert.equal(france.main, '143456'); assert.equal(france.day, '  ');
        assert.equal(france.header, 'AEFG/ABCEFGH'); assert(france.colon);
        await press('m'); const finland = await screen();
        assert.equal(finland.main.slice(0,4), '1534'); assert.equal(finland.day, '  ');
        assert.equal(finland.header, 'AEFG/EF');
        await press('m'); const date = await screen();
        assert.equal(date.main, '072026'); assert.equal(date.day, ' 1'); assert(!date.colon);
        await press('m'); assert.equal((await screen()).header, france.header);
        await press('a', 4000); assert.equal((await screen()).header, france.header);
        await page.keyboard.down('l'); await advance(100);
        assert((await screen()).light > 0); assert.equal((await screen()).header, france.header);
        await page.keyboard.up('l'); await advance(1500); assert.equal((await screen()).light, 0);
        console.log('PASS: emulator display, view rotation, ALARM and LED');
        async function unlock() { await press('m', 3200); await press('l', 3200); await press('a', 3200); }
        // Wrong order must reset; the trailing LIGHT/ALARM cannot open settings.
        await press('m', 3200); await press('a', 3200); await press('l', 3200); await press('a', 3200);
        assert.equal((await screen()).header, france.header);
        // A short LIGHT aborts an otherwise valid attempt.
        await press('m', 3200); await press('l', 2000); await press('a', 3200);
        assert.equal((await screen()).header, france.header);
        // Exceed the total window between the first and second holds.
        await press('m', 3200); await advance(12000); await press('l', 3200); await press('a', 3200);
        assert.equal((await screen()).header, france.header);
        // An overlapping LIGHT while MODE is held invalidates the attempt.
        await page.keyboard.down('m'); await advance(3200);
        await page.keyboard.down('l'); await advance(3200);
        await page.keyboard.up('m'); await page.keyboard.up('l'); await advance(30);
        await press('a', 3200); assert.equal((await screen()).header, france.header);
        console.log('PASS: emulator invalid unlock sequences');
        await unlock(); const editor = await screen();
        assert.notEqual(editor.header, france.header); assert.notEqual(editor.header, finland.header);
        // Editor ALARM increments the hour; LIGHT selects minutes; MODE exits.
        async function visibleHour() {
            for (let i = 0; i < 10; i++) {
                const value = (await screen()).main.slice(0,2);
                if (/^\d{2}$/.test(value)) return Number(value);
                await advance(125);
            }
            throw new Error('Editor hour remained blank');
        }
        const hour = await visibleHour();
        await press('a'); await advance(300);
        assert.equal(await visibleHour(), (hour + 1) % 24);
        const hourHeader = (await screen()).header;
        await press('l'); assert.notEqual((await screen()).header, hourHeader);
        await press('m'); assert.equal((await screen()).header, france.header);
        console.log('PASS: emulator unlock and editor controls');
        // The unlock must also work from Finland and date views.
        await press('m'); await unlock(); assert.equal((await screen()).header, editor.header);
        await press('m'); await press('m'); await press('m');
        assert.equal((await screen()).header, date.header);
        await unlock(); assert.equal((await screen()).header, editor.header);
        await press('m'); // Exit the editor back to France.
        await advance(28000);
        assert.match((await screen()).main.slice(4), /^\d{2}$/);
        await advance(2000); // Cross the real thirty-second inactivity boundary.
        assert.equal((await screen()).main.slice(4), '  ');
        const sleepingMinute = (await screen()).main.slice(0,4);
        await advance(60000);
        assert.notEqual((await screen()).main.slice(0,4), sleepingMinute);
        await press('a');
        assert.equal((await screen()).header, france.header);
        assert.match((await screen()).main.slice(4), /^\d{2}$/);
        assert.equal((await screen()).light, 0);

        const headers = [france.header, finland.header, date.header];
        async function selectView(index) {
            for (let attempts = 0; attempts < 3; attempts++) {
                if ((await screen()).header === headers[index]) return;
                await press('m');
            }
            assert.equal((await screen()).header, headers[index]);
        }
        // Every wake button works on every view and performs its action once.
        for (let view = 0; view < 3; view++) {
            for (const key of ['a', 'l', 'm']) {
                await selectView(view);
                await advance(30000);
                if (view !== 2) assert.equal((await screen()).main.slice(4), '  ');
                await page.keyboard.down(key); await advance(150);
                if (key === 'l') assert((await screen()).light > 0);
                await page.keyboard.up(key); await advance(150);
                assert.equal((await screen()).header, headers[key === 'm' ? (view + 1) % 3 : view]);
                if ((await screen()).header !== date.header)
                    assert.match((await screen()).main.slice(4), /^\d{2}$/);
                if (key === 'a') assert.equal((await screen()).light, 0);
                await advance(1500); assert.equal((await screen()).light, 0);
            }
        }
        // Press and release in one JS task, before asynchronous C wake restores
        // normal callbacks: replay the stored press even though the pin is low.
        for (const key of ['m', 'l', 'a']) {
            await selectView(0); await advance(30000);
            await page.evaluate(key => {
                document.dispatchEvent(new KeyboardEvent('keydown', {key, bubbles:true}));
                document.dispatchEvent(new KeyboardEvent('keyup', {key, bubbles:true}));
            }, key);
            await advance(150);
            assert.equal((await screen()).header, headers[key === 'm' ? 1 : 0]);
            assert.match((await screen()).main.slice(4), /^\d{2}$/);
            assert.equal((await screen()).light > 0, key === 'l');
            await advance(1500);
        }
        // Holding LIGHT from sleep sustains illumination and keeps the view.
        await selectView(0); await advance(30000);
        await page.keyboard.down('l'); await advance(3500);
        assert((await screen()).light > 0);
        assert.equal((await screen()).header, france.header);
        await page.keyboard.up('l'); await advance(1500);
        assert.equal((await screen()).light, 0);
        // ALARM held from sleep still only wakes and does not light or change views.
        await advance(30000); await press('a', 3500);
        assert.equal((await screen()).header, france.header);
        assert.equal((await screen()).light, 0);
        // A waking MODE hold is the first hold of the existing unlock sequence.
        await advance(30000); await unlock();
        assert.equal((await screen()).header, editor.header);
        // Chords during wake still reset the unlock attempt.
        await press('m'); await advance(30000);
        await page.evaluate(() => {
            for (const key of ['m','l'])
                document.dispatchEvent(new KeyboardEvent('keydown', {key, bubbles:true}));
        });
        await advance(3500);
        await page.evaluate(() => {
            for (const key of ['m','l'])
                document.dispatchEvent(new KeyboardEvent('keyup', {key, bubbles:true}));
        });
        await advance(100); await press('a', 3200);
        assert.equal((await screen()).header, france.header);
        await unlock(); assert.equal((await screen()).header, editor.header);
        console.log('PASS: thirty-second sleep, minute updates, all wake buttons on all views, immediate actions, quick taps, held LIGHT, unlock from sleep and wake chords');
        assert.deepEqual(errors, []);
        console.log('PASS: browser emulator views, LED, ALARM, unlock/reset cases, existing editor, entry from every view');
    } finally { await browser.close(); }
})().catch(error => { console.error(error); process.exitCode = 1; });
