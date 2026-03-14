Below is a clean **Markdown file content** you can save as something like **`button_api.md`**.

````md
# Button API and Mouse Mapping Specification

## 1. Overview

This document defines the **generic button API** for the project and the **mouse mapping layer** built on top of it.

The design goal is:

- keep the **button driver generic and reusable**
- keep the **mouse behavior separate**
- allow the same button API to be reused later in other projects

This project currently uses **two buttons**:

- **B1** → acts as **Right Click**
- **B2** → acts as **Left Click**

---

## 2. Hardware Mapping

### Button GPIO Assignment

- **Button 1 (B1)** → **GPIO19**
- **Button 2 (B2)** → **GPIO18**

### Button Role Mapping

- **B1** → **Right Click**
- **B2** → **Left Click**

### Recommended Electrical Configuration

The recommended button wiring is:

- one side of the button connected to the **GPIO**
- the other side connected to **GND**
- GPIO configured as **INPUT_PULLUP**

This means the button logic is:

- **not pressed** → GPIO reads **HIGH**
- **pressed** → GPIO reads **LOW**

This is referred to as **active-low** operation.

---

## 3. Timing Specification

The following timing values are the **default project values**.

## 3.1 Debounce

Debounce is required to prevent false triggers caused by mechanical button bounce.

- **Debounce time**: **20 ms**

## 3.2 Short Click

A short click is a valid press-and-release within the following range:

- **minimum short click duration**: **50 ms**
- **maximum short click duration**: **250 ms**

If the press duration is below the minimum, it is ignored as noise.

## 3.3 Long Click

A long click is detected when the button remains pressed for:

- **long click threshold**: **500 ms or more**

## 3.4 Double Click

A double click is detected when:

- the first short click is completed
- the second short click occurs within the allowed gap

### Double-click gap

- **maximum gap between clicks**: **250 ms**

---

## 4. Design Rules

1. The **button driver** must only detect button events.
2. The **button driver must not contain mouse-specific behavior**.
3. The **mouse layer** will map button events to mouse actions.
4. The API must remain reusable for future projects.

---

## 5. Status Type

```c
typedef enum {
    BTN_OK = 0,
    BTN_ERR_INVALID_ARG,
    BTN_ERR_NOT_INIT,
    BTN_ERR_ALREADY_INIT,
    BTN_ERR_INTERNAL
} status_btn;
````

---

## 6. Button ID Type

```c
typedef enum {
    BTN_ID_B1 = 0,
    BTN_ID_B2,
    BTN_ID_MAX
} button_id_t;
```

---

## 7. Button Function Type

This defines the logical role of the button.

```c
typedef enum {
    BTN_FUNC_NONE = 0,
    BTN_FUNC_LEFT_CLICK,
    BTN_FUNC_RIGHT_CLICK,
    BTN_FUNC_MIDDLE_CLICK,
    BTN_FUNC_CUSTOM
} button_function_t;
```

---

## 8. Button Event Type

```c
typedef enum {
    BTN_EVENT_NONE = 0,
    BTN_EVENT_PRESS,
    BTN_EVENT_RELEASE,
    BTN_EVENT_SHORT_CLICK,
    BTN_EVENT_LONG_CLICK,
    BTN_EVENT_DOUBLE_CLICK
} button_event_t;
```

---

## 9. Button Structure

The button structure contains:

* button identity
* logical function
* GPIO number
* polarity
* timing configuration
* runtime state
* last detected event

```c
typedef struct {
    button_id_t id;
    button_function_t function;
    int gpio;
    bool active_low;

    uint32_t debounce_ms;
    uint32_t short_click_min_ms;
    uint32_t short_click_max_ms;
    uint32_t long_click_ms;
    uint32_t double_click_gap_ms;

    bool initialized;
    bool pressed;
    bool long_reported;

    uint32_t last_change_time_ms;
    uint32_t press_time_ms;
    uint32_t release_time_ms;

    button_event_t last_event;
} button_t;
```

---

## 10. Default Button Configuration for This Project

```c
button_t button_b1 = {
    .id = BTN_ID_B1,
    .function = BTN_FUNC_RIGHT_CLICK,
    .gpio = 19,
    .active_low = true,
    .debounce_ms = 20,
    .short_click_min_ms = 50,
    .short_click_max_ms = 250,
    .long_click_ms = 500,
    .double_click_gap_ms = 250
};

button_t button_b2 = {
    .id = BTN_ID_B2,
    .function = BTN_FUNC_LEFT_CLICK,
    .gpio = 18,
    .active_low = true,
    .debounce_ms = 20,
    .short_click_min_ms = 50,
    .short_click_max_ms = 250,
    .long_click_ms = 500,
    .double_click_gap_ms = 250
};
```

---

## 11. Button API

## 11.1 Initialization API

```c
status_btn btn_init(button_t *btn);
status_btn btn_deinit(button_t *btn);
```

### Description

* `btn_init()` initializes the GPIO and internal state of the button
* `btn_deinit()` releases or disables the button logic if needed

---

## 11.2 Update API

```c
status_btn btn_update(button_t *btn, uint32_t now_ms);
```

### Description

This function must be called periodically.

It is responsible for:

* reading the GPIO state
* applying debounce
* detecting press/release
* detecting short click
* detecting long click
* detecting double click

---

## 11.3 Event Retrieval API

```c
button_event_t btn_get_event(button_t *btn);
button_event_t btn_peek_event(const button_t *btn);
```

### Description

* `btn_get_event()` returns the last event and clears it
* `btn_peek_event()` returns the last event without clearing it

---

## 11.4 State Query API

```c
bool btn_is_pressed(const button_t *btn);
button_function_t btn_get_function(const button_t *btn);
button_id_t btn_get_id(const button_t *btn);
int btn_get_gpio(const button_t *btn);
```

---

## 11.5 Convenience API

These helper functions can be added for easier application logic.

```c
bool btn_is_short_click(button_t *btn);
bool btn_is_long_click(button_t *btn);
bool btn_is_double_click(button_t *btn);
```

---

## 12. Mouse API

The mouse API is a separate layer that maps button events to mouse behavior.

---

## 12.1 Mouse Status Type

```c
typedef enum {
    MOUSE_OK = 0,
    MOUSE_ERR_INVALID_ARG,
    MOUSE_ERR_NOT_INIT,
    MOUSE_ERR_NOT_CONNECTED,
    MOUSE_ERR_INTERNAL
} status_mouse;
```

---

## 12.2 Mouse Button Type

```c
typedef enum {
    MOUSE_BTN_LEFT = 0,
    MOUSE_BTN_RIGHT,
    MOUSE_BTN_MIDDLE
} mouse_button_t;
```

---

## 12.3 Mouse Initialization API

```c
status_mouse mouse_init(void);
status_mouse mouse_deinit(void);
```

---

## 12.4 Mouse Movement API

```c
status_mouse mouse_move(int8_t dx, int8_t dy);
status_mouse mouse_scroll(int8_t wheel);
```

---

## 12.5 Mouse Button Action API

```c
status_mouse mouse_press(mouse_button_t btn);
status_mouse mouse_release(mouse_button_t btn);
status_mouse mouse_click(mouse_button_t btn);
status_mouse mouse_double_click(mouse_button_t btn);
status_mouse mouse_long_click(mouse_button_t btn);
```

### Description

* `mouse_press()` presses and holds the specified mouse button
* `mouse_release()` releases the specified mouse button
* `mouse_click()` performs a normal single click
* `mouse_double_click()` performs a double click
* `mouse_long_click()` performs a press-hold-release sequence based on the long-click logic

---

## 12.6 Mouse State API

```c
bool mouse_is_ready(void);
bool mouse_is_connected(void);
```

---

## 13. Button-to-Mouse Mapping API

This layer translates button events into mouse actions.

```c
status_mouse mouse_handle_button_event(button_t *btn, button_event_t event);
```

### Mapping Rules

#### B1 → Right Click

* `BTN_EVENT_SHORT_CLICK` → `mouse_click(MOUSE_BTN_RIGHT)`
* `BTN_EVENT_DOUBLE_CLICK` → `mouse_double_click(MOUSE_BTN_RIGHT)`
* `BTN_EVENT_LONG_CLICK` → `mouse_long_click(MOUSE_BTN_RIGHT)`

#### B2 → Left Click

* `BTN_EVENT_SHORT_CLICK` → `mouse_click(MOUSE_BTN_LEFT)`
* `BTN_EVENT_DOUBLE_CLICK` → `mouse_double_click(MOUSE_BTN_LEFT)`
* `BTN_EVENT_LONG_CLICK` → `mouse_long_click(MOUSE_BTN_LEFT)`

---

## 14. Example Event Handling Flow

```c
btn_update(&button_b1, now_ms);
btn_update(&button_b2, now_ms);

button_event_t evt_b1 = btn_get_event(&button_b1);
button_event_t evt_b2 = btn_get_event(&button_b2);

if (evt_b1 != BTN_EVENT_NONE) {
    mouse_handle_button_event(&button_b1, evt_b1);
}

if (evt_b2 != BTN_EVENT_NONE) {
    mouse_handle_button_event(&button_b2, evt_b2);
}
```

---

## 15. Summary

### Buttons

* **B1** → GPIO19 → Right Click
* **B2** → GPIO18 → Left Click

### Timing

* **Debounce**: 20 ms
* **Short click**: 50 ms to 250 ms
* **Long click**: 500 ms or more
* **Double-click gap**: 250 ms maximum

### Architecture

* **Button API** detects events
* **Mouse API** performs mouse actions
* **Mapping layer** connects button events to mouse behavior

