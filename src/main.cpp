#include <Arduino.h>
#include <string.h>
#include "ESP8266WiFi.h"
#include "ESP8266HTTPClient.h"
#include "PubSubClient.h"
#include "kxtj3-1057.h"
#include "I2CKeyPad.h"
#include "Adafruit_GFX.h"
#include "Adafruit_ST7735.h"
#include "SoftwareSerial.h"

#define ACCELERATION 4
#define MAX_BUFFER_SIZE 50
#define BURD_RATE 9600
#define TFT_DC 15
#define TFT_CS 0
#define TFT_RST -1

const char *HOST = "101.200.78.91";
const uint16_t PORT = 7000;
const char *mqtt_server = "broker-cn.emqx.io";
const String login_server = "http://10.3.8.216/login";
const uint8_t KEYPAD_ADDRESS = 0x38;
const uint8_t COUNTER_ADDRESS = 0x0E;
const char *keys = "EMC154329876#0*NF";
const char alphabet[9][3] = {
    {'a', 'b', 'c'},
    {'d', 'e', 'f'},
    {'g', 'h', 'i'},
    {'j', 'k', 'l'},
    {'m', 'n', 'o'},
    {'p', 'q', 'r'},
    {'s', 't', 'u'},
    {'v', 'w', 'x'},
    {'y', 'z'}};
//代码中需要用到的常量

WiFiClient tcpClient;
PubSubClient mqttClient(tcpClient);
HTTPClient httpClient;
KXTJ3 IMU(COUNTER_ADDRESS);
I2CKeyPad keypad(KEYPAD_ADDRESS);
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);
SoftwareSerial swSer(0, 2, false);
//初始化相应的对象

//代码中的全局变量
//指示主界面是否需要更新的全局变量
bool flag = true;
//全局计时器
uint32_t timer;
uint8_t hour = 0;
uint8_t minute = 0;
uint8_t second = 0;
//记录步数的全局变量
/*
这里有个问题
由于整个程序时会在不同的循环中反复横跳
为了能有一个全局的步数记录
我在所有的循环中都添加了测量步数的函数
*/
int walkCount = 0;
//按键函数需要用到的变量
char lastPressedKey = 'F';
int pressedCounter = 0;
//mqtt模块需要用到的变量
unsigned long lastMsg = 0;
char msg[MAX_BUFFER_SIZE];
int value = 0;
String clientId = "ESP8266Client-";
//连接WiFi需要用到的常量
String ssid = "";
char pass[20] = {'\0'};
//存储键盘输入的值
char input = 'F';
String wifi_scan_array[6];

//代码中涉及的函数原型
char inputLetter(void);
void callback(char *topic, byte *payload, int length);
void reconnect(void);
void cleanScreen(void);
void printText(String text, int x, int y);
bool scan_wifi();
int connect_wifi(const char *ssid, const char *pass);
int login_school_network();
void counter_setup(void);
int counter_run(void);
void keyPadSetUp(void);
char getKeyPressed(void);
uint32_t get_time(void);
void mainMenu(void);
void wifiMenu(void);
void walkCounter(void);
void air202(void);

void setup()
{
    //初始化串口
    Serial.begin(115200);
    swSer.begin(9600);
    //初始化屏幕
    tft.initR(INITR_MINI160x80);
    tft.setRotation(3);
    tft.fillScreen(ST7735_BLACK);
    printText("Screen Initilized Success", 0, 5);
    delay(1000);
    cleanScreen();
    //初始化键盘
    keyPadSetUp();
    //初始化加速度计
    counter_setup();
    printText("Keypad Initilized Success", 0, 5);
    delay(1000);
    cleanScreen();
}

void loop()
{
    if (!flag)
    {
        printText("Walk Counter", 40, 20);
        flag = false;
    }
    int input = getKeyPressed();
    switch (input)
    {
    case 'M':
        mainMenu();
        break;
    case '6':
        walkCounter();
        break;
    case '4':
        get_time();
        break;
    case '2':
        air202();
        break;
    default:
        break;
    }
    cleanScreen();
    counter_run();
    delay(100);
}

//以下是系统的界面函数

//展示系统的主界面
void mainMenu(void)
{
    int input = 'F';
    bool flag = false;

    cleanScreen();

    while (input == 'F')
    {
        if (!flag)
        {
            printText("Main Menu", 30, 5);
            printText("1-WiFi Config", 0, 15);
            printText("2-Config", 0, 25);
            printText("Press # to quit", 0, 55);
            flag = true;
        }
        input = getKeyPressed();
        switch (input)
        {
        case '1':
            wifiMenu();
            flag = false;
            break;
        case '2':
            break;
        case '#':
            return;
            break;
        }
        counter_run();
        delay(100);
        input = 'F';
    }
}

void wifiMenu(void)
{
    int input = 'F';

    cleanScreen();

    bool result = scan_wifi();
    if (result)
    {
        for (int i = 0; i < 6; i++)
        {
            printText(wifi_scan_array[i], 0, 5 + 10 * i);
        }
    }
    else
    {
        printText("No WiFi Scanned!", 5, 5);
    }
    while (input == 'F')
    {
        input = getKeyPressed();
        //串口调试
        //Serial.println(input);
        if (input >= '1' && input <= '6')
        {
            cleanScreen();
            printText("Connect to " + wifi_scan_array[input - 49], 0, 5);
            printText("Enter the password", 0, 15);
            printText("Press Comfire to contiure", 0, 25);
            printText("Press # to delete", 0, 35);
            ssid = wifi_scan_array[input - 49];
            delay(1000);
            cleanScreen();
            input = 'F';
            char *p = pass;
            while (input == 'F')
            {
                input = getKeyPressed();
                //串口调试
                //Serial.println(input);
                //Serial.println(pass);
                /*
                这里似乎有一个bug
                如果WiFi的位置在列表的最开始
                在输密码时就会多出一个1
                */
                if (input == 'C')
                {
                    break;
                }
                if (input != 'F' && input != '#')
                {
                    char temp = input;
                    tft.setCursor(5 * strlen(pass), 5);
                    tft.print(temp);
                    *p = temp;
                    *(p + 1) = '\0';
                    p++;
                }
                if (input == '#')
                {
                    cleanScreen();
                    pass[strlen(pass) - 1] = '\0';
                    p--;
                    for (uint8_t i = 0; i < strlen(pass); i++)
                    {
                        tft.setCursor(5 * i, 5);
                        tft.print(pass[i]);
                    }
                    Serial.print(pass);
                }
                input = 'F';
                delay(100);
            }
            cleanScreen();
            //调试用
            //Serial.println(pass);
            int result = connect_wifi(ssid.c_str(), pass);
            if (result == 0)
            {
                printText("Connect success", 0, 5);
                delay(500);
                cleanScreen();
            }
            else
            {
                printText("Connection failed", 0, 5);
                delay(1000);
                cleanScreen();
            }
            input = 'F';
            return;
            //记得将输出复位为F
        }
        if (input == '#')
        {
            input = 'F';
            cleanScreen();
            return;
        }
        counter_run();
        delay(100);
    }
}

//显示计步器界面的函数
void walkCounter(void)
{
    int input = 'F';
    //表示当前已经行走的步数的变量
    cleanScreen();
    while (input == 'F')
    {
        cleanScreen();
        printText("The value of the pedometer is", 0, 5);
        input = getKeyPressed();
        if (input == '#')
        {
            input = 'F';
            break;
        }
        tft.setCursor(40, 20);
        tft.setTextSize(2);
        tft.print(walkCount);
        counter_run();
        delay(100);
    }
}

//输入英文字母的函数
char inputLetter(void)
{
    char input = 'F';
    bool flag = true;
    int last_input = 'F';
    char result;

    cleanScreen();
    while (input == 'F')
    {
        input = getKeyPressed();
        if ('1' <= input && input <= '9')
        {
            if (flag)
            {
                cleanScreen();
                for (int i = 0; i < 3; i++)
                {
                    String temp = "#";
                    temp = temp + alphabet[input - 49][i];
                    printText(temp, 5, 10 * (i + 1));
                }
                flag = false;
                last_input = input;
            }
            else
            {
                if ('1' <= input && input <= '3')
                {
                    cleanScreen();
                    result = alphabet[last_input - 49][input - 49];
                    flag = true;
                    break;
                }
            }
        }
        counter_run();
        delay(100);
        input = 'F';
    }
    return result;
}

//air202的控制界面
void air202(void)
{
    //跳转到一个新界面，先清屏
    cleanScreen();
    
    char input = 'F';
    //标记当前是在发送信息还是接受信息
    bool flag = true;
    char message[20] ={'\0'};
    char *p = message;
    String result;

    printText("Are you sure to connect Air202?", 0, 5);
    printText("Confirm or quit", 0, 20);
    while(input == 'F')
    {
        input = getKeyPressed();
        if(input == 'C'){
            break;
        }
        if(input == '#'){
            return;
        }
        input = 'F';
        delay(100);
    }
    input = 'F';
    cleanScreen();
    while(input == 'F'){
        input = getKeyPressed();
        if(flag == true){
            cleanScreen();
            printText("Enter the message", 0, 5);
            for(uint8_t i = 0; i < strlen(message); i++){
                tft.setCursor(5 * i, 20);
                tft.print(message[i]);
            }
            if(input == 'M'){
                break;
            }
            switch (input)
            {
            case '*':
            {
                char temp = inputLetter();
                Serial.println(temp);
                *p = temp;
                *(p + 1) = '\0';
                p++;
                cleanScreen();
                break;
            }
            case '#':
            {
                message[strlen(message) - 1] = '\0';
                p--;
                break;
            }
                
            case 'C':
            {
                swSer.println(message);
                flag = false;
                break;
            }
            }
        }else{
            bool wait = true;
            while(swSer.available() > 0)
            {
                result = result + (char)swSer.read();
                if(swSer.available() == 0){
                    wait = false;
                }
            }
            if(!wait)
            {
                cleanScreen();
                printText(result, 0, 5);
                message[0] = '\0';
                delay(2000);
                flag = true;
            }
        }
        delay(100);
        input = 'F';
    }
}

//以下是系统的工具函数

//mqtt模块收到消息的回调函数
void callback(char *topic, byte *payload, int length)
{
    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("] ");
    for (int i = 0; i < length; i++)
    {
        Serial.print((char)payload[i]);
    }
    Serial.println();
}

//mqtt模块重新链接函数
void reconnect(void)
{
    // Loop until we're reconnected
    while (!mqttClient.connected())
    {
        Serial.print("Attempting MQTT connection...");
        // Create a random client ID

        // Attempt to connect
        if (mqttClient.connect(clientId.c_str()))
        {
            Serial.println("connected");
            // Once connected, publish an announcement...
            mqttClient.publish("/vsop/bupt", "hello world");
            // ... and resubscribe
            mqttClient.subscribe("inTopic");
        }
        else
        {
            Serial.print("failed, rc=");
            Serial.print(mqttClient.state());
            Serial.println(" try again in 5 seconds");
            // Wait 5 seconds before retrying
            delay(5000);
        }
    }
}

//清除屏幕
void cleanScreen(void)
{
    tft.fillScreen(ST7735_BLACK);
}

//在屏幕上打印文字
void printText(String text, int x, int y)
{
    tft.setTextColor(ST7735_WHITE);
    tft.setTextSize(1.2);
    tft.setTextWrap(true);
    tft.setCursor(x, y);
    tft.print(text.c_str());
}

//连接WiFi
int connect_wifi(const char *ssid, const char *pass)
{
    int time = 0;
    bool conn = true;

    WiFi.mode(WIFI_STA);
    //设置WiFi的模式
    WiFi.begin(ssid, pass);
    //连接指定的WiFi

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        time = time + 500;
        if (time >= 15000)
        {
            conn = false;
            break;
        }
    }
    if (conn)
    {
        return 0;
    }
    else
    {
        return -1;
    }
}

//登录校园网
//该段函数经验证无法正常在校园网环境下正常工作
//目前我实现在校园网环境下的自动登录都需要依赖浏览器
//暂时我还没有可以摆脱掉浏览器的方法
int login_school_network()
{
    httpClient.begin(tcpClient, login_server);
    String post_content = "user=$username&pass=$password&line=";
    int httpCode = httpClient.POST(post_content);
    //如果返回post请求成功的结果，则函数返回0，否则返回http状态码
    if (httpCode == 200)
    {
        return 0;
    }
    else
    {
        return httpCode;
    }
}

//扫描WiFi
bool scan_wifi()
{
    //扫描结果
    int scanResult;
    //WiFi的一些参数
    int32_t rssi;
    uint8_t encryptionType;
    uint8_t *bssid;
    int32_t channel;
    bool hidden;
    //在扫描可以连接的WiFi之前先断开现有的WiFi连接
    WiFi.disconnect();
    delay(100);
    scanResult = WiFi.scanNetworks(false, true);
    if (scanResult == 0)
    {
        return false;
    }
    else
    {
        if (scanResult <= 6)
        {
            for (int i = 0; i < scanResult; i++)
            {
                WiFi.getNetworkInfo(i, wifi_scan_array[i], encryptionType, rssi, bssid, channel, hidden);
            }
        }
        else
        {
            for (int i = 0; i < 6; i++)
            {
                WiFi.getNetworkInfo(i, wifi_scan_array[i], encryptionType, rssi, bssid, channel, hidden);
            }
        }
        return true;
    }
}

//计步器模块初始化
void counter_setup(void)
{
    float sampleRate = 12.5;
    //采样的速度
    uint8_t accelRange = 4;
    //采样的准确范围
    if (IMU.begin(sampleRate, accelRange) == -1)
    {
        Serial.print("Failed to initialize!\n");
    }
    else
    {
        Serial.print("Initialized\n");
    }

    uint8_t readDate;
    IMU.readRegister(&readDate, KXTJ3_WHO_AM_I);
    Serial.print("Who am I ? 0x");
    Serial.println(readDate, HEX);
}

//计步器模块运行
/*
注意：
这个函数有问题
加速度计的返回值一直是一个常数
从串口的数据来看
芯片的初始化是成功的
回报的Who Am I 的值是0x35
*/
int counter_run(void)
{
    IMU.standby(false);
    //禁用传感器的休眠模式
    float g_x, g_y, g_z;
    g_x = IMU.axisAccel(X);
    g_y = IMU.axisAccel(Y);
    g_z = IMU.axisAccel(Z);
    float average = g_x * g_x + g_y * g_y + g_z * g_z;
    //调试用
    //Serial.println(average);
    if (average > ACCELERATION)
    {
        walkCount++;
    }
    IMU.standby(true);
    return walkCount;
}

//输入键盘的初始化
void keyPadSetUp(void)
{
    Wire.begin();
    Wire.setClock(400000);
    if (keypad.begin() == false)
    {
        Serial.println("按键模块初始化失败");
        while (true)
            ;
        //如果按键模块初始化失败，程序停止在这里
    }
}

//侦测按下的键盘
char getKeyPressed(void)
{
    char keyPressed = keys[keypad.getKey()];
    if (keyPressed != 'F')
    {
        if (keyPressed != lastPressedKey)
        {
            pressedCounter = 0;
            lastPressedKey = keyPressed;
            return keyPressed;
        }
        else
        {
            pressedCounter++;
            if (pressedCounter <= 10)
            {
                return 'F';
            }
            else
            {
                return keyPressed;
            }
        }
    }
    else
    {
        return keyPressed;
    }
}

//与互联网同步时间
//暂时测试失败
uint32_t get_time()
{
    printText("Connect to server...", 0, 5);
    if (true)
    {
        if (!tcpClient.connect(HOST, 7000))
        {
            cleanScreen();
            printText("Connect to server fialed", 0, 5);
            delay(1000);
            return 0;
        }
        String result = tcpClient.readStringUntil('\n');
        cleanScreen();
        printText(result, 0, 5);
        tcpClient.println("time");
        /* hour = (tcpClient.read() - 48) * 10;
        hour = hour + tcpClient.read() - 48;
        minute = (tcpClient.read() - 48) * 10;
        minute = minute + tcpClient.read() - 48;
        second = (tcpClient.read() - 48) * 10;
        second = second + tcpClient.read() - 48; */
        while (tcpClient.available() == 0)
        {
        }
        Serial.println(tcpClient.read());
        Serial.println(tcpClient.read());
        Serial.println(tcpClient.read());
        Serial.println(tcpClient.read());
        Serial.println(tcpClient.read());
        Serial.println(tcpClient.read());
    }
    return 0;
}
