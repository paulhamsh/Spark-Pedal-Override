#include <M5StickC.h>  
#include "SparkClass.h"
#include "SparkPresets.h"

#include "BluetoothSerial.h" // https://github.com/espressif/arduino-esp32

// Bluetooth vars
#define SPARK_NAME "Spark 40 Audio"
#define MY_NAME    "MIDI Spark"

#define BACKGROUND TFT_BLACK
#define TEXT_COLOUR TFT_WHITE

#define STATUS 0
#define IN     15
#define OUT    52

#define STD_HEIGHT 10


BluetoothSerial SerialBT;


// Spark vars
SparkClass sc2, scr;
SparkClass sc_setpreset7f;
SparkClass sc_getserial;

SparkPreset preset;


// ------------------------------------------------------------------------------------------
// Display routintes

// Display vars
#define DISP_LEN 50
char outstr[DISP_LEN+1];
char instr[DISP_LEN+1];
char statstr[DISP_LEN+1];
char titlestr[DISP_LEN+1];

int bar_pos;
unsigned long bar_count;


void display_val(float val)
{
   int dist;

   dist = uint8_t(val * 150);
   M5.Lcd.fillRoundRect(5 + dist, IN + STD_HEIGHT + 10 , 150 - dist, 15, 4, BACKGROUND);
   M5.Lcd.drawRoundRect(5, IN + STD_HEIGHT + 10 , 150 , 15, 4, TEXT_COLOUR);
   M5.Lcd.fillRoundRect(5, IN + STD_HEIGHT + 10 , dist, 15, 4, TEXT_COLOUR);
}

void display_str()
{
   char i_str[DISP_LEN], o_str[DISP_LEN], s_str[DISP_LEN];

   strncpy(i_str, instr, 25);
   if (strlen(i_str) < 25) strncat(i_str, "                         ", 25 - strlen(i_str));
   M5.Lcd.drawString(i_str,    0, IN,    2);
   
   strncpy(o_str, outstr, 25);
   if (strlen(o_str) < 25) strncat(o_str, "                         ", 25 - strlen(o_str));   
   M5.Lcd.drawString(o_str,   0, OUT,    2);
   
   strncpy(s_str, statstr, 25);
   if (strlen(s_str) < 25) strncat(s_str, "                         ", 25 - strlen(s_str));   
   M5.Lcd.drawString(s_str,  0, STATUS, 2);   
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
      strncpy( statstr, "Bluetooth init fail", 25);
      display_str();
      while (true);
   }    
   connected = false;
   bt_event = 0;
}


void connect_to_spark() {
   int rec;

   while (!connected) {
      strncpy (statstr, "Connecting to Spark", 25);
      display_str();
      connected = SerialBT.connect(SPARK_NAME);
      if (connected && SerialBT.hasClient()) {
         strncpy(statstr, "Connected to Spark", 25);
         display_str();
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
      if (rec <0) Serial.println("WAITING FOR ACK, GOT AN ERROR"); 
   }
      
   // and send the last one      
   SerialBT.write(spark_class.buf[spark_class.last_block], spark_class.last_pos+1); 
   
   // read the ACK
      rec = scr.get_data();
      if (rec <0) Serial.println("WAITING FOR ACK, GOT AN ERROR"); 
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
   M5.Lcd.setRotation(3);
   M5.Lcd.fillScreen(BLACK);
   
   strncpy(statstr,  "Started", 25);
   strncpy(outstr,   "Nothing out", 25);
   strncpy(instr,    "Nothing in", 25);
   strncpy(titlestr, "Spark Override", 25);
   display_str();

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

   
   // this will connect if not already connected
   if (!connected) connect_to_spark();

   // see if this keeps the connection alive
   if (millis() - keep_alive  > 10000) {
      keep_alive = millis();
      send_receive_bt(sc_getserial);
   }
   
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
               Serial.println("Get preset");
               Serial.println(ret);
               if (ret >= 0){
                  snprintf(instr, DISP_LEN-1, "Preset: %s", preset.Name);
                  display_str();
               }
            }
            else if (cmd == 0x03 && sub_cmd == 0x37) {
               ret = scr.get_effect_parameter(i, a_str, &param, &val);
               Serial.println("Get effect params");
               Serial.println(ret);
               if (ret >=0) {
                  snprintf(instr, DISP_LEN-1, "%s %d %0.2f", a_str, param, val);
                  display_str();  
                  display_val(val);
               }
            }
            else if (cmd == 0x03 && sub_cmd == 0x06) {
               // it can only be an amp change if received from the Spark
               ret = scr.get_effect_change(i, a_str, b_str);
               Serial.println("Get effect change");
               Serial.println(ret);
               if (ret >= 0) {
            
                  snprintf(instr, DISP_LEN-1, "-> %s", b_str);
                  display_str();
            
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
                  display_str();

//                  send_preset_request(0x7f);
               }
            }
//            else {
//               snprintf(instr, DISP_LEN-1, "Command %2X %2X", scr.messages[i].cmd, scr.messages[i].sub_cmd);

//            }
         }
      }
   }
}
