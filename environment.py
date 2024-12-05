import RPi.GPIO as GPIO
import Adafruit_DHT
import threading
import time
import spidev
import redis

# DHT settings
TEMP_LED_PIN = 18
HUMID_RED_PIN = 22
HUMID_GREEN_PIN = 27
DHT_SENSOR = Adafruit_DHT.DHT11
DHT_PIN = 4

# dust settings
DUST_RED_PIN = 5
DUST_BLUE_PIN = 6
DUST_LED_PIN = 21
ADC_CHANNEL = 3  # MCP channel
spi = spidev.SpiDev()  # SPI create
spi.open(0, 0)       
spi.max_speed_hz = 1350000  # SPI speed

# Button settings
BUTTON_PIN1 = 20
BUTTON_PIN2 = 16

# GPIO initialize
GPIO.setwarnings(False)
GPIO.setmode(GPIO.BCM)
GPIO.setup(TEMP_LED_PIN, GPIO.OUT)
GPIO.setup(HUMID_RED_PIN, GPIO.OUT)
GPIO.setup(HUMID_GREEN_PIN, GPIO.OUT)
GPIO.setup(DUST_LED_PIN, GPIO.OUT)
GPIO.setup(DUST_RED_PIN, GPIO.OUT)
GPIO.setup(DUST_BLUE_PIN, GPIO.OUT)
GPIO.setup(BUTTON_PIN1, GPIO.IN, pull_up_down=GPIO.PUD_UP)  
GPIO.setup(BUTTON_PIN2, GPIO.IN, pull_up_down=GPIO.PUD_UP)

# Redis connection
REDIS_HOST = "54.79.123.103"
REDIS_PORT = 6379
CHANNEL_NAME = "test"

try:
    redis_client = redis.StrictRedis(host=REDIS_HOST, port=REDIS_PORT, decode_responses=True)
    print(f"Connected to Redis server at {REDIS_HOST}:{REDIS_PORT}")
except Exception as e:
    print(f"Failed to connect to Redis server: {e}")
    redis_client = None

# global variable
global humidity, temperature, dust_data
humidity = None
temperature = None
dust_data = None


# Publish function
def publish_message(message):
    if redis_client:
        try:
            redis_client.publish(CHANNEL_NAME, message)
            print(f"Published: {message}")
        except Exception as e:
            print(f"Failed to publish message: {e}")

# Button handler
def handle_button_press(channel):
    publish_message("EMERGENCY home")

# Add event detection for buttons
GPIO.add_event_detect(BUTTON_PIN1, GPIO.FALLING, callback=handle_button_press, bouncetime=100)
GPIO.add_event_detect(BUTTON_PIN2, GPIO.FALLING, callback=handle_button_press, bouncetime=100)


# DHT sensor
def read_dht_sensor():
    global humidity, temperature
    while True:
        temp_sum = 0
        hum_sum = 0
        valid_readings = 0
        
        for _ in range(10):  # 1sec, 10times
            hum, temp = Adafruit_DHT.read(DHT_SENSOR, DHT_PIN)
            if hum is not None and temp is not None:
                hum_sum += hum
                temp_sum += temp
                valid_readings += 1
            time.sleep(0.1)
        
        if valid_readings > 0:
            humidity = hum_sum / valid_readings
            temperature = temp_sum / valid_readings
            print(f"[DHT] Temp: {temperature:.1f}°C, Humidity: {humidity:.1f}%")
            
            # temperature LED control
            if temperature >= 35:
                GPIO.output(TEMP_LED_PIN, GPIO.HIGH)
            else:
                GPIO.output(TEMP_LED_PIN, GPIO.LOW)
            
            # humidity LED control
            if humidity >= 70:
                GPIO.output(HUMID_RED_PIN, GPIO.HIGH)
                GPIO.output(HUMID_GREEN_PIN, GPIO.LOW)
            elif humidity >= 50:
                GPIO.output(HUMID_RED_PIN, GPIO.HIGH)
                GPIO.output(HUMID_GREEN_PIN, GPIO.HIGH)
            else:
                GPIO.output(HUMID_RED_PIN, GPIO.LOW)
                GPIO.output(HUMID_GREEN_PIN, GPIO.HIGH)
        else:
            print("[DHT] No valid readings received")
        
        time.sleep(1)

# Dust sensor
def read_dust_sensor():
    global dust_data
    while True:
        total_dust = 0
        valid_readings = 0
        
        for _ in range(10): # 1sec, 10times
            GPIO.output(DUST_LED_PIN, GPIO.LOW)
            time.sleep(0.00028)
        
            adc = spi.xfer2([1, (8 + ADC_CHANNEL) << 4, 0])
            adc_value = ((adc[1] & 3) << 8) + adc[2]
            time.sleep(0.00004)
            GPIO.output(DUST_LED_PIN, GPIO.HIGH)
            time.sleep(0.00968)
        
            calVoltage = adc_value * (5.0 / 1024.0)
            reading = abs((calVoltage - 0.5) * 170)
            
            if reading >= 0:
                total_dust += reading
                valid_readings += 1
                
            time.sleep(0.1)
            
        if valid_readings > 0:
            dust_data = total_dust / valid_readings
            print(f"[Dust] {dust_data:.2f} µg/m³  ")
        else:
            print("[Dust] No valid readings")
        
        # dust LED control
        if dust_data > 150: 
            GPIO.output(DUST_RED_PIN, GPIO.HIGH)
            GPIO.output(DUST_BLUE_PIN, GPIO.LOW)
        else:
            GPIO.output(DUST_RED_PIN, GPIO.LOW)
            GPIO.output(DUST_BLUE_PIN, GPIO.HIGH)
        
        time.sleep(1)

# used multi-threading
try:
    dht_thread = threading.Thread(target=read_dht_sensor)
    dust_thread = threading.Thread(target=read_dust_sensor)
    
    # start thread
    dht_thread.start()
    dust_thread.start()
    
    # main thread
    dht_thread.join()
    dust_thread.join()

except KeyboardInterrupt:
    print("\nSTOP!!!!!")

finally:
    GPIO.cleanup()
    spi.close()
