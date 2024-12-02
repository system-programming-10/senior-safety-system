from gpio_handler import GPIOHandler


class RGBLED:
    """RGB LED 제어 클래스"""

    def __init__(self, gpio_handler: GPIOHandler, red_pin, green_pin, blue_pin):
        self.gpio_handler = gpio_handler
        self.red_pin = red_pin
        self.green_pin = green_pin
        self.blue_pin = blue_pin

        # 핀 설정
        for pin in (self.red_pin, self.green_pin, self.blue_pin):
            self.gpio_handler.setup_pin(pin, mode=1)  # GPIO.OUT

    def on(self):
        """RGB LED 켜기 (모든 색 활성화)"""
        for pin in (self.red_pin, self.green_pin, self.blue_pin):
            self.gpio_handler.write_pin(pin, True)  # GPIO.HIGH

    def off(self):
        """RGB LED 끄기 (모든 색 비활성화)"""
        for pin in (self.red_pin, self.green_pin, self.blue_pin):
            self.gpio_handler.write_pin(pin, False)  # GPIO.LOW
