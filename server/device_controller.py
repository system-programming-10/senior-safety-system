import time
from gpio_handler import GPIOHandler
from i2c_lcd import I2CLCD
from rgb_led import RGBLED
from vibration_motor import VibrationMotor


class DeviceController:
    """장치 제어 및 메시지 처리 클래스"""

    TYPE_MESSAGE_MAP = {
        "0": "화재 발생!",
        "1": "낙상 사고!",
        "2": "hi yo~!",
    }

    def __init__(
        self, gpio_handler: GPIOHandler, vibration_pin, red_pin, green_pin, blue_pin
    ):
        self.gpio_handler = gpio_handler
        self.vibration_motor = VibrationMotor(gpio_handler, pin=vibration_pin)
        self.rgb_led = RGBLED(
            gpio_handler, red_pin=red_pin, green_pin=green_pin, blue_pin=blue_pin
        )
        self.lcd = I2CLCD()

    def parse_message(self, message: str):
        """Redis 메시지 파싱"""
        parts = message.split()

        if len(parts) != 6:
            raise ValueError(
                "Invalid message format. Expected: 'type {type} value {value} address {address}'"
            )

        type_ = parts[1]
        value = parts[3]
        address = parts[5]

        return type_, value, address

    def handle_message(self, raw_data):
        """수신한 메시지를 처리"""
        try:
            type_, value, address = self.parse_message(raw_data)
            print(f"Parsed - Type: {type_}, Value: {value}, Address: {address}")

            # LCD 출력
            lcd_message = self.TYPE_MESSAGE_MAP.get(type_, "Unknown")
            self.lcd.print(f"{lcd_message}\nAddr: {address}")

            # 진동 모터 및 LED 활성화
            print("Activating vibration motor and LED...")
            self.vibration_motor.on()
            self.rgb_led.on()

            # 2초간 활성화
            time.sleep(2)

            # 진동 모터 및 LED 비활성화
            self.vibration_motor.off()
            self.rgb_led.off()
            print("Deactivated vibration motor and LED.")

        except ValueError as e:
            print(f"Error parsing message: {e}")

    def cleanup(self):
        """GPIO 정리"""
        self.gpio_handler.cleanup()
        print("Cleaned up GPIO.")
