import RPi.GPIO as GPIO


class GPIOHandler:
    def __init__(self):
        GPIO.setmode(GPIO.BCM)  # BCM 핀 넘버링 사용
        self.pins = {}

    def setup_pin(self, pin, mode):
        """핀을 입력 또는 출력으로 설정"""
        GPIO.setup(pin, mode)
        self.pins[pin] = mode

    def write_pin(self, pin, state):
        """핀에 HIGH(1) 또는 LOW(0)를 출력"""
        if pin not in self.pins:
            raise ValueError(f"Pin {pin} not set up. Use setup_pin() first.")
        GPIO.output(pin, state)

    def cleanup(self):
        """GPIO 핀 정리"""
        GPIO.cleanup()
