/*
 * MIT License
 *
 * Copyright (c) 2020 Joey Castillo
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "hpl_systick_config.h"

#include "watch_extint.h"

// this warning only appears when you `make BOARD=OSO-SWAT-A1-02`. it's annoying,
// but i'd rather have it warn us at build-time than fail silently at run-time.
// besides, no one but me really has any of these boards anyway.
#if BTN_ALARM != GPIO(GPIO_PORTA, 2)
#warning This board revision does not support external wake on BTN_ALARM, so watch_register_extwake_callback will not work with it. Use watch_register_interrupt_callback instead.
#endif

static bool button_wake_enabled;
static volatile bool button_wake_pending;
static ext_irq_cb_t button_wake_callbacks[3];
static const uint8_t wake_button_pins[] = {BTN_MODE, BTN_LIGHT, BTN_ALARM};

static void _watch_mode_wake(void) {
    button_wake_pending = true;
    if (button_wake_callbacks[0]) button_wake_callbacks[0]();
}

static void _watch_light_wake(void) {
    button_wake_pending = true;
    if (button_wake_callbacks[1]) button_wake_callbacks[1]();
}

static void _watch_alarm_wake(void) {
    button_wake_pending = true;
    if (button_wake_callbacks[2]) button_wake_callbacks[2]();
}

void watch_register_button_wake_callbacks(ext_irq_cb_t mode, ext_irq_cb_t light, ext_irq_cb_t alarm) {
    button_wake_callbacks[0] = mode;
    button_wake_callbacks[1] = light;
    button_wake_callbacks[2] = alarm;
    button_wake_pending = false;
    watch_enable_external_interrupts();
    const ext_irq_cb_t handlers[] = {_watch_mode_wake, _watch_light_wake, _watch_alarm_wake};
    const uint8_t channels[] = {WATCH_BTN_MODE_EIC_CHANNEL, WATCH_BTN_LIGHT_EIC_CHANNEL, WATCH_BTN_ALARM_EIC_CHANNEL};
    uint32_t configs[2] = {0, 0};
    uint32_t mask = 0;
    for (uint8_t i = 0; i < 3; i++) {
        watch_register_interrupt_callback(wake_button_pins[i], handlers[i], INTERRUPT_TRIGGER_RISING);
        configs[channels[i] / 8] |= EIC_CONFIG_SENSE0(INTERRUPT_TRIGGER_RISING) << (4 * (channels[i] % 8));
        mask |= 1ul << channels[i];
    }

    // Asynchronous edge detection works in STANDBY without a sample clock.
    hri_eic_clear_CTRLA_ENABLE_bit(EIC);
    hri_eic_wait_for_sync(EIC, EIC_SYNCBUSY_ENABLE);
    // All unused sense/filter paths remain disabled.
    hri_eic_write_CONFIG_reg(EIC, 0, configs[0]);
    hri_eic_write_CONFIG_reg(EIC, 1, configs[1]);
    hri_eic_write_ASYNCH_reg(EIC, mask);
    hri_eic_write_EVCTRL_reg(EIC, 0);
    hri_eic_clear_INTEN_reg(EIC, 0xFFFF);
    hri_eic_clear_INTFLAG_reg(EIC, 0xFFFF);
    hri_eic_set_INTEN_reg(EIC, mask);
    hri_eic_set_CTRLA_ENABLE_bit(EIC);
    hri_eic_wait_for_sync(EIC, EIC_SYNCBUSY_ENABLE);
    button_wake_enabled = true;
    hri_gclk_write_PCHCTRL_reg(GCLK, EIC_GCLK_ID, 0);
}

void watch_disable_button_wake_callbacks(void) {
    if (!button_wake_enabled) return;
    // Restore the clock before resetting the enable-protected EIC registers.
    hri_gclk_write_PCHCTRL_reg(GCLK, EIC_GCLK_ID,
        GCLK_PCHCTRL_GEN_GCLK3_Val | (1 << GCLK_PCHCTRL_CHEN_Pos));
    watch_disable_external_interrupts();
    button_wake_enabled = false;
    for (uint8_t i = 0; i < 3; i++) button_wake_callbacks[i] = NULL;
    button_wake_pending = false;
}

void watch_register_extwake_callback(uint8_t pin, ext_irq_cb_t callback, bool level) {
    uint32_t pinmux;
    hri_rtc_tampctrl_reg_t config = RTC->MODE2.TAMPCTRL.reg;

    switch (pin) {
        case A4:
            a4_callback = callback;
            pinmux = PINMUX_PB00G_RTC_IN0;
            config &= ~(3 << RTC_TAMPCTRL_IN0ACT_Pos);
            config &= ~(1 << RTC_TAMPCTRL_TAMLVL0_Pos);
            config |= 1 << RTC_TAMPCTRL_IN0ACT_Pos;
            if (level) config |= 1 << RTC_TAMPCTRL_TAMLVL0_Pos;
            break;
        case A2:
            a2_callback = callback;
            pinmux = PINMUX_PB02G_RTC_IN1;
            config &= ~(3 << RTC_TAMPCTRL_IN1ACT_Pos);
            config &= ~(1 << RTC_TAMPCTRL_TAMLVL1_Pos);
            config |= 1 << RTC_TAMPCTRL_IN1ACT_Pos;
            if (level) config |= 1 << RTC_TAMPCTRL_TAMLVL1_Pos;
            break;
        case BTN_ALARM:
            gpio_set_pin_pull_mode(pin, GPIO_PULL_DOWN);
            btn_alarm_callback = callback;
            pinmux = PINMUX_PA02G_RTC_IN2;
            config &= ~(3 << RTC_TAMPCTRL_IN2ACT_Pos);
            config &= ~(1 << RTC_TAMPCTRL_TAMLVL2_Pos);
            config |= 1 << RTC_TAMPCTRL_IN2ACT_Pos;
            if (level) config |= 1 << RTC_TAMPCTRL_TAMLVL2_Pos;
            break;
        default:
            return;
    }
    gpio_set_pin_direction(pin, GPIO_DIRECTION_IN);
    gpio_set_pin_function(pin, pinmux);

    // disable the RTC
    RTC->MODE2.CTRLA.bit.ENABLE = 0;
    while (RTC->MODE2.SYNCBUSY.bit.ENABLE); // wait for RTC to be disabled

    // update the configuration
    RTC->MODE2.TAMPCTRL.reg = config;
    // re-enable the RTC
    RTC->MODE2.CTRLA.bit.ENABLE = 1;
    while (RTC->MODE2.SYNCBUSY.bit.ENABLE); // wait for RTC to be enabled

    NVIC_ClearPendingIRQ(RTC_IRQn);
    NVIC_EnableIRQ(RTC_IRQn);
    RTC->MODE2.INTENSET.reg = RTC_MODE2_INTENSET_TAMPER;
}

void watch_disable_extwake_interrupt(uint8_t pin) {
    hri_rtc_tampctrl_reg_t config = hri_rtc_get_TAMPCTRL_reg(RTC, 0xFFFFFFFF);

    switch (pin) {
        case A4:
            a4_callback = NULL;
            config &= ~(3 << RTC_TAMPCTRL_IN0ACT_Pos);
            break;
        case A2:
            a2_callback = NULL;
            config &= ~(3 << RTC_TAMPCTRL_IN1ACT_Pos);
            break;
        case BTN_ALARM:
            btn_alarm_callback = NULL;
            config &= ~(3 << RTC_TAMPCTRL_IN2ACT_Pos);
            break;
        default:
            return;
    }

	if (hri_rtcmode0_get_CTRLA_ENABLE_bit(RTC)) {
		hri_rtcmode0_clear_CTRLA_ENABLE_bit(RTC);
		hri_rtcmode0_wait_for_sync(RTC, RTC_MODE0_SYNCBUSY_ENABLE);
	}
    hri_rtc_write_TAMPCTRL_reg(RTC, config);
    hri_rtcmode0_set_CTRLA_ENABLE_bit(RTC);
}

void watch_store_backup_data(uint32_t data, uint8_t reg) {
    if (reg < 8) {
        RTC->MODE0.BKUP[reg].reg = data;
    }
}

uint32_t watch_get_backup_data(uint8_t reg) {
    if (reg < 8) {
        return RTC->MODE0.BKUP[reg].reg;
    }

    return 0;
}

static void _watch_disable_all_pins_except_rtc(void) {
    uint32_t config = RTC->MODE0.TAMPCTRL.reg;
    uint32_t porta_pins_to_disable = 0xFFFFFFFB; // Retain the RTC ALARM pin.
    uint32_t portb_pins_to_disable = 0xFFFFFFFF;
    if (button_wake_enabled) {
        for (uint8_t i = 0; i < 3; i++) {
            uint8_t pin = wake_button_pins[i];
            if (pin < 32) porta_pins_to_disable &= ~(1ul << pin);
            else portb_pins_to_disable &= ~(1ul << (pin % 32));
        }
    }

    // if there's an action set on RTC/IN[0], leave PB00 configured
    if (config & RTC_TAMPCTRL_IN0ACT_Msk) portb_pins_to_disable &= 0xFFFFFFFE;
    // same with RTC/IN[1] and PB02
    if (config & RTC_TAMPCTRL_IN1ACT_Msk) portb_pins_to_disable &= 0xFFFFFFFB;

    // port A: always keep PA02 configured as-is; that's our ALARM button.
    gpio_set_port_direction(0, porta_pins_to_disable, GPIO_DIRECTION_OFF);
    // port B: disable all pins we didn't save above.
    gpio_set_port_direction(1, portb_pins_to_disable, GPIO_DIRECTION_OFF);
}

static void _watch_disable_all_peripherals_except_slcd(void) {
    _watch_disable_tcc();
    watch_disable_adc();
    if (!button_wake_enabled) watch_disable_external_interrupts();
    watch_disable_i2c();
    // TODO: replace this with a proper function when we remove the debug UART
    SERCOM3->USART.CTRLA.reg &= ~SERCOM_USART_CTRLA_ENABLE;
    MCLK->APBCMASK.reg &= ~MCLK_APBCMASK_SERCOM3;
}

void watch_enter_sleep_mode(void) {
    // disable all other peripherals
    _watch_disable_all_peripherals_except_slcd();

    // disable tick interrupt
    watch_rtc_disable_all_periodic_callbacks();

    // disable brownout detector interrupt, which could inadvertently wake us up.
    SUPC->INTENCLR.bit.BOD33DET = 1;

    // per Microchip datasheet clarification DS80000782,
    // work around silicon erratum 1.8.4 by disabling the SysTick interrupt, which is
    // enabled as part of driver init, before going to sleep.
    SysTick->CTRL = SysTick->CTRL & ~(CONF_SYSTICK_TICKINT << SysTick_CTRL_TICKINT_Pos);

    // disable all pins
    _watch_disable_all_pins_except_rtc();

    // Don't lose a button interrupt delivered while preparing to enter STANDBY.
    if (button_wake_enabled) {
        uint32_t interrupt_mask = __get_PRIMASK();
        __disable_irq();
        if (!button_wake_pending) sleep(4);
        __set_PRIMASK(interrupt_mask);
        button_wake_pending = false;
    } else {
        sleep(4);
    }

    // and we awake! re-enable the brownout detector and SysTick interrupt
    SUPC->INTENSET.bit.BOD33DET = 1;
    SysTick->CTRL = SysTick->CTRL | (CONF_SYSTICK_TICKINT << SysTick_CTRL_TICKINT_Pos);

    // call app_setup so the app can re-enable everything we disabled.
    app_setup();

    // and call app_wake_from_standby (since main won't have a chance to do it)
    app_wake_from_standby();
}

void watch_enter_deep_sleep_mode(void) {
    // identical to sleep mode except we disable the LCD first.
    slcd_sync_deinit(&SEGMENT_LCD_0);
    hri_mclk_clear_APBCMASK_SLCD_bit(SLCD);

    watch_enter_sleep_mode();
}

void watch_enter_backup_mode(void) {
    watch_rtc_disable_all_periodic_callbacks();
    _watch_disable_all_peripherals_except_slcd();
    slcd_sync_deinit(&SEGMENT_LCD_0);
    hri_mclk_clear_APBCMASK_SLCD_bit(SLCD);
    _watch_disable_all_pins_except_rtc();

    // go into backup sleep mode (5). when we exit, the reset controller will take over.
    sleep(5);
}
