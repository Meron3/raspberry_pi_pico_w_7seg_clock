//	https://github.com/Meron3/raspberry_pi_7esg_clock/
// @Meron3, https://zenn.dev/meron3/articles/8aa2274458e5cb 
// 2024-11-17


#include <TM1637Display.h>
#include <NTPClient.h>
#include <WiFi.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <TimeLib.h>

const char* ssid  = "your_ssid";
const char* pw    = "your_password";

const char* NTPSRV      = "ntp.nict.jp";  //  NTPサーバーのアドレス
const long  GMT_OFFSET  = 9;  				//UTCからJSTまでの時差

float temp = 20, voltage;
int watch = 0, now_time = 0;
char colon = 0, count = 30, watch_sec = 0, watch_min = 0, watch_st = 0, mode = 0;   //modeの値　0:時計,1:温度計,2:ストップウォッチ
uint8_t data[] = {0b01000000,0b01000000,0b01000000,0b01000000};	//"--:-.-"を初期化時に表示させる用

#define sw1   14		//GPIOのピン番号を指定
#define sw2   15

#define adc   26

#define led_colon   0
#define led_point   1  

TM1637Display display(2, 3);		//GPIO2:CLK,GPIO3:DIOに接続

void setClock(const char* ntpsrv, const long gmt_offset)
 {NTP.begin(ntpsrv);                              // NTPサーバとの同期
  NTP.waitSet();

  time_t now = time(nullptr);                     // 時刻の取得
  struct tm timeinfo;                             // tm（時刻）構造体の生成
  gmtime_r(&now, &timeinfo);                      // time_tからtmへ変換

  setTime(timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec, timeinfo.tm_mday, timeinfo.tm_mon+1, timeinfo.tm_year+1900); 
  adjustTime(gmt_offset * SECS_PER_HOUR);         // 時刻を日本時間へ変更
}

void PwmSetUp()
{
  gpio_set_function(led_colon, GPIO_FUNC_PWM);
  gpio_set_function(led_point, GPIO_FUNC_PWM);

  uint slice_num_colon = pwm_gpio_to_slice_num(led_colon);
  uint slice_num_point = pwm_gpio_to_slice_num(led_point);

  pwm_set_wrap(slice_num_colon, 12);  
  pwm_set_wrap(slice_num_point, 12);  
  
  pwm_set_enabled(slice_num_colon, true);
  pwm_set_enabled(slice_num_point, true);

  pwm_set_chan_level(slice_num_colon, led_colon, 500);
  pwm_set_chan_level(slice_num_point, led_point, 500);

}

void setup() 
{ 
  display.setBrightness(3);
  display.showNumberDec(8888); 

  pinMode(sw1, INPUT);
	gpio_pull_up(sw1);
  pinMode(sw2, INPUT);
	gpio_pull_up(sw2);

  adc_init();
  adc_gpio_init(adc); // GPIO 26 = ADC0 
  adc_select_input(0);

  PwmSetUp();
  
  WiFi.begin(ssid, pw);
  display.setSegments(data);
  delay(500);

  setClock(NTPSRV, GMT_OFFSET);   //時刻取得実行

}

void loop()
 {
  if (digitalRead(sw1) == LOW)
  {
    if (mode == 0) mode = 1;
    
    else if (mode == 1) mode = 2;
    
    else if (mode == 2) mode = 0;

    watch_sec = 0;
    watch_min = 0;
    watch = 0;
  }

  const float cf = 3.4f / 4096;
  voltage = adc_read() * cf;
  temp = ( temp + ( -1481.96 + sqrt(2.1962 * pow(10,6) + (1.8639 - voltage) / (3.88 * pow(10,-6))))) / 2 ;

  now_time = hour() * 100 + minute();  

  if (mode == 0)   //時計
  {
    pwm_set_chan_level(pwm_gpio_to_slice_num(led_point), led_point, 0);
    display.showNumberDec(now_time);

    if(colon == 0)
    {
      pwm_set_chan_level(pwm_gpio_to_slice_num(led_colon), led_colon, 500);
      colon = 1;
    }
    else if(colon == 1)
    {
     pwm_set_chan_level(pwm_gpio_to_slice_num(led_colon), led_colon, 0);
      colon = 0;
    }
    count = 30;
  }
  
  else if (mode == 1)   //温度計
  { 
    pwm_set_chan_level(pwm_gpio_to_slice_num(led_colon), led_colon, 0);
    pwm_set_chan_level(pwm_gpio_to_slice_num(led_point), led_point, 500);

    if (count > 29)
    {
      display.showNumberDec(temp * 10);
      count = 0;
    }
  }

  else if (mode == 2)   //ストップウォッチ
  { 
    if (watch == 0) display.showNumberDec(0,true,4,0);
    
    pwm_set_chan_level(pwm_gpio_to_slice_num(led_colon), led_colon, 500);
    pwm_set_chan_level(pwm_gpio_to_slice_num(led_point), led_point, 0);

    if (digitalRead(sw2) == LOW)
    {
      if (watch_st == 1) watch_st = 0;
      else if (watch_st == 0) watch_st = 1;
    }
    
    if (watch_st == 0) pwm_set_chan_level(pwm_gpio_to_slice_num(led_colon), led_colon, 0);


    if (watch_st == 1)
    {
      watch_sec += 1;

      if (watch_sec == 60)
     {
        watch_sec = 0;
        watch_min += 1;
     }
      if (watch > 9900)
      {
        watch_st = 0;
      }
  
      watch = watch_min * 100 + watch_sec;
      display.showNumberDec(watch,true,4,0);

    }
    count = 30;
  }

  count = count + 1;    //loop関数は1sおきに実行
  delay(1000);
}
