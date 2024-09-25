#if CONFIG_FREERTOS_UNICORE
  static const BaseType_t app_cpu = 0;
#else 
  static const BaseType_t app_cpu = 1;
#endif

#define BUFFER_SIZE 10

hw_timer_t *timer = NULL;
static volatile uint16_t val;
static SemaphoreHandle_t bin_sem = NULL;
volatile int buffer[BUFFER_SIZE]; // Buffer to store 10 values
volatile int head = 0;            // Head pointer for the buffer
volatile int i = 0;               // Initialize index
volatile int data;
volatile float average = 0;       // Initialize average
char receivedData[100];

static portMUX_TYPE spinlock = portMUX_INITIALIZER_UNLOCKED;

void IRAM_ATTR onTimer() 
{
    portENTER_CRITICAL_ISR(&spinlock);
    buffer[head] = random(1, 1001); // Random number from 1 to 1000
    head = (head + 1) % BUFFER_SIZE; // Move the head to the next position in the buffer
    
    // Check if the buffer is full
    if (head == 0) {
        xSemaphoreGiveFromISR(bin_sem, NULL);
    }
    portEXIT_CRITICAL_ISR(&spinlock);
}

void task1(void *parameters)
{
    while(1)
    {
        xSemaphoreTake(bin_sem, portMAX_DELAY);
        portENTER_CRITICAL(&spinlock);
        
        // Calculate the average of the values in the buffer
        data = 0;
        for (int j = 0; j < BUFFER_SIZE; j++)
        {
            data += buffer[j];
        }
        
        average = (float)data / BUFFER_SIZE; // Compute average
        portEXIT_CRITICAL(&spinlock);
        
        //Serial.print("Average calculated: ");
        //Serial.println(average); // Print average for verification
        vTaskDelay(100 / portTICK_PERIOD_MS); // Task delay
    }
}

void task2(void *parameters)
{
    while (1)
    {
        while (Serial.available() > 0) 
        {
            char c = Serial.read();
            if (c == '\n' || c == '\r') 
            {
                if (i > 0) {
                    receivedData[i] = '\0'; // Null-terminate the string
                    Serial.print("Received Input: ");
                    Serial.println(receivedData);
                    
                    if (strcmp(receivedData, "avg") == 0) {
                        Serial.print("AVERAGE : ");
                        Serial.println(average);
                    }
                    
                    i = 0; // Reset index for next input
                    memset(receivedData, 0, sizeof(receivedData));
                }
            } 
            else 
            {
                if (i < sizeof(receivedData) - 1) 
                {
                    receivedData[i++] = c;
                }
            }
        }
        vTaskDelay(100 / portTICK_PERIOD_MS); // Task delay
    }
}

void setup() {
    Serial.begin(115200);
    randomSeed(analogRead(0));
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    Serial.println();

    bin_sem = xSemaphoreCreateBinary();

    if (bin_sem == NULL)
    {
        Serial.println("COULD NOT CREATE SEMAPHORE");
        ESP.restart();
    }

    xTaskCreatePinnedToCore(
        task1,
        "Calculate Average",
        2048,
        NULL,
        1,
        NULL,
        app_cpu
    );

    xTaskCreatePinnedToCore(
        task2,
        "Echo Input",
        2048,
        NULL,
        1,
        NULL,
        app_cpu
    );

    // Initialize hardware timer with frequency (1 MHz -> 1 tick = 1 microsecond)
    timer = timerBegin(1000000);  // Set frequency to 1 MHz

    // Attach the timer interrupt handler
    timerAttachInterrupt(timer, &onTimer);

    // Set the alarm to trigger every 1 second (1000000 microseconds) with auto-reload enabled
    timerAlarm(timer, 100000, true, 0); // Change the period to 1 second

    // Start the timer
    timerStart(timer);
}

void loop() {
    // Main loop can remain empty as everything is handled by the timer interrupt
}
