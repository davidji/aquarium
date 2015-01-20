#include <SoftwareSerial.h>
#include <Time.h>
#include <Messenger.h>

uint8_t brightness = 192;
boolean inverted = true;
boolean autoMode = false;

const int DATETIME_STR_LEN = 19;
const int TIME_STR_LEN = 5;
const int NAME_STR_LEN = 32;
const uint8_t MAX_BRIGHTNESS = 0xFF;

const int ledPin = 3;
const int maxPoints = 20;

Messenger serialMessage = Messenger(); 
Messenger bluetoothMessage = Messenger();

SoftwareSerial bluetooth(4, 5, false);

void setup() {
  writeBrightness(brightness);
 
  Serial.begin(9600);
  serialMessage.attach(messageReadySerial);
  
  bluetooth.begin(9600);
  bluetoothMessage.attach(messageReadyBluetooth);
}


void loop() {
  if(Serial.available()) serialMessage.process(Serial.read());
  if(bluetooth.available())  bluetoothMessage.process(bluetooth.read());
  
  if(autoMode) {
    uint8_t value = pointValue(countSecondsSinceMidnight(now()));
    if(value != brightness) {
      brightness = value;
      writeBrightness(brightness);
      printBrightness(&bluetooth);
      printBrightness(&Serial);
    }
  }
}

void writeBrightness(uint8_t value) {
  if(inverted) {
    value = MAX_BRIGHTNESS - value;
  }
  
  analogWrite(ledPin, value);
}

void messageReadySerial() {
  messageReady(&serialMessage, &Serial);
}

void messageReadyBluetooth() {
  messageReady(&bluetoothMessage, &bluetooth);
}

// Create the callback function
void messageReady(Messenger *message, Print *out) {
    if(message->checkString("set")) {
      if(message->checkString("time")) {
        char timestr[DATETIME_STR_LEN + 1];
        message->copyString(timestr, DATETIME_STR_LEN);
        setTime(timestr);
        printDateTimeVar(out);
      } else if(message->checkString("brightness")) {
        autoMode = false;
        brightness = message->readInt();
        writeBrightness(brightness);
        printBrightness(out);
      } else if(message->checkString("inverted")) {
        if(message->checkString("true")) {
          inverted = true;
          writeBrightness(brightness);
        } else if(message->checkString("false")) {
          inverted = false;
          writeBrightness(brightness);
        } else {
          out->println("inverted must be true or false");
        }
        
        out->print("inverted=");
        out->println(inverted);
        
      } else if(message->checkString("program")) {
        if(message->checkString("all")) {
          if(message->checkString("clear")) {
            pointClearAll();
          }
        } else {
          int point = message->readInt();
          if(message->checkString("clear")) {
            pointClear(point);
          } else {
            char name[NAME_STR_LEN];
            char time[TIME_STR_LEN];
            message->copyString(name, NAME_STR_LEN + 1);
            message->copyString(time, TIME_STR_LEN + 1);
            if(pointSet(point, name, time, message->readInt())) {
              pointPrint(point, out);
            } else {
              out->println("set program invalid");
            }
          }
        }
      } else {
        out->println("set: unknown");
      }
    } else if(message->checkString("get")) {
      if(message->checkString("time")) {
        printDateTimeVar(out);
      } else if(message->checkString("brightness")) {
        printBrightness(out);
      } else if(message->checkString("program")) {
        if(message->checkString("all")) {
          pointPrintAll(out);
        } else {
          pointPrint(message->readInt(), out);
        }
      } else {
        out->println("set: unknown");
      }
    } else if(message->checkString("auto")) {
      out->println("auto");
      autoMode = true;
    } else if(message->checkString("test")) {
      pointTest(out); 
    }
}

void printDateTimeVar(Print *out) {
   out->print("time=");
   printDateTime(out);
   out->println();
}

void printBrightness(Print *out) {
  out->print("brightness=");
  out->print(brightness, DEC);
  out->println();
}

void printPadded(Print *printer, int value) {
  if(value < 10)
    printer->print(0, DEC);
  printer->print(value, DEC);
}

void printDateTime(Print *printer) {
  time_t t = now();
  printPadded(printer, year(t));
  printer->print('-');
  printPadded(printer, month(t));
  printer->print('-');
  printPadded(printer, day(t));
  printer->print('T');
  printPadded(printer, hour(t));
  printer->print(':');
  printPadded(printer, minute(t));
  printer->print(':');
  printPadded(printer, second(t));
}

const int DATETIME_STR_YEAR = 0;
const int DATETIME_STR_MONTH = 5;
const int DATETIME_STR_DAY = 8;
const int DATETIME_STR_HOUR = 11;
const int DATETIME_STR_MIN = 14;
const int DATETIME_STR_SEC = 17;

void setTime(char *timestr) {
 setTime(parseInt(timestr, DATETIME_STR_HOUR, 2),
         parseInt(timestr, DATETIME_STR_MIN, 2),
         parseInt(timestr, DATETIME_STR_SEC, 2),
         parseInt(timestr, DATETIME_STR_DAY, 2),
         parseInt(timestr, DATETIME_STR_MONTH, 2),
         parseInt(timestr, DATETIME_STR_YEAR, 4));
}


int parseInt(char *str, int off, int len) {
  int value = 0;
  for(int i = off; i != (off + len); ++i) {
    int next = str[i];
    if(next >= '0' && next <= '9') {
      value = value*10 + (next - '0');
    }
    else {
      break;
    }     
  }

  return value; 
}

// --------------------------------------------------------------------------------------
// PROGRAMMING

class TimeValue {

public:
        boolean enabled;
	char name[NAME_STR_LEN+1];
	long start_time;
	long start_value;

	long delta_t(TimeValue *next);
	long delta_v(TimeValue *next);
	uint8_t value(long seconds, TimeValue *next);
        boolean includes(long seconds, TimeValue *next);
	void print(Print *printer);
};

long TimeValue::delta_t(TimeValue *next) {
	return interval(start_time, next->start_time);
}

long TimeValue::delta_v(TimeValue *next) {
	return next->start_value - start_value;
}

uint8_t TimeValue::value(long seconds, TimeValue *next) {
    long afterStart = interval(start_time, seconds);
    long value = start_value + (delta_v(next) * afterStart) / delta_t(next);
    return byte(value);
}

boolean TimeValue::includes(long seconds, TimeValue *next) {
  return delta_t(next) > interval(start_time, seconds);
}

void TimeValue::print(Print *printer) {
  printer->print(name);
  printer->print(' ');
  printer->print(enabled ? 'E' : ' ');
  printer->print(' ');
  printTime(printer, start_time);
  printer->print(' ');
  printer->print(start_value, DEC);
}

// --------------------------------------------------------------------------------------

TimeValue points[maxPoints];

void pointClearAll() {
  for(int i = 0; i != maxPoints; ++i) {
    pointClear(i);
  }
}

void pointClear(int point) {
  points[point].enabled = false;
  strcpy(points[point].name, "");
  points[point].start_time = 0;
  points[point].start_value = 0;
}

boolean pointSet(int point, char name[], char time[], int value) {
  long start_time = countSecondsSinceMidnight(parseInt(time, 0, 2), parseInt(time, 3, 2), 0);
  points[point].enabled = true;
  strcpy(points[point].name, name);
  points[point].start_time = start_time;
  points[point].start_value = value;
  return true;
}

void pointPrint(int point, Print *out) {
  printPadded(out, point);
  out->print(": ");
  points[point].print(out);
  out->println();
}

void pointPrintAll(Print *out) {
  for(int i = 0; i != maxPoints; ++i) {
    pointPrint(i, out);
  }  
}

void pointTest(Print *out) {
  int value = -1;
  int point = -1;
  for(int hour = 0; hour != 24; ++hour) {
    for(int minute = 0; minute != 60; ++minute) {
      long secs = countSecondsSinceMidnight(hour, minute, 0);
      if(point != pointSearch(secs)) {
        point = pointSearch(secs);
        out->println(points[point].name);
      }
      
      if(value != pointValue(secs)) {
          value = pointValue(secs);
          printTime(out, secs);
          out->print(' ');
          out->println(value, DEC);
      }
    }
  }
}

uint8_t pointValue(long seconds) {
  int point = pointSearch(seconds);
  if(point < 0) {
    return 0;
  } else {
    int next = nextPoint(point);
    return points[point].value(seconds, &points[next]);
  }
}

int nextPoint(int point) {
  int index = point + 1;
  for(; (index < maxPoints || (index%maxPoints) < point) && !points[index%maxPoints].enabled; ++index);
  return index%maxPoints;
}

int pointSearch(long seconds) {
  for(int point = 0; point != maxPoints; ++point) {
    if(points[point].enabled) {
      int next = nextPoint(point);
      if(points[point].includes(seconds, &points[next])) {
        return point;
      }
    }
  }
  return -1;
}

long countSecondsSinceMidnight(time_t time) {
  return countSecondsSinceMidnight(hour(time), minute(time), second(time)); 
}

long countSecondsSinceMidnight(uint8_t hours, uint8_t minutes, uint8_t seconds) {
	long total = hours;
	total *= 60;
	total += minutes;
	total *= 60;
	total += seconds;
	return total;
}

long SECONDS_PER_DAY = countSecondsSinceMidnight(24, 0, 0);

// This assumes that if to is before from, that to is in fact tomorrow
long interval(long from, long to) {
  if (from >= to)
    to += SECONDS_PER_DAY;
  return (to - from);
}

void printTime(Print *printer, long seconds) {
  printPadded(printer, seconds/3600);
  printer->print(':');
  printPadded(printer, (seconds/60)%60);
}


