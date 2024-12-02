import time

from gpio_handler import GPIOHandler
from i2c_lcd import I2CLCD
from redis_handler import RedisConfig, RedisHandler
from rgb_led import RGBLED
from server import Server
from vibration_motor import VibrationMotor

REDIS_HOST = ""
REDIS_PORT = 6379
REDIS_CHANNEL = "test"


if __name__ == "__main__":
    gpio_pins = {
        "gpio_handler": GPIOHandler(),
        "vibration_motor": 17,
        "rgb_led_red": 22,
        "rgb_led_green": 27,
        "rgb_led_blue": 24,
    }
    redis_config = RedisConfig(host=REDIS_HOST, port=REDIS_PORT, channel=REDIS_CHANNEL)

    server = Server(redis_config=redis_config, gpio_pins=gpio_pins)
    server.run()
