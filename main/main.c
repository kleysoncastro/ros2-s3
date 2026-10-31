#include <stdio.h>
#include <unistd.h>

#include "esp_log.h"
#include "esp_usbcdc_logging.h"
#include "esp_usbcdc_transport.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <rcl/error_handling.h>
#include <rcl/rcl.h>
#include <rclc/executor.h>
#include <rclc/rclc.h>
#include <rmw_microros/rmw_microros.h>
#include <rmw_microxrcedds_c/config.h>
#include <std_msgs/msg/color_rgba.h>
#include <std_msgs/msg/int32.h>

#include "led_strip.h"

#define TIMER_PERIOD_MS 1000
#define NUMBER_OF_HANDLES 2

#define NODE_NAME "esp32s3"
#define TOPIC_NAME "/esp32s3/counter"
#define LED_TOPIC_NAME "/esp32s3/led"

#define TAG_MAIN "MAIN"
#define TAG_TASK "MICRO_ROS"

// Aborts the micro-ROS task on error
#define RCCHECK(fn) {                                                                   \
    rcl_ret_t temp_rc = fn;                                                             \
    if (temp_rc != RCL_RET_OK) {                                                        \
        ESP_LOGE("RCCHECK", "Failed status on line %d: %d. Aborting.", __LINE__, (int)temp_rc); \
        vTaskDelete(NULL);                                                              \
    }                                                                                   \
}

// Logs the error and keeps going
#define RCSOFTCHECK(fn) {                                                               \
    rcl_ret_t temp_rc = fn;                                                             \
    if (temp_rc != RCL_RET_OK) {                                                        \
        ESP_LOGW("RCSOFTCHECK", "Failed status on line %d: %d. Continuing.", __LINE__, (int)temp_rc); \
    }                                                                                   \
}

static rcl_publisher_t publisher;
static std_msgs__msg__Int32 msg;
static rcl_subscription_t led_sub;
static std_msgs__msg__ColorRGBA led_msg;
static led_strip_handle_t led_strip;
static tinyusb_cdcacm_itf_t cdc_port = TINYUSB_CDC_ACM_0;

static void timer_callback(rcl_timer_t *timer, int64_t last_call_time)
{
    RCLC_UNUSED(last_call_time);
    if (timer != NULL) {
        RCSOFTCHECK(rcl_publish(&publisher, &msg, NULL));
        ESP_LOGI(TAG_TASK, "Published: %ld", (long)msg.data++);
    }
}

static uint8_t to_u8(float v)
{
    if (v < 0.0f) v = 0.0f;
    if (v > 1.0f) v = 1.0f;
    return (uint8_t)(v * 255.0f);
}

static void led_init(void)
{
    led_strip_config_t strip_config = {
        .strip_gpio_num = CONFIG_LED_STRIP_GPIO,
        .max_leds = 1,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
    };
    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000,
    };
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    led_strip_clear(led_strip);
}

static void led_callback(const void *msgin)
{
    const std_msgs__msg__ColorRGBA *c = (const std_msgs__msg__ColorRGBA *)msgin;
    led_strip_set_pixel(led_strip, 0, to_u8(c->r), to_u8(c->g), to_u8(c->b));
    led_strip_refresh(led_strip);
    ESP_LOGI(TAG_TASK, "LED r=%.2f g=%.2f b=%.2f", c->r, c->g, c->b);
}

static void micro_ros_task(void *arg)
{
    rcl_allocator_t allocator = rcl_get_default_allocator();
    rclc_support_t support;

    RCCHECK(rclc_support_init(&support, 0, NULL, &allocator));

    rcl_node_t node;
    RCCHECK(rclc_node_init_default(&node, NODE_NAME, "", &support));

    RCCHECK(rclc_publisher_init_default(
        &publisher, &node,
        ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Int32),
        TOPIC_NAME));

    rcl_timer_t timer;
    RCCHECK(rclc_timer_init_default2(
        &timer, &support, RCL_MS_TO_NS(TIMER_PERIOD_MS), timer_callback, true));

    RCCHECK(rclc_subscription_init_default(
        &led_sub, &node,
        ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, ColorRGBA),
        LED_TOPIC_NAME));

    rclc_executor_t executor;
    RCCHECK(rclc_executor_init(&executor, &support.context, NUMBER_OF_HANDLES, &allocator));
    RCCHECK(rclc_executor_add_timer(&executor, &timer));
    RCCHECK(rclc_executor_add_subscription(&executor, &led_sub, &led_msg, &led_callback, ON_NEW_DATA));

    msg.data = 0;

    while (true) {
        rcl_ret_t ret = rclc_executor_spin_some(&executor, RCL_MS_TO_NS(100));
        if (ret != RCL_RET_OK) {
            ESP_LOGE(TAG_TASK, "rclc_executor_spin_some() failed");
        }
        usleep(10000);
    }

    RCCHECK(rcl_subscription_fini(&led_sub, &node));
    RCCHECK(rcl_publisher_fini(&publisher, &node));
    RCCHECK(rcl_node_fini(&node));
    vTaskDelete(NULL);
}

void app_main(void)
{
    led_init();

    // Second CDC interface carries the ESP_LOG output
#if (CONFIG_TINYUSB_CDC_COUNT >= 2)
    if (esp_usbcdc_logging_init() == ESP_OK) {
        ESP_LOGI(TAG_MAIN, "USB-CDC logging initialized");
    }
#endif

#if defined(RMW_UXRCE_TRANSPORT_CUSTOM)
    rmw_ret_t ret = rmw_uros_set_custom_transport(
        true, (void *)&cdc_port,
        esp_usbcdc_open, esp_usbcdc_close,
        esp_usbcdc_write, esp_usbcdc_read);

    if (ret != RMW_RET_OK) {
        ESP_LOGE(TAG_MAIN, "Failed to set micro-ROS custom transport");
        return;
    }
#else
#error micro-ROS transports misconfigured
#endif

    xTaskCreate(micro_ros_task, "uros_task",
                CONFIG_MICRO_ROS_APP_STACK, NULL,
                CONFIG_MICRO_ROS_APP_TASK_PRIO, NULL);
}
