#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <linux/spi/spidev.h>
#include <sys/ioctl.h>
#include <string.h>
#include <hiredis/hiredis.h>
#include <time.h>
#include <sys/time.h>
#include <time.h>

#define GPIO_PATH "/sys/class/gpio"
#define PIR_GPIO "4"  
#define IR_GPIO "21"
#define TRIG_GPIO "23"  
#define ECHO_GPIO "24"
#define SPI_DEVICE "/dev/spidev0.0"

#define FILTER_SIZE 5

const char *REDIS_HOST = "54.79.123.103";  // Redis server IP
int REDIS_PORT = 6379; 

volatile int pir_state = 0;
volatile int ir_state = 0;
volatile float ultrasonic_distance = 100.0;

pthread_mutex_t pir_lock;
pthread_mutex_t ir_lock;
pthread_mutex_t ultrasonic_lock;

int pir_buffer[FILTER_SIZE] = {0}, pir_index = 0;
int ir_buffer[FILTER_SIZE] = {0}, ir_index = 0;

void save_time_to_file(const char *filename, const char *event) {
    FILE *file = fopen(filename, "a"); // Append mode to save multiple events
    if (file == NULL) {
        perror("Failed to open file for saving time");
        return;
    }

    time_t now = time(NULL);
    struct tm *time_info = localtime(&now);
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", time_info);

    fprintf(file, "%s: %s\n", event, time_str);
    fclose(file);

    printf("Saved event '%s' at time: %s\n", event, time_str);
}

int publish_to_redis(const char *channel, const char *message) {
    redisContext *c = redisConnect(REDIS_HOST, REDIS_PORT);

    // Check connection
    if (c == NULL || c->err) {
        if (c) {
            printf("Connection error: %s\n", c->errstr);
            redisFree(c);
        } else {
            printf("Connection error: can't allocate redis context\n");
        }
        exit(1);
    }
    
     printf("Connected to Redis server at %s:%d\n", REDIS_HOST, REDIS_PORT);

    // Publish message to Redis
    redisReply *reply = redisCommand(c, "PUBLISH %s %s", channel, message);
    
    if (reply == NULL) {
        printf("Publish failed. Check Redis connection or server status.\n");
        redisFree(c);
        return 1;  // Exit on failure
    }
    

    // Print result
    printf("Published to %s: %lld subscribers received the message.\n", 
           channel, reply->integer);

    // Clean up
    freeReplyObject(reply);
    redisFree(c);

    return 0;
}


void gpio_export(const char *gpio_num) {
    int fd = open(GPIO_PATH "/export", O_WRONLY);
    if (fd == -1) {
        perror("Failed to export GPIO");
        exit(1);
    }

    if (write(fd, gpio_num, strlen(gpio_num)) != strlen(gpio_num)) {
        perror("Failed to write GPIO number to export");
        close(fd);
        exit(1);
    }
    close(fd);
}

void gpio_unexport(const char *gpio_num) {
    int fd = open(GPIO_PATH "/unexport", O_WRONLY);
    if (fd == -1) {
        perror("Failed to unexport GPIO");
        return;
    }

    if (write(fd, gpio_num, strlen(gpio_num)) != strlen(gpio_num)) {
        perror("Failed to write GPIO number to unexport");
    }
    close(fd);
}


void gpio_set_direction(const char *gpio_num, const char *direction) {
    
    char path[128];
    snprintf(path, sizeof(path), GPIO_PATH "/gpio%s/direction", gpio_num);
    int fd = open(path, O_WRONLY);
    if (fd == -1) {
        perror("Failed to set GPIO direction");
        exit(1);
    }

    if (write(fd, direction, strlen(direction)) != strlen(direction)) {
        perror("Failed to write GPIO direction");
        close(fd);
        exit(1);
    }
    close(fd);
}

void gpio_write(const char *gpio_num, const char *value) {
    char path[128];
    snprintf(path, sizeof(path), GPIO_PATH "/gpio%s/value", gpio_num);
    int fd = open(path, O_WRONLY);
    if (fd == -1) {
        perror("Failed to write GPIO value");
        exit(1);
    }

    if (write(fd, value, strlen(value)) != strlen(value)) {
        perror("Failed to write value to GPIO");
        close(fd);
        exit(1);
    }
    close(fd);
}

int gpio_read(const char *gpio_num) {
    char path[128];
    char value;
    snprintf(path, sizeof(path), GPIO_PATH "/gpio%s/value", gpio_num);
    int fd = open(path, O_RDONLY);
    if (fd == -1) {
        perror("Failed to read GPIO value");
        exit(1);
    }
    if (read(fd, &value, 1) != 1) {
        perror("Failed to read value from GPIO");
        close(fd);
        exit(1);
    }
    close(fd);
    return (value == '1') ? 1 : 0;
}

int gpio_read_weighted_filter(const char *gpio_num, int *buffer, int *index) {
    static int weights[FILTER_SIZE] = {1, 2, 3, 4, 5};
    int current_value = gpio_read(gpio_num);

    buffer[*index] = current_value;
    *index = (*index + 1) % FILTER_SIZE;

    int weighted_sum = 0;
    int weight_total = 0;
    for (int i = 0; i < FILTER_SIZE; i++) {
        weighted_sum += buffer[i] * weights[i];
        weight_total += weights[i];
    }

    return (weighted_sum >= weight_total / 2) ? 1 : 0;
}


void *ultrasonic_sensor_thread(void *arg) {
    while (1) {
        struct timeval start, end;
        gpio_write(TRIG_GPIO, "1");
        usleep(10);  // 10 microseconds pulse
        gpio_write(TRIG_GPIO, "0");

        // Wait for ECHO signal to go HIGH
        while (gpio_read(ECHO_GPIO) == 0);
        gettimeofday(&start, NULL);

        // Wait for ECHO signal to go LOW
        while (gpio_read(ECHO_GPIO) == 1);
        gettimeofday(&end, NULL);

        // Calculate distance
        long time_diff = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec);
        float distance = time_diff * 0.034 / 2;

        pthread_mutex_lock(&ultrasonic_lock);
        ultrasonic_distance = distance;
        pthread_mutex_unlock(&ultrasonic_lock);

        usleep(100000);  // 100ms
    }
    return NULL;
}


void *pir_sensor_thread(void *arg){
    while(1) {
        int value = gpio_read_weighted_filter(PIR_GPIO , pir_buffer , &pir_index);
        pthread_mutex_lock(&pir_lock);
        pir_state = value;
        pthread_mutex_unlock(&pir_lock);
        usleep(50000);
    }
    return NULL;
}


void *ir_sensor_thread(void *arg) {
    while (1) {
        int value = gpio_read_weighted_filter(IR_GPIO,ir_buffer,&ir_index);
        pthread_mutex_lock(&ir_lock);
        ir_state = value;
        pthread_mutex_unlock(&ir_lock);
        usleep(50000); 
    }
    return NULL;
}

int read_adc(int fd, int channel) {
    uint8_t tx[3] = {1, (8 + channel) << 4, 0};
    uint8_t rx[3] = {0};

    struct spi_ioc_transfer transfer = {
        .tx_buf = (unsigned long)tx,
        .rx_buf = (unsigned long)rx,
        .len = 3,
        .speed_hz = 1350000,
        .bits_per_word = 8,
    };

    if (ioctl(fd, SPI_IOC_MESSAGE(1), &transfer) < 0) {
        perror("SPI transfer failed");
        return -1;
    }

    return ((rx[1] & 3) << 8) + rx[2];
}

int main() {
    pthread_t pir_thread, ir_thread, ultrasonic_thread;
    int fd, channel = 5;
    

    gpio_export(PIR_GPIO);
    gpio_set_direction(PIR_GPIO, "in");
    gpio_export(IR_GPIO);
    gpio_set_direction(IR_GPIO, "in");
    gpio_export(TRIG_GPIO);
    gpio_set_direction(TRIG_GPIO, "out");
    gpio_export(ECHO_GPIO);
    gpio_set_direction(ECHO_GPIO, "in");

    if ((fd = open(SPI_DEVICE, O_RDWR)) < 0) {
        perror("Failed to open SPI device");
        return 1;
    }

    uint8_t mode = 0;
    if (ioctl(fd, SPI_IOC_WR_MODE, &mode) < 0) {
        perror("Failed to set SPI mode");
        close(fd);
        return 1;
    }
    int speed = 1350000;
    if (ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0) {
        perror("Failed to set SPI speed");
        close(fd);
        return 1;
    }

    pthread_mutex_init(&pir_lock, NULL);
    pthread_mutex_init(&ir_lock, NULL);
    pthread_mutex_init(&ultrasonic_lock, NULL);

    if (pthread_create(&pir_thread, NULL, pir_sensor_thread, NULL) != 0) {
        perror("Failed to create PIR sensor thread");
        return 1;
    }
    if (pthread_create(&ir_thread, NULL, ir_sensor_thread, NULL) != 0) {
        perror("Failed to create IR sensor thread");
        return 1;
    }
    if (pthread_create(&ultrasonic_thread, NULL, ultrasonic_sensor_thread, NULL) != 0) {
        perror("Failed to create Ultrasonic sensor thread");
        return 1;
    }

    while (1) {
        int pir_copy, ir_copy;
        float distance;

        pthread_mutex_lock(&pir_lock);
        pir_copy = pir_state;
        pthread_mutex_unlock(&pir_lock);


        if (pir_copy == 1) {
            printf("now checking if grandma is on bed\n");
            
            time_t start_time = time(NULL);
            int ir_triggered = 1;
            float trigger_distance = 1000;
            
            
            while(difftime(time(NULL) , start_time) < 10){
                pthread_mutex_lock(&ir_lock);
                int ir_copy = ir_state;
                pthread_mutex_unlock(&ir_lock);
                
                pthread_mutex_lock(&ultrasonic_lock);
                float distance = ultrasonic_distance;
                pthread_mutex_unlock(&ultrasonic_lock);
                
                if(ir_copy == 0 && distance <= 20.0){
                    ir_triggered = 0;
                    trigger_distance = distance;
            
                    printf("Trigger condition met: Grandma is going to sleep\n");
                    save_time_to_file("grandma_sleep_log.txt", "Grandma went to sleep");


                    time_t start_time = time(NULL);
                    while (difftime(time(NULL), start_time) < 5) {
                        int value = read_adc(fd, channel);
                        float voltage = value * 3.3 / 1023;

                        printf("Vibration sensor - CH%d: Value=%d, Voltage=%.2fV\n", channel, value, voltage);

                        if (value > 300) {
                            char message[256];
                            snprintf(message, sizeof(message), "falling 16499");
                            publish_to_redis("test", message);
                                
                        }
                        usleep(500000);  // 0.5s
                    }
                    break;
                }
                usleep(500000);
            }
            
            if(ir_triggered || trigger_distance > 20.0){
                continue;
            }


            while (1) {
                pthread_mutex_lock(&pir_lock);
                pir_copy = pir_state;
                pthread_mutex_unlock(&pir_lock);

                pthread_mutex_lock(&ir_lock);
                ir_copy = ir_state;
                pthread_mutex_unlock(&ir_lock);

                pthread_mutex_lock(&ultrasonic_lock);
                distance = ultrasonic_distance;
                pthread_mutex_unlock(&ultrasonic_lock);

                printf("Monitoring - PIR: %d, IR: %d, Distance: %.2f cm\n", pir_copy, ir_copy, distance);

                //get out of bed 
                if (pir_copy == 1 && ir_copy == 1 && distance > 20.0){
                    time_t now = time(NULL);
                    struct tm *local_time = localtime(&now);
                        
                    int hour = local_time -> tm_hour;
                        
                    if(hour >= 0 && hour < 8){
                        printf("It is between 12 AM and 8 AM. Grandma maybe not wake up yet.\n");
                        save_time_to_file("grandma_sleep_log.txt" , "detected move at night");
                    }else{
                        printf("Condition reset: Grandma wakes up\n");
                        save_time_to_file("grandma_sleep_log.txt", "Grandma woke up");
                        usleep(5000000);
                        break;
                    }
                }
                int value = read_adc(fd, channel);
                float voltage = value * 3.3 / 1023;

                printf("Vibration sensor - CH%d: Value=%d, Voltage=%.2fV\n", channel, value, voltage);

                if (value > 300) {
                    char message[256];
                    snprintf(message, sizeof(message), "falling 16499");
                    publish_to_redis("test", message);
                }
                usleep(500000);
            }
        } else {
            printf("No motion or object detected.\n");
        }

        usleep(500000);  // 0.5s
    }
    

    pthread_mutex_destroy(&pir_lock);
    pthread_mutex_destroy(&ir_lock);
    pthread_mutex_destroy(&ultrasonic_lock);
    
    
    gpio_unexport(PIR_GPIO);
    gpio_unexport(IR_GPIO);
    gpio_unexport(TRIG_GPIO);
    gpio_unexport(ECHO_GPIO);
    
    close(fd);
    return 0;
}
