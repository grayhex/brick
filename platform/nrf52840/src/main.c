/*
 * Brick V1 — nRF52840 platform bring-up application.
 *
 * What this is: the smallest application that proves the update chain on the
 * nRF52840 DK: reports its version and image state, runs a self-test and only
 * then confirms the MCUboot image, and carries MCUmgr/SMP over USB CDC ACM.
 *
 * What this is not: the Brick console. libbrickcore (firmware/) is NOT linked
 * here; bk_hal_t is connected in a separate change.
 */
#include <zephyr/kernel.h>
#include <zephyr/app_version.h>
#include <zephyr/device.h>
#include <zephyr/dfu/mcuboot.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/watchdog.h>
#include <zephyr/logging/log.h>
#include <zephyr/storage/flash_map.h>

LOG_MODULE_REGISTER(brick, LOG_LEVEL_INF);

#define LED0_NODE DT_ALIAS(led0)
#define WDT_NODE  DT_ALIAS(watchdog0)

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
static const struct device *const wdt = DEVICE_DT_GET(WDT_NODE);
static int wdt_channel = -1;

static const char *swap_name(int type)
{
	switch (type) {
	case BOOT_SWAP_TYPE_NONE:
		return "none";
	case BOOT_SWAP_TYPE_TEST:
		return "test";
	case BOOT_SWAP_TYPE_PERM:
		return "permanent";
	case BOOT_SWAP_TYPE_REVERT:
		return "revert";
	case BOOT_SWAP_TYPE_FAIL:
		return "fail";
	default:
		return "unknown";
	}
}

/*
 * The nRF52840 watchdog cannot be stopped once started and keeps running across a soft reset,
 * so main() feeds it for ever and MCUboot feeds it too while it swaps, hashes or waits in serial
 * recovery (CONFIG_BOOT_WATCHDOG_FEED, checked by scripts/verify_build.py).
 */
static int watchdog_start(void)
{
	const struct wdt_timeout_cfg cfg = {
		.flags = WDT_FLAG_RESET_SOC,
		.window = {.min = 0, .max = CONFIG_BRICK_SELFTEST_WDT_TIMEOUT_MS},
	};

	if (!device_is_ready(wdt)) {
		return -ENODEV;
	}
	wdt_channel = wdt_install_timeout(wdt, &cfg);
	if (wdt_channel < 0) {
		return wdt_channel;
	}
	return wdt_setup(wdt, WDT_OPT_PAUSE_HALTED_BY_DBG);
}

static int check_area(const char *name, uint8_t id, size_t *size)
{
	const struct flash_area *fa;
	uint8_t probe[16];
	int rc = flash_area_open(id, &fa);

	if (rc != 0) {
		LOG_ERR("self-test: %s: open failed (%d)", name, rc);
		return rc;
	}
	rc = flash_area_read(fa, 0, probe, sizeof probe);
	*size = fa->fa_size;
	LOG_INF("flash area %-8s id=%u dev=%s off=0x%06lx size=%u KiB", name, id, fa->fa_dev->name,
		(unsigned long)fa->fa_off, (unsigned)(fa->fa_size / 1024U));
	flash_area_close(fa);
	if (rc != 0) {
		LOG_ERR("self-test: %s: read failed (%d)", name, rc);
	}
	return rc;
}

/*
 * Minimal self-test before the image may be confirmed. Real products add: storage
 * integrity (BKSV A/B), hardware revision check, display/input init.
 * Confirming immediately at start would defeat revert, see FIRMWARE_UPDATE_NRF52840.md.
 */
static int self_test(void)
{
	size_t s0 = 0, s1 = 0, st = 0;

	if (check_area("slot0", PARTITION_ID(slot0_partition), &s0) != 0 ||
	    check_area("slot1", PARTITION_ID(slot1_partition), &s1) != 0 ||
	    check_area("storage", PARTITION_ID(storage_partition), &st) != 0) {
		return -EIO;
	}
	if (s1 < s0) {
		LOG_ERR("self-test: slot1 (%u) smaller than slot0 (%u)", (unsigned)s1, (unsigned)s0);
		return -EINVAL;
	}
	return 0;
}

static void blink(int period_ms)
{
	static int64_t last;
	int64_t now = k_uptime_get();

	if (now - last >= period_ms / 2) {
		last = now;
		gpio_pin_toggle_dt(&led);
	}
}

int main(void)
{
	bool confirmed = boot_is_img_confirmed();
	int rc;

	LOG_INF("Brick V1 platform %s, signed version %s, built %s %s", APP_VERSION_EXTENDED_STRING,
		CONFIG_MCUBOOT_IMGTOOL_SIGN_VERSION, __DATE__, __TIME__);
	LOG_INF("image: confirmed=%s, last swap=%s", confirmed ? "yes" : "NO (test)",
		swap_name(mcuboot_swap_type()));

	if (gpio_is_ready_dt(&led)) {
		gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
	}
	rc = watchdog_start();
	if (rc != 0) {
		LOG_ERR("watchdog start failed (%d)", rc);
	}

#if defined(CONFIG_BRICK_TEST_CRASH_BEFORE_CONFIRM)
	if (!confirmed) {
		LOG_WRN("TEST: faulting before confirmation");
		k_panic();
	}
#elif defined(CONFIG_BRICK_TEST_HANG_BEFORE_CONFIRM)
	if (!confirmed) {
		LOG_WRN("TEST: hanging before confirmation, watchdog must reset");
		for (;;) {
			k_busy_wait(1000);
		}
	}
#endif

	if (!confirmed) {
		rc = self_test();
		if (rc != 0) {
			LOG_ERR("self-test FAILED (%d): image stays unconfirmed, MCUboot will revert", rc);
		} else if (IS_ENABLED(CONFIG_BRICK_TEST_NEVER_CONFIRM)) {
			LOG_WRN("TEST: self-test passed, confirmation withheld on purpose");
		} else {
			rc = boot_write_img_confirmed();
			LOG_INF("self-test passed, confirm image: %s (%d)", rc == 0 ? "OK" : "FAILED", rc);
		}
		confirmed = boot_is_img_confirmed();
	}

	LOG_INF("ready: image %s; MCUmgr/SMP on the nRF USB port (CDC ACM)",
		confirmed ? "confirmed" : "NOT confirmed");

	for (;;) {
		if (wdt_channel >= 0) {
			wdt_feed(wdt, wdt_channel);
		}
		blink(confirmed ? 1000 : 200);
		k_sleep(K_MSEC(50));
	}
	return 0;
}
