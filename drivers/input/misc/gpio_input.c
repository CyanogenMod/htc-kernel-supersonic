/* drivers/input/misc/gpio_input.c
 *
 * Copyright (C) 2007 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/gpio_event.h>
#include <linux/hrtimer.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/wakelock.h>
#ifdef CONFIG_OPTICALJOYSTICK_CRUCIAL
#include <linux/curcial_oj.h>
#endif

enum {
	DEBOUNCE_UNSTABLE     = BIT(0),	/* Got irq, while debouncing */
	DEBOUNCE_PRESSED      = BIT(1),
	DEBOUNCE_NOTPRESSED   = BIT(2),
	DEBOUNCE_WAIT_IRQ     = BIT(3),	/* Stable irq state */
	DEBOUNCE_POLL         = BIT(4),	/* Stable polling state */

	DEBOUNCE_UNKNOWN =
		DEBOUNCE_PRESSED | DEBOUNCE_NOTPRESSED,
};

struct gpio_key_state {
	struct gpio_input_state *ds;
	uint8_t debounce;
#ifdef CONFIG_ARCH_MSM7X30
	struct work_struct work;
#endif
};

struct gpio_input_state {
	struct gpio_event_input_devs *input_devs;
	const struct gpio_event_input_info *info;
	struct hrtimer timer;
	int use_irq;
	int debounce_count;
	spinlock_t irq_lock;
	struct wake_lock wake_lock;
	struct gpio_key_state key_state[0];
};

static enum hrtimer_restart gpio_event_input_timer_func(struct hrtimer *timer)
{
	int i;
	int pressed;
	struct gpio_input_state *ds =
		container_of(timer, struct gpio_input_state, timer);
	unsigned gpio_flags = ds->info->flags;
	unsigned npolarity;
	int nkeys = ds->info->keymap_size;
	const struct gpio_event_direct_entry *key_entry;
	struct gpio_key_state *key_state;
	unsigned long irqflags;
	uint8_t debounce;

#if 0
	key_entry = kp->keys_info->keymap;
	key_state = kp->key_state;
	for (i = 0; i < nkeys; i++, key_entry++, key_state++)
		pr_info("gpio_read_detect_status %d %d\n", key_entry->gpio,
			gpio_read_detect_status(key_entry->gpio));
#endif
	key_entry = ds->info->keymap;
	key_state = ds->key_state;
	spin_lock_irqsave(&ds->irq_lock, irqflags);
	for (i = 0; i < nkeys; i++, key_entry++, key_state++) {
		debounce = key_state->debounce;
		if (debounce & DEBOUNCE_WAIT_IRQ)
			continue;
		if (key_state->debounce & DEBOUNCE_UNSTABLE) {
			debounce = key_state->debounce = DEBOUNCE_UNKNOWN;
			enable_irq(gpio_to_irq(key_entry->gpio));
			pr_info("gpio_keys_scan_keys: key %x-%x, %d "
				"(%d) continue debounce\n",
				ds->info->type, key_entry->code,
				i, key_entry->gpio);
		}
		npolarity = !(gpio_flags & GPIOEDF_ACTIVE_HIGH);
		pressed = gpio_get_value(key_entry->gpio) ^ npolarity;
		if (debounce & DEBOUNCE_POLL) {
			if (pressed == !(debounce & DEBOUNCE_PRESSED)) {
				ds->debounce_count++;
				key_state->debounce = DEBOUNCE_UNKNOWN;
				if (gpio_flags & GPIOEDF_PRINT_KEY_DEBOUNCE)
					pr_info("gpio_keys_scan_keys: key %x-"
						"%x, %d (%d) start debounce\n",
						ds->info->type, key_entry->code,
						i, key_entry->gpio);
			}
			continue;
		}
		if (pressed && (debounce & DEBOUNCE_NOTPRESSED)) {
			if (gpio_flags & GPIOEDF_PRINT_KEY_DEBOUNCE)
				pr_info("gpio_keys_scan_keys: key %x-%x, %d "
					"(%d) debounce pressed 1\n",
					ds->info->type, key_entry->code,
					i, key_entry->gpio);
			key_state->debounce = DEBOUNCE_PRESSED;
			continue;
		}
		if (!pressed && (debounce & DEBOUNCE_PRESSED)) {
			if (gpio_flags & GPIOEDF_PRINT_KEY_DEBOUNCE)
				pr_info("gpio_keys_scan_keys: key %x-%x, %d "
					"(%d) debounce pressed 0\n",
					ds->info->type, key_entry->code,
					i, key_entry->gpio);
			key_state->debounce = DEBOUNCE_NOTPRESSED;
			continue;
		}
		/* key is stable */
		ds->debounce_count--;
		if (ds->use_irq)
			key_state->debounce |= DEBOUNCE_WAIT_IRQ;
		else
			key_state->debounce |= DEBOUNCE_POLL;
		if (gpio_flags & GPIOEDF_PRINT_KEYS)
			pr_info("gpio_keys_scan_keys: key %x-%x, %d (%d) "
				"changed to %d\n", ds->info->type,
				key_entry->code, i, key_entry->gpio, pressed);
#ifdef CONFIG_OPTICALJOYSTICK_CRUCIAL
		if (key_entry->code == BTN_MOUSE) {
			pr_info("gpio_keys_scan_keys: OJ action key %x-%x, %d (%d) "
				"changed to %d\n", ds->info->type,
				key_entry->code, i, key_entry->gpio, pressed);
			curcial_oj_send_key(BTN_MOUSE, pressed);
		} else
#endif
			input_event(ds->input_devs->dev[key_entry->dev],
				ds->info->type, key_entry->code, pressed);
	}

#if 0
	key_entry = kp->keys_info->keymap;
	key_state = kp->key_state;
	for (i = 0; i < nkeys; i++, key_entry++, key_state++) {
		pr_info("gpio_read_detect_status %d %d\n", key_entry->gpio,
			gpio_read_detect_status(key_entry->gpio));
	}
#endif

	if (ds->debounce_count)
		hrtimer_start(timer, ds->info->debounce_time, HRTIMER_MODE_REL);
	else if (!ds->use_irq)
		hrtimer_start(timer, ds->info->poll_time, HRTIMER_MODE_REL);
	else
		wake_unlock(&ds->wake_lock);

	spin_unlock_irqrestore(&ds->irq_lock, irqflags);

	return HRTIMER_NORESTART;
}

void keypad_reprort_keycode(struct gpio_key_state *ks){
	struct gpio_input_state *ds = ks->ds;
	int keymap_index = ks - ds->key_state;
	const struct gpio_event_direct_entry *key_entry;
	int pressed;

	key_entry = &ds->info->keymap[keymap_index];

	pressed = gpio_get_value(key_entry->gpio) ^
			!(ds->info->flags & GPIOEDF_ACTIVE_HIGH);
		if (ds->info->flags & GPIOEDF_PRINT_KEYS)
			pr_info("keypad_reprort_keycode: key %x-%x, %d "
				"(%d) changed to %d\n",
				ds->info->type, key_entry->code, keymap_index,
				key_entry->gpio, pressed);

#ifdef CONFIG_OPTICALJOYSTICK_CRUCIAL
		if (ds->info->info.oj_btn && key_entry->code == BTN_MOUSE){
			curcial_oj_send_key(BTN_MOUSE, pressed);
			pr_info("keypad_reprort_keycode: OJ key %x-%x, %d "
				"(%d) changed to %d\n",
				ds->info->type, key_entry->code, keymap_index,
				key_entry->gpio, pressed);
		}else
#endif
			input_event(ds->input_devs->dev[key_entry->dev],
				ds->info->type, key_entry->code, pressed);
}

#ifdef CONFIG_ARCH_MSM7X30
static void keypad_do_work(struct work_struct *w){
	struct gpio_key_state *ks = container_of(w, struct gpio_key_state, work);

	pr_info("keypad_do_work\n");

	keypad_reprort_keycode(ks);

}
#endif

static irqreturn_t gpio_event_input_irq_handler(int irq, void *dev_id)
{
	struct gpio_key_state *ks = dev_id;
	struct gpio_input_state *ds = ks->ds;
	int keymap_index = ks - ds->key_state;
	const struct gpio_event_direct_entry *key_entry;
	unsigned long irqflags;
	int pressed;

	if (!ds->use_irq)
		return IRQ_HANDLED;

	key_entry = &ds->info->keymap[keymap_index];

	if (ds->info->debounce_time.tv64) {
		spin_lock_irqsave(&ds->irq_lock, irqflags);
		if (ks->debounce & DEBOUNCE_WAIT_IRQ) {
			ks->debounce = DEBOUNCE_UNKNOWN;
			if (ds->debounce_count++ == 0) {
				wake_lock(&ds->wake_lock);
#ifndef CONFIG_ARCH_MSM7X30
				hrtimer_start(
					&ds->timer, ds->info->debounce_time,
					HRTIMER_MODE_REL);
#endif
			}
			if (ds->info->flags & GPIOEDF_PRINT_KEY_DEBOUNCE)
				pr_info("gpio_event_input_irq_handler: "
					"key %x-%x, %d (%d) start debounce\n",
					ds->info->type, key_entry->code,
					keymap_index, key_entry->gpio);
		} else {
			disable_irq_nosync(irq);
			ks->debounce = DEBOUNCE_UNSTABLE;
		}
		spin_unlock_irqrestore(&ds->irq_lock, irqflags);
	} else {
		pressed = gpio_get_value(key_entry->gpio) ^
			!(ds->info->flags & GPIOEDF_ACTIVE_HIGH);
		if (ds->info->flags & GPIOEDF_PRINT_KEYS)
			pr_info("gpio_event_input_irq_handler: key %x-%x, %d "
				"(%d) changed to %d\n",
				ds->info->type, key_entry->code, keymap_index,
				key_entry->gpio, pressed);

#ifdef CONFIG_ARCH_MSM7X30
			schedule_work(&ks->work);/*PIC8058 would use mutex, so could not det gpio value in IRQ*/
#else
			keypad_reprort_keycode(ks);
#endif
	}
	return IRQ_HANDLED;
}

static int gpio_event_input_request_irqs(struct gpio_input_state *ds)
{
	int i;
	int err;
	unsigned int irq;
	unsigned long req_flags = IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING;

	for (i = 0; i < ds->info->keymap_size; i++) {
		err = irq = gpio_to_irq(ds->info->keymap[i].gpio);
		if (err < 0)
			goto err_gpio_get_irq_num_failed;
#ifdef CONFIG_ARCH_MSM7X30
		INIT_WORK(&ds->key_state[i].work, keypad_do_work);
		schedule_work(&ds->key_state[i].work);
#endif
		err = request_irq(irq, gpio_event_input_irq_handler,
				  req_flags, "gpio_keys", &ds->key_state[i]);
		if (err) {
			pr_err("gpio_event_input_request_irqs: request_irq "
				"failed for input %d, irq %d\n",
				ds->info->keymap[i].gpio, irq);
			goto err_request_irq_failed;
		}
		enable_irq_wake(irq);
	}
	return 0;

	for (i = ds->info->keymap_size - 1; i >= 0; i--) {
		free_irq(gpio_to_irq(ds->info->keymap[i].gpio),
			 &ds->key_state[i]);
err_request_irq_failed:
err_gpio_get_irq_num_failed:
		;
	}
	return err;
}

int gpio_event_input_func(struct gpio_event_input_devs *input_devs,
			struct gpio_event_info *info, void **data, int func)
{
	int ret;
	int i;
	unsigned long irqflags;
	struct gpio_event_input_info *di;
	struct gpio_input_state *ds = *data;

	di = container_of(info, struct gpio_event_input_info, info);

	if (func == GPIO_EVENT_FUNC_SUSPEND) {
		if (ds->use_irq)
			for (i = 0; i < di->keymap_size; i++)
				disable_irq(gpio_to_irq(di->keymap[i].gpio));
#ifndef CONFIG_ARCH_MSM7X30
		hrtimer_cancel(&ds->timer);
#endif
		return 0;
	}
	if (func == GPIO_EVENT_FUNC_RESUME) {
		spin_lock_irqsave(&ds->irq_lock, irqflags);
		if (ds->use_irq)
			for (i = 0; i < di->keymap_size; i++)
				enable_irq(gpio_to_irq(di->keymap[i].gpio));
#ifdef CONFIG_ARCH_MSM7X30
		for (i = 0; i < ds->info->keymap_size; i++) {
			schedule_work(&ds->key_state[i].work);
		}
#else
		hrtimer_start(&ds->timer, ktime_set(0, 0), HRTIMER_MODE_REL);
#endif
		spin_unlock_irqrestore(&ds->irq_lock, irqflags);
		return 0;
	}

	if (func == GPIO_EVENT_FUNC_INIT) {
		if (ktime_to_ns(di->poll_time) <= 0)
			di->poll_time = ktime_set(0, 20 * NSEC_PER_MSEC);

		*data = ds = kzalloc(sizeof(*ds) + sizeof(ds->key_state[0]) *
					di->keymap_size, GFP_KERNEL);
		if (ds == NULL) {
			ret = -ENOMEM;
			pr_err("gpio_event_input_func: "
				"Failed to allocate private data\n");
			goto err_ds_alloc_failed;
		}
		ds->debounce_count = di->keymap_size;
		ds->input_devs = input_devs;
		ds->info = di;
		wake_lock_init(&ds->wake_lock, WAKE_LOCK_SUSPEND, "gpio_input");
		spin_lock_init(&ds->irq_lock);

		for (i = 0; i < di->keymap_size; i++) {
			int dev = di->keymap[i].dev;
			if (dev >= input_devs->count) {
				pr_err("gpio_event_input_func: bad device "
					"index %d >= %d for key code %d\n",
					dev, input_devs->count,
					di->keymap[i].code);
				ret = -EINVAL;
				goto err_bad_keymap;
			}
			input_set_capability(input_devs->dev[dev], di->type,
					     di->keymap[i].code);
			ds->key_state[i].ds = ds;
			ds->key_state[i].debounce = DEBOUNCE_UNKNOWN;
		}

		for (i = 0; i < di->keymap_size; i++) {
			ret = gpio_request(di->keymap[i].gpio, "gpio_kp_in");
			if (ret) {
				pr_err("gpio_event_input_func: gpio_request "
					"failed for %d\n", di->keymap[i].gpio);
				goto err_gpio_request_failed;
			}
			ret = gpio_direction_input(di->keymap[i].gpio);
			if (ret) {
				pr_err("gpio_event_input_func: "
					"gpio_direction_input failed for %d\n",
					di->keymap[i].gpio);
				goto err_gpio_configure_failed;
			}
		}
		if (di->setup_input_gpio)
			di->setup_input_gpio();

		ret = gpio_event_input_request_irqs(ds);

		spin_lock_irqsave(&ds->irq_lock, irqflags);
		ds->use_irq = ret == 0;

		pr_info("GPIO Input Driver: Start gpio inputs for %s%s in %s "
			"mode\n", input_devs->dev[0]->name,
			(input_devs->count > 1) ? "..." : "",
			ret == 0 ? "interrupt" : "polling");

		hrtimer_init(&ds->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		ds->timer.function = gpio_event_input_timer_func;
#ifndef CONFIG_ARCH_MSM7X30
		hrtimer_start(&ds->timer, ktime_set(0, 0), HRTIMER_MODE_REL);
#endif
		spin_unlock_irqrestore(&ds->irq_lock, irqflags);
		return 0;
	}

	ret = 0;
	spin_lock_irqsave(&ds->irq_lock, irqflags);
#ifndef CONFIG_ARCH_MSM7X30
	hrtimer_cancel(&ds->timer);
#endif
	if (ds->use_irq) {
		for (i = di->keymap_size - 1; i >= 0; i--) {
			free_irq(gpio_to_irq(di->keymap[i].gpio),
				 &ds->key_state[i]);
		}
	}
	spin_unlock_irqrestore(&ds->irq_lock, irqflags);

	for (i = di->keymap_size - 1; i >= 0; i--) {
err_gpio_configure_failed:
		gpio_free(di->keymap[i].gpio);
err_gpio_request_failed:
		;
	}
err_bad_keymap:
	wake_lock_destroy(&ds->wake_lock);
	kfree(ds);
err_ds_alloc_failed:
	return ret;
}
