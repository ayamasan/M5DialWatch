#include <M5Dial.h>

#define WIFI_SSID     "xxxxx"
#define WIFI_PASSWORD "xxxxxxxxxx"
#define NTP_TIMEZONE  "UTC-8"
#define NTP_SERVER1   "0.pool.ntp.org"
#define NTP_SERVER2   "1.pool.ntp.org"
#define NTP_SERVER3   "2.pool.ntp.org"

#include <WiFi.h>

#if __has_include(<esp_sntp.h>)
#include <esp_sntp.h>
#define SNTP_ENABLED 1
#elif __has_include(<sntp.h>)
#include <sntp.h>
#define SNTP_ENABLED 1
#endif

char buf[100];
static constexpr const char* const wd[7] 
= {"Sun", "Mon", "Tue", "Wed", "Thr", "Fri", "Sat"};

int wifion = 0;
time_t ttold;
int sx, sy;
int oldday = 0;

// 初期設定
void setup() {
	// put your setup code here, to run once:
	
	auto cfg = M5.config();
	M5Dial.begin(cfg, true, true);
	
	M5Dial.Display.fillScreen(BLACK);
	M5Dial.Display.setTextColor(GREEN, BLACK);
	M5Dial.Display.setTextDatum(middle_center);
	M5Dial.Display.setTextFont(&fonts::Orbitron_Light_32);
	M5Dial.Display.setTextSize(0.5);
	
	sx = M5Dial.Display.width() / 2;
	sy = M5Dial.Display.height() / 2;
	
	
	// WiFi接続
	M5Dial.Display.fillScreen(BLACK);
	M5Dial.Display.drawString("WiFi connecting...", sx, sy);
	WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
	int retry = 0;
	int tout = 0;
	wifion = 1;
	while(WiFi.status() != WL_CONNECTED){
		delay(500);
		tout++;
		if(tout > 10){  // 5秒でタイムアウト、接続リトライ
			retry++;
			if(retry > 3){
				wifion = 0;  // WiFi接続できず
				break;
			}
			WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
			tout = 0;
		}
	}
	if(wifion != 0){
		Serial.println("WiFi Connected.");
		M5Dial.Display.fillScreen(BLACK);
		M5Dial.Display.drawString("Connected.", sx, sy);
	}
	else{
		Serial.println("ERROR : WiFi cannot Connect.");
		M5Dial.Display.fillScreen(BLACK);
		M5Dial.Display.drawString("Not Connect.", sx, sy);
	}
	
	if(wifion != 0){
		// NTP
		configTzTime(NTP_TIMEZONE, NTP_SERVER1, NTP_SERVER2, NTP_SERVER3);
		tout = 0;
		retry = 0;
		while(sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED){
			delay(1000);
			tout++;
			if(tout > 10){  // 10秒でタイムアウト、接続リトライ
				retry++;
				if(retry > 3){
					wifion = 0;  // NTPサーバー接続できず
					break;
				}
				configTzTime(NTP_TIMEZONE, NTP_SERVER1, NTP_SERVER2, NTP_SERVER3);
				tout = 0;
			}
		}
		M5Dial.Display.fillScreen(BLACK);
		M5Dial.Display.drawString("NTP Connected.", sx, sy);
	}
	
	// NTP時刻取得できたのでRTCにセット
	if(wifion != 0){
		// RTC
		time_t tt = time(nullptr) + 1;  // Advance one second.
		while(tt > time(nullptr)){
			;  /// Synchronization in seconds
		}
		tt += (9 * 60 * 60);  // JPN
		M5Dial.Rtc.setDateTime(gmtime(&tt));
		ttold = tt;
		Serial.printf("TT = %ld\n", tt);
	}
	
	// RTCから時刻を読み出す
	auto dt = M5Dial.Rtc.getDateTime();
	sprintf(buf, 
		"%04d/%02d/%02d(%s) %02d:%02d:%02d\r\n",
		dt.date.year, dt.date.month, dt.date.date,
		wd[dt.date.weekDay], dt.time.hours, dt.time.minutes,
		dt.time.seconds
	);
	oldday = 0; //dt.date.date;
	
	// NTPなしなら、RTC時刻からtime_tを作成
	if(wifion == 0){
		struct tm tm;
		tm.tm_year  = dt.date.year - 1900;    // 年 [1900からの経過年数]
		tm.tm_mon   = dt.date.month - 1;   // 月 [0-11] 0から始まることに注意
		tm.tm_mday  = dt.date.date;    // 日 [1-31]
		tm.tm_wday  = dt.date.weekDay; // 曜日 [0:日 1:月 ... 6:土]
		tm.tm_hour  = dt.time.hours;   // 時 [0-23]
		tm.tm_min   = dt.time.minutes; // 分 [0-59]
		tm.tm_sec   = dt.time.seconds; // 秒 [0-61]
		tm.tm_isdst = -1;              // 夏時間フラグ
		
		time_t tt = mktime(&tm) + (8 * 60 * 60);
		ttold = tt;
		Serial.printf("TT = %ld\n", tt);
	}
	
	M5Dial.Display.fillScreen(NAVY);
	M5.Lcd.fillRect(0, sy-30, M5Dial.Display.width(), 60, WHITE);
}

// メインループ
void loop() {
	// put your main code here, to run repeatedly:
	m5::rtc_datetime_t dt = M5Dial.Rtc.getDateTime();
	
	struct tm tm;
	tm.tm_year  = dt.date.year - 1900;    // 年 [1900からの経過年数]
	tm.tm_mon   = dt.date.month - 1;   // 月 [0-11] 0から始まることに注意
	tm.tm_mday  = dt.date.date;    // 日 [1-31]
	tm.tm_wday  = dt.date.weekDay; // 曜日 [0:日 1:月 ... 6:土]
	tm.tm_hour  = dt.time.hours;   // 時 [0-23]
	tm.tm_min   = dt.time.minutes; // 分 [0-59]
	tm.tm_sec   = dt.time.seconds; // 秒 [0-61]
	tm.tm_isdst = -1;              // 夏時間フラグ
	
	time_t tt = mktime(&tm) + (8 * 60 * 60);
	
	// たまに5時間（+432000）間違えるので対処
	// +-60秒以上は無視する
	if(tt != ttold){
		if(tt > 2000000000){
			Serial.printf("ERROR : Delta = %ld > 2000000000 (%ld)\n", tt, ttold);
			tt = ttold;
		}
		else if(tt < 0){
			Serial.printf("ERROR : Delta = %ld < 0 (%ld)\n", tt, ttold);
			tt = ttold;
		}
		else if(((tt - ttold) > 60) || ((tt - ttold) < -60)){
			Serial.printf("ERROR : Delta = %ld (%ld - %ld)\n", (tt-ttold), tt, ttold);
			tt = ttold;
		}
	}
	
	if(tt > ttold){
		// 年月日更新
		if(oldday != dt.date.date){
			M5Dial.Display.fillScreen(NAVY);
			M5.Lcd.fillRect(0, sy-30, M5Dial.Display.width(), 60, WHITE);
			
			if(dt.date.weekDay == 0){
				M5Dial.Display.setTextColor(RED, NAVY);    // 赤色
			}
			else if(dt.date.weekDay == 6){
				M5Dial.Display.setTextColor(CYAN, NAVY);   // 水色
			}
			else{
				M5Dial.Display.setTextColor(GREEN, NAVY);  // 緑
			}
			sprintf(buf, "%04d R%d", dt.date.year, dt.date.year-2018);
			M5Dial.Display.setTextFont(&fonts::Orbitron_Light_32);
			M5Dial.Display.setTextSize(1);
			M5Dial.Display.drawString(buf, sx, sy-54);
			Serial.println(buf);
			sprintf(buf, "%02d/%02d %s"
			, dt.date.month
			, dt.date.date
			, wd[dt.date.weekDay]);
			M5Dial.Display.setTextSize(1);
			M5Dial.Display.drawString(buf, sx, sy+50);
			Serial.println(buf);
			oldday = dt.date.date;
		}
		
		// 時間更新
		M5Dial.Display.setTextSize(1);
		M5Dial.Display.setTextColor(BLACK, WHITE);
		M5Dial.Display.setTextFont(7);
		sprintf(buf, "%02d:%02d:%02d"
		, dt.time.hours
		, dt.time.minutes
		, dt.time.seconds);
		M5Dial.Display.drawString(buf, sx, sy);
		
		Serial.printf("TT = %ld > %ld\n", tt, ttold);
		
		M5Dial.Speaker.tone(1000, 20);
		ttold = tt;
	}
	else{
		delay(20);
	}
}
