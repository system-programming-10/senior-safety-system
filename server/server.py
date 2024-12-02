from device_controller import DeviceController
from redis_handler import RedisConfig, RedisHandler


class Server:
    def __init__(self, redis_config: RedisConfig, gpio_pins={}):
        self.redis_handler = RedisHandler(redis_config)
        self.device_controller = DeviceController(
            gpio_handler=gpio_pins["gpio_handler"],
            vibration_pin=gpio_pins["vibration_motor"],
            red_pin=gpio_pins["rgb_led_red"],
            green_pin=gpio_pins["rgb_led_green"],
            blue_pin=gpio_pins["rgb_led_blue"],
        )

    def run(self):
        try:
            for raw_data in self.redis_handler.listen():
                self.device_controller.handle_message(raw_data)

        except KeyboardInterrupt:
            print("Exiting...")

        finally:
            self.device_controller.cleanup()
