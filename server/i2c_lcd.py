import smbus
import time


class I2CLCD:
    def __init__(self, address=0x27, width=16):
        self.I2C_ADDR = address
        self.LCD_WIDTH = width

        # LCD 명령어
        self.LCD_CHR = 1  # 데이터 전송
        self.LCD_CMD = 0  # 명령어 전송

        self.LCD_LINE_1 = 0x80  # 1행 주소
        self.LCD_LINE_2 = 0xC0  # 2행 주소

        self.LCD_BACKLIGHT = 0x08  # 백라이트 켜기
        self.ENABLE = 0b00000100  # Enable 비트

        # 딜레이
        self.E_PULSE = 0.0005
        self.E_DELAY = 0.0005

        # I2C 인터페이스 초기화
        self.bus = smbus.SMBus(1)  # Raspberry Pi에서 I2C-1 버스 사용

        # LCD 초기화
        self.lcd_init()

    def lcd_byte(self, bits, mode):
        """LCD에 데이터를 보내는 함수."""
        bits_high = mode | (bits & 0xF0) | self.LCD_BACKLIGHT
        bits_low = mode | ((bits << 4) & 0xF0) | self.LCD_BACKLIGHT

        # 상위 비트 전송
        self.bus.write_byte(self.I2C_ADDR, bits_high)
        self.lcd_toggle_enable(bits_high)

        # 하위 비트 전송
        self.bus.write_byte(self.I2C_ADDR, bits_low)
        self.lcd_toggle_enable(bits_low)

    def lcd_toggle_enable(self, bits):
        """Enable 신호를 토글하여 LCD에 데이터를 쓰도록 트리거."""
        time.sleep(self.E_DELAY)
        self.bus.write_byte(self.I2C_ADDR, (bits | self.ENABLE))
        time.sleep(self.E_PULSE)
        self.bus.write_byte(self.I2C_ADDR, (bits & ~self.ENABLE))
        time.sleep(self.E_DELAY)

    def lcd_init(self):
        """LCD 초기화."""
        self.lcd_byte(0x33, self.LCD_CMD)  # 초기화 명령어
        self.lcd_byte(0x32, self.LCD_CMD)  # 초기화 명령어
        self.lcd_byte(0x06, self.LCD_CMD)  # 커서를 오른쪽으로 이동
        self.lcd_byte(0x0C, self.LCD_CMD)  # 화면 켜기, 커서 끄기
        self.lcd_byte(0x28, self.LCD_CMD)  # 2행 모드
        self.lcd_byte(0x01, self.LCD_CMD)  # 화면 지우기
        time.sleep(self.E_DELAY)

    def lcd_string(self, message, line):
        """LCD에 문자열을 출력."""
        message = message.ljust(self.LCD_WIDTH, " ")  # 문자열을 LCD 너비에 맞추기
        self.lcd_byte(line, self.LCD_CMD)
        for i in range(self.LCD_WIDTH):
            self.lcd_byte(ord(message[i]), self.LCD_CHR)

    def print(self, message):
        """LCD에 메시지를 출력하는 인터페이스."""
        lines = message.split("\n")
        if len(lines) > 0:
            self.lcd_string(lines[0], self.LCD_LINE_1)
        if len(lines) > 1:
            self.lcd_string(lines[1], self.LCD_LINE_2)
