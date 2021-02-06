#include <M5Stack.h>
#include <ArduinoJson.h>
#include "SparkClass.h"
#include "SparkPresets.h"

#include "BluetoothSerial.h" // https://github.com/espressif/arduino-esp32

// Bluetooth vars
#define SPARK_NAME "Spark 40 Audio"
#define MY_NAME    "MIDI Spark"

#define BACKGROUND TFT_BLACK
#define TEXT_COLOUR TFT_WHITE

#define MIDI   68
#define STATUS 34
#define IN     102
#define OUT    172

#define STD_HEIGHT 30
#define PANEL_HEIGHT 68

BluetoothSerial SerialBT;


// Spark vars
SparkClass sc1, sc2, sc3, sc4, scr, scrp;
SparkClass sc_setpreset7f;

SparkPreset preset;
SparkClass sc_getserial;

// ------------------------------------------------------------------------------------------
// Display routintes

// Display vars
#define DISP_LEN 50
char outstr[DISP_LEN+1];
char instr[DISP_LEN+1];
char statstr[DISP_LEN+1];
int bar_pos;
unsigned long bar_count;

void display_background(const char *title, int y, int height)
{
   int x_pos;

   x_pos = 160 - 3 * strlen(title);
//   M5.Lcd.fillRoundRect(0, y, 320, height, 4, BACKGROUND);
   M5.Lcd.drawRoundRect(0, y, 320, height, 4, TEXT_COLOUR);
   M5.Lcd.setCursor(x_pos, y);
   M5.Lcd.print(title);
   M5.Lcd.setCursor(8,y+8);
 
}

void do_backgrounds()
{
   M5.Lcd.setTextColor(TEXT_COLOUR, BACKGROUND);
   M5.Lcd.setTextSize(1);
   display_background("", 0, STD_HEIGHT);
   display_background(" STATUS ",    STATUS, PANEL_HEIGHT);
   display_background(" RECEIVED ",  IN,     PANEL_HEIGHT);
   display_background(" SENT ",      OUT,    PANEL_HEIGHT);         
   M5.Lcd.setTextSize(2); 
   M5.Lcd.setCursor (45, 8);
   M5.Lcd.print("Spark Pedal Override");
   bar_pos=0;
   bar_count = millis();
   
}

void display_bar()
{
   if (millis() - bar_count > 400) {
      bar_count = millis();
      M5.Lcd.fillRoundRect(15 + bar_pos*30, STATUS + STD_HEIGHT + 10 , 15, 15, 4, BACKGROUND);
      bar_pos ++;
      bar_pos %= 10;
      M5.Lcd.fillRoundRect(15 + bar_pos*30, STATUS + STD_HEIGHT + 10, 15, 15, 4, TEXT_COLOUR);
   }
}

void display_val(float val)
{
   int dist;

   dist = int(val * 290);
   M5.Lcd.fillRoundRect(15 + dist, IN + STD_HEIGHT + 10 , 290 - dist, 15, 4, BACKGROUND);
   M5.Lcd.drawRoundRect(15, IN + STD_HEIGHT + 10 , 290 , 15, 4, TEXT_COLOUR);
   M5.Lcd.fillRoundRect(15, IN + STD_HEIGHT + 10 , dist, 15, 4, TEXT_COLOUR);
}

void display_str(const char *a_str, int y)
{
   char b_str[30];

   strncpy(b_str, a_str, 25);
   if (strlen(a_str) < 25) strncat(b_str, "                         ", 25-strlen(a_str));
   M5.Lcd.setCursor(8,y+8);
   M5.Lcd.print(b_str);
}



// ------------------------------------------------------------------------------------------
// Bluetooth routines

bool connected;
int bt_event;

void btEventCallback(esp_spp_cb_event_t event, esp_spp_cb_param_t *param){
  // On BT connection close
  if (event == ESP_SPP_CLOSE_EVT ){
    // TODO: Until the cause of connection instability (compared to Pi version) over long durations 
    // is resolved, this should keep your pedal and amp connected fairly well by forcing reconnection
    // in the main loop
    connected = false;
  }
  if (event != ESP_SPP_CLOSE_EVT ) {
     bt_event = event;
  }
}

void start_bt() {
   SerialBT.register_callback(btEventCallback);
   
   if (!SerialBT.begin (MY_NAME, true)) {
      display_str("Bluetooth init fail", STATUS);
      while (true);
   }    
   connected = false;
   bt_event = 0;
}


void connect_to_spark() {
   int rec;

   while (!connected) {
      display_str("Connecting to Spark", STATUS);
      connected = SerialBT.connect(SPARK_NAME);
      if (connected && SerialBT.hasClient()) {
         display_str("Connected to Spark", STATUS);
      }
      else {
         connected = false;
         delay(3000);
      }
   }
   // flush anything read from Spark - just in case

   while (SerialBT.available())
      rec = SerialBT.read(); 
}

// ----------------------------------------------------------------------------------------
// Send messages to the Spark (and receive an acknowledgement where appropriate

void send_bt(SparkClass& spark_class)
{
   int i;

   // if multiple blocks then send all but the last - the for loop should only run if last_block > 0
   for (i = 0; i < spark_class.last_block; i++) {
      SerialBT.write(spark_class.buf[i], BLK_SIZE);
 
   }
      
   // and send the last one      
   SerialBT.write(spark_class.buf[spark_class.last_block], spark_class.last_pos+1);

}


// Helper functions to send an acknoweledgement and to send a requesjames luit for a preset

void send_ack(int seq, int cmd)
{
   byte ack[]{  0x01, 0xfe, 0x00, 0x00, 0x41, 0xff, 0x17, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
                0xf0, 0x01, 0xff, 0x00, 0x04, 0xff, 0xf7};
   ack[18] = seq;
   ack[21] = cmd;            

   SerialBT.write(ack, sizeof(ack));
}

void send_preset_request(int preset)
{
   byte req[]{0x01, 0xfe, 0x00, 0x00, 0x53, 0xfe, 0x3c, 0x00,
              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
              0xf0, 0x01, 0x04, 0x00, 0x02, 0x01,
              0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00,
              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
              0x00, 0x00, 0x00, 0x00, 0x00, 0xf7};
 
   req[24] = preset;
   SerialBT.write(req, sizeof(req));      
}


void send_receive_bt(SparkClass& spark_class)
{
   int i;
   int rec;

   // if multiple blocks then send all but the last - the for loop should only run if last_block > 0
   for (i = 0; i < spark_class.last_block; i++) {
      SerialBT.write(spark_class.buf[i], BLK_SIZE);
      // read the ACK
      rec = scr.get_data();
      if (rec <0) {
         Serial.print("Receive error "); 
         Serial.println(rec);
      }
   }
      
   // and send the last one      
   SerialBT.write(spark_class.buf[spark_class.last_block], spark_class.last_pos+1); 
   
   // read the ACK
      rec = scr.get_data();
      if (rec <0) {
         Serial.print("Receive error "); 
         Serial.println(rec);
      } 
}


// ------------------------------------------------------------------------------------------

int i, j, p;
int pres;

int cmd, sub_cmd;
char a_str[STR_LEN+1];
char b_str[STR_LEN+1];
int param;
float val;

unsigned long keep_alive;

void setup() {
   M5.begin();
   M5.Power.begin();
//   M5.Power.setPowerBoostKeepOn(true);
   M5.Lcd.fillScreen(BACKGROUND);
   do_backgrounds();

   display_str("Started",      STATUS);
   display_str("Nothing out",  OUT);
   display_str("Nothing in",   IN);

   start_bt();
   connect_to_spark();

   keep_alive = millis();
   
   // set up the change to 7f message for when we send a full preset
   sc_setpreset7f.change_hardware_preset(0x7f);
   sc_getserial.get_serial();
}

void loop() {
   int av;
   int ret;
   int ct;
   
   M5.update();
   display_bar();
   
/*
   if (M5.BtnA.wasReleased()) {
      if (connected)
         display_str("Connected still", IN);
      else
         display_str("No longer connected", IN);      
   }

   if (M5.BtnB.wasReleased()) {
      connected = false;
   }

   if (!SerialBT.hasClient()) {
      display_str("Lost connection", STATUS);
      while (true);
   }
*/
   
   // this will connect if not already connected
   if (!connected) connect_to_spark();

   // see if this keeps the connection alive
   if (millis() - keep_alive  > 10000) {
      keep_alive = millis();
      send_receive_bt(sc_getserial);
   }
   
//   if (bt_event != 0) {
//      snprintf(statstr, DISP_LEN-1,"BT Event: %d", bt_event);
//      display_str(statstr, STATUS);
//      bt_event = 0;
//   }
   delay(10);
   
   if (SerialBT.available()) {
      // reset timeout
      keep_alive = millis();
      
      if (scr.get_data() >= 0 && scr.parse_data() >=0) {
         for (i=0; i<scr.num_messages; i++) {
            cmd = scr.messages[i].cmd;
            sub_cmd = scr.messages[i].sub_cmd;

            if (cmd == 0x03 && sub_cmd == 0x01) {
               ret = scr.get_preset(i, &preset);
               Serial.print("Get preset ");
               Serial.println(ret);
               if (ret >= 0){
                  snprintf(instr, DISP_LEN-1, "Preset: %s", preset.Name);
                  display_str(instr, IN);
               }
            }
            else if (cmd == 0x03 && sub_cmd == 0x37) {
               ret = scr.get_effect_parameter(i, a_str, &param, &val);
               Serial.print("Get effect params ");
               Serial.println(ret);
               if (ret >=0) {
                  snprintf(instr, DISP_LEN-1, "%s %d %0.2f", a_str, param, val);
                  display_str(instr, IN);  
                  display_val(val);
               }
            }
            else if (cmd == 0x03 && sub_cmd == 0x06) {
               // it can only be an amp change if received from the Spark
               ret = scr.get_effect_change(i, a_str, b_str);
               Serial.print("Get effect change ");
               Serial.println(ret);
               if (ret >= 0) {
            
                  snprintf(instr, DISP_LEN-1, "-> %s", b_str);
                  display_str(instr, IN);
            
                  if      (!strncmp(b_str, "FatAcousticV2", STR_LEN-1)) pres = 16;
                  else if (!strncmp(b_str, "GK800", STR_LEN-1)) pres = 17;
                  else if (!strncmp(b_str, "Twin", STR_LEN-1)) pres = 3;
                  else if (!strncmp(b_str, "TwoStoneSP50", STR_LEN-1)) pres = 12;
                  else if (!strncmp(b_str, "OverDrivenJM45", STR_LEN-1)) pres = 5; 
                  else if (!strncmp(b_str, "AmericanHighGain", STR_LEN-1)) pres = 22;
                  else if (!strncmp(b_str, "EVH", STR_LEN-1)) pres = 7;
                                               
                  sc2.create_preset(*presets[pres]);
                  send_receive_bt(sc2);
                  send_receive_bt(sc_setpreset7f);

                  snprintf(outstr, DISP_LEN-1, "Preset: %s", presets[pres]->Name);
                  display_str(outstr, OUT);

//                  send_preset_request(0x7f);
               }
            }
//            else {
//               snprintf(instr, DISP_LEN-1, "Command %2X %2X", scr.messages[i].cmd, scr.messages[i].sub_cmd);
//               display_str(instr, IN);
//            }
         }
      }
   }
}
