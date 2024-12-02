from gpio_handler import GPIOHandler


class VibrationMotor:
    """진동 모터 제어 클래스"""

    def __init__(self, gpio_handler: GPIOHandler, pin):
        self.gpio_handler = gpio_handler
        self.pin = pin
        self.gpio_handler.setup_pin(self.pin, mode=1)  # GPIO.OUT

    def on(self):
        """진동 모터 켜기"""
        self.gpio_handler.write_pin(self.pin, True)  # GPIO.HIGH

    def off(self):
        """진동 모터 끄기"""
        self.gpio_handler.write_pin(self.pin, False)  # GPIO.LOW
