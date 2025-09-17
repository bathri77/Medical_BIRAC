/*
 * @Description: LilyGo Display Vibration Control (ESP32 AP + UDP Send after each point)
 * @Author: Modified for Diarc Application
 * @Date: 2025-09-17
 * @License: GPL 3.0
 */

#include <Arduino.h>
#include "Arduino_GFX_Library.h"
#include "pin_config.h"
#include "Wire.h"
#include "Arduino_DriveBus_Library.h"
#include <WiFi.h>
#include <WiFiUdp.h>

// ===== WiFi Config =====
const char *AP_SSID = "esp";
const char *AP_PASSWORD = "12345678";
WiFiUDP udp;
const int UDP_PORT = 4210;

// ===== PWM Config =====
#define VIBRATION_PWM_PIN 2
#define PWM_FREQUENCY 1000
#define PWM_RESOLUTION 8
#define PWM_CHANNEL 0

// ===== Touch Buttons =====
#define PLUS_BUTTON_X 50
#define PLUS_BUTTON_Y 160
#define MINUS_BUTTON_X 170
#define MINUS_BUTTON_Y 160
#define NEXT_BUTTON_X 90
#define NEXT_BUTTON_Y 250
#define BUTTON_WIDTH 80
#define BUTTON_HEIGHT 60
#define NEXT_BUTTON_WIDTH 100
#define NEXT_BUTTON_HEIGHT 50

// ===== Display Areas =====
#define VALUE_AREA_X 120
#define VALUE_AREA_Y 85
#define VALUE_AREA_W 80
#define VALUE_AREA_H 35

#define POINT_INFO_X 20
#define POINT_INFO_Y 50
#define POINT_INFO_W 280
#define POINT_INFO_H 20

#define PROGRESS_BAR_X ((LCD_WIDTH - 200) / 2)
#define PROGRESS_BAR_Y 230
#define PROGRESS_BAR_W 200
#define PROGRESS_BAR_H 15

#define STORED_POINTS_Y 320
#define STORED_POINTS_H 20

#define POINT_CIRCLES_Y 355
#define POINT_CIRCLES_H 20

// ===== Colors =====
#define BACKGROUND_COLOR 0x0841
#define BUTTON_COLOR 0x07E0
#define BUTTON_PRESSED_COLOR 0xF800
#define NEXT_BUTTON_COLOR 0x001F
#define NEXT_BUTTON_PRESSED_COLOR 0xF81F
#define TEXT_COLOR 0x0841
#define TITLE_COLOR 0xFFE0
#define VALUE_COLOR 0x07FF
#define POINT_COLOR 0xF81F

// ===== Vibration Vars =====
static int vibrationPercentage = 0;
static bool plusButtonPressed = false;
static bool minusButtonPressed = false;
static bool nextButtonPressed = false;
static size_t lastUpdateTime = 0;

// ===== Point Tracking =====
static int vibrationPoints[6] = {0, 0, 0, 0, 0, 0};
static int currentPoint = 1;
static int setNumber = 1;

// ===== Display & Touch =====
Arduino_DataBus *bus = new Arduino_ESP32QSPI(
    LCD_CS, LCD_SCLK, LCD_SDIO0, LCD_SDIO1,
    LCD_SDIO2, LCD_SDIO3);

Arduino_GFX *gfx = new Arduino_CO5300(bus, LCD_RST,
                                      0, false, LCD_WIDTH, LCD_HEIGHT,
                                      20, 0, 0, 0);

std::shared_ptr<Arduino_IIC_DriveBus> IIC_Bus =
    std::make_shared<Arduino_HWIIC>(IIC_SDA, IIC_SCL, &Wire);

void Arduino_IIC_Touch_Interrupt(void);

std::unique_ptr<Arduino_IIC> FT3168(new Arduino_FT3x68(IIC_Bus, FT3168_DEVICE_ADDRESS,
                                                       TP_RST, TP_INT, Arduino_IIC_Touch_Interrupt));

void Arduino_IIC_Touch_Interrupt(void)
{
    FT3168->IIC_Interrupt_Flag = true;
}

// ===== Vibration PWM =====
void updatePWM()
{
    int pwmValue = map(vibrationPercentage, 0, 25, 0, 255);
    ledcWrite(PWM_CHANNEL, pwmValue);
    Serial.printf("Vibration: %d%%, PWM Value: %d\n", vibrationPercentage, pwmValue);
}

// ===== Store Point & Send via UDP =====
void storeCurrentPoint()
{
    vibrationPoints[currentPoint - 1] = vibrationPercentage;
    Serial.printf("Stored Point %d: %d%%\n", currentPoint, vibrationPercentage);

    // --- Send Data via UDP ---
    char msg[50];
    sprintf(msg, "Set %d - Point %d: %d%%", setNumber, currentPoint, vibrationPercentage);
    udp.beginPacket("192.168.4.255", UDP_PORT); // broadcast
    udp.write((uint8_t*)msg, strlen(msg));
    udp.endPacket();
    Serial.printf("Sent UDP: %s\n", msg);

    currentPoint++;

    if (currentPoint > 6) {
        Serial.printf("Set %d complete!\n", setNumber);
        currentPoint = 1;
        setNumber++;
        for (int i = 0; i < 6; i++) vibrationPoints[i] = 0;
        vibrationPercentage = 0;
        updatePWM();
    }
}

// ===== UI Functions =====
void clearArea(int x, int y, int width, int height)
{
    gfx->fillRect(x, y, width, height, BACKGROUND_COLOR);
}

void drawSingleButton(int x, int y, int width, int height, const char* text, bool pressed, uint16_t normalColor, uint16_t pressedColor)
{
    uint16_t buttonColor = pressed ? pressedColor : normalColor;
    gfx->fillRoundRect(x, y, width, height, 8, buttonColor);
    gfx->drawRoundRect(x, y, width, height, 8, TEXT_COLOR);
    gfx->setTextColor(TEXT_COLOR);
    int textSize = (width > 90) ? 2 : 3;
    gfx->setTextSize(textSize);
    int textWidth = strlen(text) * 6 * textSize;
    int textHeight = 8 * textSize;
    int textX = x + (width - textWidth) / 2;
    int textY = y + (height - textHeight) / 2;
    gfx->setCursor(textX, textY);
    gfx->print(text);
}

void updateVibrationValue()
{
    clearArea(VALUE_AREA_X, VALUE_AREA_Y, VALUE_AREA_W, VALUE_AREA_H);
    gfx->setTextSize(4);
    gfx->setTextColor(VALUE_COLOR);
    gfx->setCursor(130, 90);
    gfx->printf("%d%%", vibrationPercentage);
}

void updateProgressBar()
{
    clearArea(PROGRESS_BAR_X, PROGRESS_BAR_Y, PROGRESS_BAR_W, PROGRESS_BAR_H);
    gfx->fillRect(PROGRESS_BAR_X, PROGRESS_BAR_Y, PROGRESS_BAR_W, PROGRESS_BAR_H, 0x2104);
    gfx->drawRect(PROGRESS_BAR_X, PROGRESS_BAR_Y, PROGRESS_BAR_W, PROGRESS_BAR_H, TEXT_COLOR);
    int fillWidth = map(vibrationPercentage, 0, 25, 0, PROGRESS_BAR_W - 4);
    if (fillWidth > 0) {
        gfx->fillRect(PROGRESS_BAR_X + 2, PROGRESS_BAR_Y + 2, fillWidth, PROGRESS_BAR_H - 4, BUTTON_COLOR);
    }
}

void updatePointInfo()
{
    clearArea(POINT_INFO_X, POINT_INFO_Y, POINT_INFO_W, POINT_INFO_H);
    gfx->setTextSize(2);
    gfx->setTextColor(POINT_COLOR);
    gfx->setCursor(20, 55);
    gfx->printf("Set %d - Point %d", setNumber, currentPoint);
    vibrationPercentage = 0;
    updatePWM();
    updateVibrationValue();
    updateProgressBar();
}

void updatePointCircles()
{
    clearArea(0, POINT_CIRCLES_Y - 10, LCD_WIDTH, POINT_CIRCLES_H);
    int pointWidth = (LCD_WIDTH - 40) / 6;
    for (int i = 0; i < 6; i++) {
        int pointX = 20 + (i * pointWidth);
        uint16_t pointColor;
        if (i < currentPoint - 1) pointColor = BUTTON_COLOR;
        else if (i == currentPoint - 1) pointColor = VALUE_COLOR;
        else pointColor = 0x5AEB;
        gfx->fillCircle(pointX + pointWidth/2, POINT_CIRCLES_Y, 8, pointColor);
        gfx->drawCircle(pointX + pointWidth/2, POINT_CIRCLES_Y, 8, TEXT_COLOR);
    }
}

void updatePlusButton() { drawSingleButton(PLUS_BUTTON_X, PLUS_BUTTON_Y, BUTTON_WIDTH, BUTTON_HEIGHT, "+", plusButtonPressed, BUTTON_COLOR, BUTTON_PRESSED_COLOR); }
void updateMinusButton() { drawSingleButton(MINUS_BUTTON_X, MINUS_BUTTON_Y, BUTTON_WIDTH, BUTTON_HEIGHT, "-", minusButtonPressed, BUTTON_COLOR, BUTTON_PRESSED_COLOR); }
void updateNextButton() { drawSingleButton(NEXT_BUTTON_X, NEXT_BUTTON_Y, NEXT_BUTTON_WIDTH, NEXT_BUTTON_HEIGHT, "NEXT", nextButtonPressed, NEXT_BUTTON_COLOR, NEXT_BUTTON_PRESSED_COLOR); }

void drawFullInterface()
{
    gfx->fillScreen(BACKGROUND_COLOR);
    gfx->setTextSize(4);
    gfx->setTextColor(TITLE_COLOR);
    gfx->setCursor(20, 20);
    gfx->print("Dia Tech-D");
    updatePointInfo();
    updateVibrationValue();
    updatePlusButton();
    updateMinusButton();
    updateNextButton();
    updateProgressBar();
    updatePointCircles();
}

bool isPointInButton(int x, int y, int buttonX, int buttonY, int width, int height)
{
    return (x >= buttonX && x <= buttonX + width &&
            y >= buttonY && y <= buttonY + height);
}

// ===== Handle Touch =====
void handleTouch()
{
    if (FT3168->IIC_Interrupt_Flag) {
        FT3168->IIC_Interrupt_Flag = false;
        int32_t touch_x = FT3168->IIC_Read_Device_Value(FT3168->Arduino_IIC_Touch::Value_Information::TOUCH_COORDINATE_X);
        int32_t touch_y = FT3168->IIC_Read_Device_Value(FT3168->Arduino_IIC_Touch::Value_Information::TOUCH_COORDINATE_Y);
        uint8_t fingers_number = FT3168->IIC_Read_Device_Value(FT3168->Arduino_IIC_Touch::Value_Information::TOUCH_FINGER_NUMBER);

        if (fingers_number > 0) {
            if (isPointInButton(touch_x, touch_y, PLUS_BUTTON_X, PLUS_BUTTON_Y, BUTTON_WIDTH, BUTTON_HEIGHT)) {
                plusButtonPressed = true;
                updatePlusButton();
                if (vibrationPercentage < 25) {
                    vibrationPercentage++;
                    updatePWM();
                    updateVibrationValue();
                    updateProgressBar();
                }
            } else plusButtonPressed = false;

            if (isPointInButton(touch_x, touch_y, MINUS_BUTTON_X, MINUS_BUTTON_Y, BUTTON_WIDTH, BUTTON_HEIGHT)) {
                minusButtonPressed = true;
                updateMinusButton();
                if (vibrationPercentage > 0) {
                    vibrationPercentage--;
                    updatePWM();
                    updateVibrationValue();
                    updateProgressBar();
                }
            } else minusButtonPressed = false;

            if (isPointInButton(touch_x, touch_y, NEXT_BUTTON_X, NEXT_BUTTON_Y, NEXT_BUTTON_WIDTH, NEXT_BUTTON_HEIGHT)) {
                nextButtonPressed = true;
                updateNextButton();
                storeCurrentPoint();
                updatePointInfo();
                updatePointCircles();
            } else nextButtonPressed = false;
        }
    }
}

// ===== Setup =====
void setup()
{
    Serial.begin(115200);
    Serial.println("Diarc Vibration Control Starting...");

    // Start WiFi Access Point
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASSWORD);
    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP: ");
    Serial.println(IP);
    udp.begin(UDP_PORT);

    pinMode(LCD_EN, OUTPUT);
    digitalWrite(LCD_EN, HIGH);
    gfx->begin();
    gfx->fillScreen(BLACK);
    gfx->setTextSize(3);
    gfx->setTextColor(WHITE);
    gfx->setCursor(50, 200);
    gfx->print("Starting...");
    delay(2000);

    ledcSetup(PWM_CHANNEL, PWM_FREQUENCY, PWM_RESOLUTION);
    ledcAttachPin(VIBRATION_PWM_PIN, PWM_CHANNEL);

    if (!FT3168->begin()) {
        Serial.println("Touch controller failed!");
        gfx->fillScreen(RED);
        gfx->setTextSize(2);
        gfx->setTextColor(WHITE);
        gfx->setCursor(50, 200);
        gfx->print("Touch Init Failed!");
        delay(2000);
    } else {
        Serial.println("Touch controller OK");
    }

    for (int i = 0; i <= 255; i += 5) {
        gfx->Display_Brightness(i);
        delay(10);
    }

    updatePWM();
    drawFullInterface();
}

// ===== Loop =====
void loop()
{
    handleTouch();
    if (millis() - lastUpdateTime > 100) {
        lastUpdateTime = millis();
    }
}
