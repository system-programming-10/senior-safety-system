#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <linux/spi/spidev.h>
#include <sys/ioctl.h>
#include <time.h>
#include <hiredis/hiredis.h>

#define IN 0
#define OUT 1

#define LOW 0
#define HIGH 1

#define IR_SENSOR_PIN 27
#define MOTOR_GPIO_PIN 21

#define PIN 17
#define POUT2 18
#define VALUE_MAX 40
#define DIRECTION_MAX 40

// GPIO 설정
#define SPI_DEVICE "/dev/spidev0.0"
#define SPI_MODE 0
#define SPI_BITS 8
#define SPI_SPEED 1000000
#define SPI_DELAY 5

#define PWM 0

//GPIO Funtions
static int GPIOExport(int pin) {
#define BUFFER_MAX 3
  char buffer[BUFFER_MAX];
  ssize_t bytes_written;
  int fd;

  fd = open("/sys/class/gpio/export", O_WRONLY);
  if (-1 == fd) {
    fprintf(stderr, "Failed to open export for writing!\n");
    return (-1);
  }

  bytes_written = snprintf(buffer, BUFFER_MAX, "%d", pin);
  write(fd, buffer, bytes_written);
  close(fd);
  return (0);
}

static int GPIOUnexport(int pin) {
  char buffer[BUFFER_MAX];
  ssize_t bytes_written;
  int fd;

  fd = open("/sys/class/gpio/unexport", O_WRONLY);
  if (-1 == fd) {
    fprintf(stderr, "Failed to open unexport for writing!\n");
    return (-1);
  }

  bytes_written = snprintf(buffer, BUFFER_MAX, "%d", pin);
  write(fd, buffer, bytes_written);
  close(fd);
  return (0);
}

static int GPIODirection(int pin, int dir) {
  static const char s_directions_str[] = "in\0out";

  char path[DIRECTION_MAX];
  int fd;

  snprintf(path, DIRECTION_MAX, "/sys/class/gpio/gpio%d/direction", pin);
  fd = open(path, O_WRONLY);
  if (-1 == fd) {
    fprintf(stderr, "Failed to open gpio direction for writing!\n");
    return (-1);
  }

  if (-1 ==
      write(fd, &s_directions_str[IN == dir ? 0 : 3], IN == dir ? 2 : 3)) {
    fprintf(stderr, "Failed to set direction!\n");
    return (-1);
  }

  close(fd);
  return (0);
}

static int GPIORead(int pin) {
  char path[VALUE_MAX];
  char value_str[3];
  int fd;

  snprintf(path, VALUE_MAX, "/sys/class/gpio/gpio%d/value", pin);
  fd = open(path, O_RDONLY);
  if (-1 == fd) {
    fprintf(stderr, "Failed to open gpio value for reading!\n");
    return (-1);
  }

  if (-1 == read(fd, value_str, 3)) {
    fprintf(stderr, "Failed to read value!\n");
    return (-1);
  }

  close(fd);

  return (atoi(value_str));
}

static int GPIOWrite(int pin, int value) {
  static const char s_values_str[] = "01";

  char path[VALUE_MAX];
  int fd;

  snprintf(path, VALUE_MAX, "/sys/class/gpio/gpio%d/value", pin);
  fd = open(path, O_WRONLY);
  if (-1 == fd) {
    fprintf(stderr, "Failed to open gpio value for writing!\n");
    return (-1);
  }

  if (1 != write(fd, &s_values_str[LOW == value ? 0 : 1], 1)) {
    fprintf(stderr, "Failed to write value!\n");
    return (-1);
  }

  close(fd);
  return (0);
}

// PWM
static int PWMExport(int pwmnum) {
#define BUFFER_MAX 3
  char buffer[BUFFER_MAX];
  int fd, byte;

  // TODO: Enter the export path.
  fd = open("/sys/class/pwm/pwmchip0/export", O_WRONLY);
  if (-1 == fd) {
    fprintf(stderr, "Failed to open export for export!\n");
    return (-1);
  }

  byte = snprintf(buffer, BUFFER_MAX, "%d", pwmnum);
  write(fd, buffer, byte);
  close(fd);

  sleep(1);

  return (0);
}

static int PWMEnable(int pwmnum) {
  static const char s_enable_str[] = "1";

  char path[DIRECTION_MAX];
  int fd;

  // TODO: Enter the enable path.
  snprintf(path, DIRECTION_MAX, "/sys/class/pwm/pwmchip0/pwm0/enable", pwmnum);
  fd = open(path, O_WRONLY);
  if (-1 == fd) {
    fprintf(stderr, "Failed to open in enable!\n");
    return -1;
  }

  write(fd, s_enable_str, strlen(s_enable_str));
  close(fd);

  return (0);
}

static int PWMWritePeriod(int pwmnum, int value) {
  char s_value_str[VALUE_MAX];
  char path[VALUE_MAX];
  int fd, byte;

  // TODO: Enter the period path.
  snprintf(path, VALUE_MAX, "/sys/class/pwm/pwmchip0/pwm0/period", pwmnum);
  fd = open(path, O_WRONLY);
  if (-1 == fd) {
    fprintf(stderr, "Failed to open in period!\n");
    return (-1);
  }
  byte = snprintf(s_value_str, VALUE_MAX, "%d", value);

  if (-1 == write(fd, s_value_str, byte)) {
    fprintf(stderr, "Failed to write value in period!\n");
    close(fd);
    return -1;
  }
  close(fd);

  return (0);
}

static int PWMWriteDutyCycle(int pwmnum, int value) {
  char s_value_str[VALUE_MAX];
  char path[VALUE_MAX];
  int fd, byte;

  // TODO: Enter the duty_cycle path.
  snprintf(path, VALUE_MAX, "/sys/class/pwm/pwmchip0/pwm0/duty_cycle", pwmnum);
  fd = open(path, O_WRONLY);
  if (-1 == fd) {
    fprintf(stderr, "Failed to open in duty cycle!\n");
    return (-1);
  }
  byte = snprintf(s_value_str, VALUE_MAX, "%d", value);

  if (-1 == write(fd, s_value_str, byte)) {
    fprintf(stderr, "Failed to write value in duty cycle!\n");
    close(fd);
    return -1;
  }
  close(fd);

  return (0);
}

// PWM 비활성화 함수
int pwm_disable() {
    int fd = open("/sys/class/pwm/pwmchip0/pwm0/enable", O_WRONLY);
    if (fd < 0) {
        perror("Failed to disable PWM");
        return -1;
    }
    if (write(fd, "0", 1) != 1) {
        perror("Failed to write to enable");
        close(fd);
        return -1;
    }
    close(fd);
    return 0;
}

// SPI 초기화
int init_spi(int *spi_fd) {
    *spi_fd = open(SPI_DEVICE, O_RDWR);
    if (*spi_fd < 0) {
        perror("Failed to open SPI device");
        return -1;
    }

    if (ioctl(*spi_fd, SPI_IOC_WR_MODE, &(uint8_t){SPI_MODE}) == -1 ||
        ioctl(*spi_fd, SPI_IOC_WR_BITS_PER_WORD, &(uint8_t){SPI_BITS}) == -1 ||
        ioctl(*spi_fd, SPI_IOC_WR_MAX_SPEED_HZ, &(uint32_t){SPI_SPEED}) == -1) {
        perror("Failed to configure SPI");
        close(*spi_fd);
        return -1;
    }

    return 0;
}

// MCP3008에서 데이터 읽기
int read_mcp3008(int spi_fd, uint8_t channel) {
    uint8_t tx[] = {1, (8 + channel) << 4, 0};
    uint8_t rx[3] = {0};

    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)tx,
        .rx_buf = (unsigned long)rx,
        .len = sizeof(tx),
        .delay_usecs = SPI_DELAY,
        .speed_hz = SPI_SPEED,
        .bits_per_word = SPI_BITS,
    };

    if (ioctl(spi_fd, SPI_IOC_MESSAGE(1), &tr) == -1) {
        perror("SPI transfer failed");
        return -1;
    }

    // 데이터 변환 (10비트 값 반환)
    return ((rx[1] & 0x03) << 8) | rx[2];
}

int main() {
    int on_fire = 0;//집 화재 발생 유무 
    // GPIO gas pin 17 and pout 20
    Gas_on:
      if (GPIOExport(POUT2) == -1 || GPIOExport(PIN) == -1) {
          goto Gas_on;
      }
      if (GPIODirection(POUT2, OUT) == -1 || GPIODirection(PIN, IN) == -1) {
          goto Gas_on;
      }
    
    // GPIO set motor_pin motor_pin 21
    Motor_on:
      if (GPIOExport(MOTOR_GPIO_PIN) == -1) {
          goto Motor_on;
      }

      if (GPIODirection(MOTOR_GPIO_PIN, OUT) == -1) {
          goto Motor_on;
      }

    //GPIO set ir_sensor pin 27
    Ir_on:
      if (GPIOExport(IR_SENSOR_PIN) == -1) {
          goto Ir_on;
      }

      if (GPIODirection(IR_SENSOR_PIN, IN) == -1) {
          goto Ir_on;
      }

    // PWM servo motor pin 18
    Pwm_on:
      if(PWMExport(PWM) == -1 || PWMWritePeriod(PWM, 10000000) == -1 || PWMWriteDutyCycle(PWM, 0) == -1 || PWMEnable(PWM) == -1){
        goto Pwm_on;
      }

    // SPI 초기화
    int spi_fd;
    Spi_on:
      if (init_spi(&spi_fd) == -1) {
          goto Spi_on;
      }
    
    //GPIO motor_on and off
    /*
    printf("Turning motor ON\n");
    GPIOWrite(MOTOR_GPIO_PIN, 1);
    sleep(3);

    printf("Turning motor OFF\n");
    GPIOWrite(MOTOR_GPIO_PIN, 0);

    GPIOUnexport(MOTOR_GPIO_PIN);
    */
    
    // check ir_sensor for something exist
    /*
    while (1) {
        int value = GPIORead(IR_SENSOR_PIN);
        if (value == -1) {
            return -1;
        }

        if (value == 0) {
            printf("Object detected!\n");
        } else {
            printf("No object detected.\n");
        }

        usleep(500000); 
    }
    */

    // 서보모터 움직임 제어
    /*
    PWMWriteDutyCycle(PWM, 1000000); // 0도
    printf("Servo at 0 degrees.\n");
    sleep(2);

    PWMWriteDutyCycle(PWM, 1500000); // 90도
    printf("Servo at 90 degrees.\n");
    sleep(2);

    PWMWriteDutyCycle(PWM, 2000000); // 180도
    printf("Servo at 180 degrees.\n");
    sleep(2);
    */

    // gas and fire variance for check
    int fire_gas_value = 150;
    int fire_flame_value = 0;
    int fire_count = 0;
    int in_fire = 1;
    int pre_value = 2;
    // rredis 주소 변수
    redisContext *c;  // Redis connection context
    redisReply *reply;

    // Redis server details
    const char *redis_host = "";  // Redis server IP
    int redis_port = ;                     // Redis server port
    c = redisConnect(redis_host, redis_port);
    // try connect server 100 try 0.1seconds
    for(int i=0;i<100;i++){
      if(c == NULL || c->err){
        usleep(100000);
        continue;
      }
      else{
        break;
      }
    }

    // check_value and 
    while (1) {
        int value = GPIORead(IR_SENSOR_PIN);

        if (GPIOWrite(POUT2, 1) == -1 || value == -1) {
            continue;
        } 
        int flame_value = GPIORead(PIN);
        // 가스 센서 읽기 (MCP3008, 채널 0 사용)
        int gas_value = read_mcp3008(spi_fd, 0);
        if (gas_value == -1) {
            continue;
        }

        // 결과 출력
        printf("불꽃 센서: %s, 가스 센서 값: %d\n", flame_value ? "감지 안 됨" : "감지됨 ", gas_value);

        if(fire_flame_value == flame_value && fire_gas_value <= gas_value){
            fire_count++;
        }
        //2초 동안 불 감지 
        if(fire_count >= 4){
            printf("불 나는 중 %d, %d\n",fire_count, in_fire);

            //적외선 감지 -> 자리 비움
            if(value == 0){
                printf("find_people\n");
            }
            else{
              fire_count -= 1;
            }
            if(in_fire == 1 && value == 0){
                in_fire = 0;
            }
            else if(in_fire == 0 && value == 0 && pre_value != value){//직전 적외선 값이랑 비교함으로써 오차 범위 줄임 
                in_fire = 1;
                fire_count = 0;
            }
        }
        //자리를 비우고 1분 이상 돌아오지 않음 -> 1초동안 진동 모터 
        if(fire_count >= 124 && in_fire == 0){
            printf("Turning motor ON\n");
            GPIOWrite(MOTOR_GPIO_PIN, 1);
            sleep(1);

            printf("Turning motor OFF\n");
            GPIOWrite(MOTOR_GPIO_PIN, 0);
        }
        //알람을 울리고 30초 이상 돌아오지 않음 -> 모터 90도 회전
        if(fire_count >= 144 && in_fire == 0){
            PWMWriteDutyCycle(PWM, 1500000);// set motor degree 90
            sleep(1);
            in_fire = 1;//진동모터 종료
            PWMWriteDutyCycle(PWM, 1000000);// set motor degree 0
        }
        // 모터로 벨브를 잠그고 10초 후에도 여전히 불을 인식하면 화재로 처리
        if(fire_count >= 164){
            on_fire = 1; //집 화재발생을 유무를 참으로 설정
            const char *channel_name = "test";
            const char *message = "OnFire adress";
            reply = redisCommand(c, "PUBLISH %s %s", channel_name, message);
            break;
        }
        usleep(500000); // 0.5초 대기
        pre_value = value;
    }
    if (GPIOUnexport(POUT2) == -1 || GPIOUnexport(PIN) == -1) {
        close(spi_fd);
        return 4;
    }
    // 리소스 정리
    GPIOUnexport(MOTOR_GPIO_PIN);
    close(spi_fd);
    freeReplyObject(reply);
    redisFree(c);
    return 0;
}
