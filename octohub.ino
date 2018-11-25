#define MQTT_MAX_PACKET_SIZE 1024

#include <ESPHelper.h>
#include <OctoPrintAPI.h>
#include <WiFiClient.h>
#include <SerialCommands.h>
#include <RemoteDebug.h>

#include "config.h"


#define MAX_FILENAME_LEN 256
#define MAX_FILE_COUNT 100
#define DEBOUNCE 1000

typedef struct {
    char* name;
    short success;
} file_entry_t;

typedef struct {
    char* directories[MAX_FILE_COUNT];
    file_entry_t files[MAX_FILE_COUNT];
    uint dir_count;
    uint file_count;
} folder_t;


ESPHelper myEsp(&homeNet);
WiFiClient client;
unsigned long debounceTimer;

OctoprintApi api(client, octoprintIp, 80, octoprintApikey);

RemoteDebug Debug;

char serialCommandBuffer[64];
SerialCommands serialCommands(&Serial, serialCommandBuffer, sizeof(serialCommandBuffer), "\n", " ");

float temperatureToolActual = -1;
float temperatureToolTarget = -1;
float temperatureBedActual = -1;
float temperatureBedTarget = -1;

char c_folder[MAX_FILENAME_LEN] = "";
folder_t c_folder_content;

bool readAPIFolderContent(String& input) {

    DynamicJsonBuffer jsonFolderBuffer(1024);
    JsonObject& root = jsonFolderBuffer.parseObject(input);

    if (!root.success()) {
        Debug.println("JSON parsing failed!");
        root.printTo(Debug);
        Debug.println("");
        return false;
    }

  /*  root.printTo(Debug);
    Debug.println("");
    jsonFolderRoot = &root;
    jsonFolderRoot->printTo(Debug);
    Debug.println("");*/

    // clean up old folder data
    c_folder_content.dir_count = 0;
    c_folder_content.file_count = 0;
    for (int i=0; i<MAX_FILE_COUNT; i++) {
        if (c_folder_content.directories[i] != NULL) {
            free(c_folder_content.directories[i]);
            c_folder_content.directories[i] = NULL;
        }
        if (c_folder_content.files[i].name != NULL) {
            free(c_folder_content.files[i].name);
            c_folder_content.files[i].name = NULL;
        }
        c_folder_content.files[i].success = 0;
    }

    // build new folder data

    c_folder_content.dir_count = (root["directories"].size()<=MAX_FILE_COUNT)?root["directories"].size():MAX_FILE_COUNT;
    c_folder_content.file_count = (root["files"].size()<=MAX_FILE_COUNT)?root["files"].size():MAX_FILE_COUNT;

    for (int i=0; i < c_folder_content.dir_count; i++) {
        const char* dir = root["directories"][i];
        c_folder_content.directories[i] = (char*) malloc(strlen(dir)+1);
        strcpy(c_folder_content.directories[i],dir);
    }
    for (int i=0; i < c_folder_content.file_count; i++) {
        const char* file = root["files"][i][0];
        const char* succ = root["files"][i][1];
        c_folder_content.files[i].name = (char*) malloc(strlen(file)+1);
        strcpy(c_folder_content.files[i].name,file);
        if (strcmp(succ,"succ") == 0) {
            c_folder_content.files[i].success = 1;
        } else if (strcmp(succ,"err") == 0) {
            c_folder_content.files[i].success = -1;
        } else c_folder_content.files[i].success = 0;
    }

    strncpy(c_folder,root["path"],MAX_FILENAME_LEN);
    return true;
}

/*************************************************
 *
 * GCode commands (received via HW Serial)
 *
 **************************************************/

void cmdJOG_X(SerialCommands* sender) {
    Debug.println("JOG_X");
    char *param = sender->Next();
    double val = 0;
    if (param != NULL) {
        Debug.print("Param: ");
        Debug.println(param);

        val = atof(param);
        Debug.print("val = "); Debug.println(val);
        api.octoPrintPrintHeadRelativeJog(val,0,0,1500);
    }

    sender->GetSerial()->println("ok");
}

void cmdJOG_Y(SerialCommands* sender) {
    Debug.println("JOG_Y");
    char *param = sender->Next();
    double val = 0;
    if (param != NULL) {
        Debug.print("Param: ");
        Debug.println(param);

        val = atof(param);
        Debug.print("val = "); Debug.println(val);
        api.octoPrintPrintHeadRelativeJog(0,val,0,1500);
    }

    sender->GetSerial()->println("ok");
}

void cmdJOG_Z(SerialCommands* sender) {
    Debug.println("JOG_Z");
    char *param = sender->Next();
    double val = 0;
    if (param != NULL) {
        Debug.print("Param: ");
        Debug.println(param);

        val = atof(param);
        Debug.print("val = "); Debug.println(val);
        api.octoPrintPrintHeadRelativeJog(0,0,val/2,1500);
    }

    sender->GetSerial()->println("ok");
}

void cmdHOME(SerialCommands* sender) {
    Debug.println("HOME");
    api.octoPrintPrintHeadHome();
}

void cmdLOAD(SerialCommands* sender) {
    unsigned long now = millis();
    if (abs(now - debounceTimer) > DEBOUNCE) {
        Debug.println("LOAD");
        api.octoPrintPrinterCommand("LOAD");
        debounceTimer = now;
    }
}

void cmdUNLOAD(SerialCommands* sender) {
    unsigned long now = millis();
    if (abs(now - debounceTimer) > DEBOUNCE) {
        Debug.println("UNLOAD");
        api.octoPrintPrinterCommand("UNLOAD");
        debounceTimer = now;
    }
}

void cmdEXTRUDE(SerialCommands* sender) {
    Debug.println("EXTRUDE");
    char *param = sender->Next();
    double val = 0;
    if (param != NULL) {
        Debug.print("Param: ");
        Debug.println(param);

        val = atof(param);
        Debug.print("val = "); Debug.println(val);
        api.octoPrintExtrude(val);
    }

    sender->GetSerial()->println("ok");
}

void cmdSET_BED_TEMP(SerialCommands* sender) {
    Debug.println("SET_BED_TEMP");
    char *param = sender->Next();
    uint16_t val = 0;
    if (param != NULL) {
        Debug.print("Param: ");
        Debug.println(param);

        val = atoi(param);
        Debug.print("val = "); Debug.println(val);
        api.octoPrintSetBedTemperature(val);
    }

    sender->GetSerial()->println("ok");
}

void cmdSET_TOOL_TEMP(SerialCommands* sender) {
    Debug.println("SET_TOOL_TEMP");
    char *param = sender->Next();
    uint16_t val = 0;
    if (param != NULL) {
        Debug.print("Param: ");
        Debug.println(param);

        val = atoi(param);
        Debug.print("val = "); Debug.println(val);
        api.octoPrintSetTool0Temperature(val);
    }
    Debug.println("SET_TOOL_TEMP done");
    sender->GetSerial()->println("ok");
}

void sendFolder(char* folder, int entries) {
    char buf[20+MAX_FILENAME_LEN];
    snprintf(buf,20+MAX_FILENAME_LEN,"FOLDER %s\nFOLDER_ENTRY_COUNT %d\n", folder, entries);
    Serial.print(buf);
}

void setFolder(char* path) {
  String path_str = String((const char*)path);
  String api_path = String("plugin/octoscreen_plugin?path=");
  String result = api.getOctoprintEndpointResults(api_path + path_str);
  if (readAPIFolderContent(result)) {
    sendFolder(c_folder, c_folder_content.dir_count+c_folder_content.file_count);
  }
}

void cmdSET_FOLDER(SerialCommands* sender) {
  Debug.println("SET_FOLDER");
  char* param = sender->Next();
  if (param != NULL) {
    setFolder(param);
    sender->GetSerial()->println("ok");
  }
}

void sendFolderEntry(int no, char* folder_entry) {
    char buf[128];
    snprintf(buf,128,"FOLDER_ENTRY %d %s\n", no, folder_entry);
    Serial.print(buf);
}


void cmdGET_FOLDER_ENTRIES(SerialCommands* sender) {
  Debug.println("GET_FOLDER_ENTRIES");
  char* param1 = sender->Next();
  char* param2 = sender->Next();
  if ((param1 != NULL) && (param2 != NULL)) {
    int no_from = atoi(param1);
    int no_to = atoi(param2);

    if ((no_from >= 0) && (no_from < MAX_FILE_COUNT) && (no_to >= no_from) && (no_from < MAX_FILE_COUNT)) {
      for (int no=no_from; no<=no_to; no++) {
        if (no < c_folder_content.dir_count) {
          char buf[MAX_FILENAME_LEN+10];
          snprintf(buf, MAX_FILENAME_LEN+10, "<%s>", c_folder_content.directories[no]);
          sendFolderEntry(no, buf);
        } else if (no < c_folder_content.dir_count + c_folder_content.file_count) {
          char buf[MAX_FILENAME_LEN+10];
          snprintf(buf, MAX_FILENAME_LEN+10, "%s", c_folder_content.files[no-c_folder_content.dir_count]);
          sendFolderEntry(no, buf);
        }
      }
    }
  }
  sender->GetSerial()->println("ok");
}

void cmdSEL_FOLDER_ENTRY(SerialCommands* sender) {
  Debug.println("SEL_FOLDER_ENTRY");
  char* param = sender->Next();
  int no = atoi(param);
  if ((no >= 0) && (no < MAX_FILE_COUNT)) {
    if (no < c_folder_content.dir_count) {
      if (!strcmp(c_folder_content.directories[no], "..")) {
        c_folder[strlen(c_folder)-1] = '\0';
        char* pos = strrchr(c_folder, '/');
        if (pos != NULL) {
          pos[1] = '\0';
          setFolder(c_folder);
        }
      } else {
        char buf[MAX_FILENAME_LEN*2+10];
        snprintf(buf, MAX_FILENAME_LEN*2+10, "%s%s/", c_folder, c_folder_content.directories[no]);
        setFolder(buf);
      }
    } else if (no < c_folder_content.dir_count + c_folder_content.file_count) {
      char buf[MAX_FILENAME_LEN*2+10];
      snprintf(buf, MAX_FILENAME_LEN*2+10, "%s%s", c_folder, c_folder_content.files[no-c_folder_content.dir_count]);
      String filename = String(buf);
      api.octoPrintFileSelect(filename);
    }
  }
  sender->GetSerial()->println("ok");
}

void sendInit() {
  setFolder("/");
  if(api.getPrinterStatistics()) {
      sendStatus(api.printerStats.printerState.c_str());
      temperatureBedActual = api.printerStats.printerBedTempActual;
      temperatureBedTarget = api.printerStats.printerBedTempTarget;
      temperatureToolActual = api.printerStats.printerTool0TempActual;
      temperatureToolTarget = api.printerStats.printerTool0TempTarget;
      sendBedTemperatures(temperatureBedActual, temperatureBedTarget);
      sendToolTemperatures(temperatureToolActual, temperatureToolTarget);
  }
  if (api.getPrintJob()) {
    if (api.printJob.jobFileName != NULL) {
      sendJob(api.printJob.jobFileName.c_str());
    }
  }
}

void cmdINIT(SerialCommands* sender) {
  sendInit();

  sender->GetSerial()->println("ok");

}

void cmdPAUSE(SerialCommands* sender) {
  api.octoPrintJobPause();
  sender->GetSerial()->println("ok");
}

void cmdRESUME(SerialCommands* sender) {
  api.octoPrintJobResume();
  sender->GetSerial()->println("ok");
}

void cmdPRINT(SerialCommands* sender) {
  api.octoPrintJobStart();
  sender->GetSerial()->println("ok");
}

void cmdCANCEL(SerialCommands* sender) {
  api.octoPrintJobCancel();
  sender->GetSerial()->println("ok");
}





SerialCommand *gcodeCommandList[] =
    {   new SerialCommand("JOG_X", cmdJOG_X),
        new SerialCommand("JOG_Y", cmdJOG_Y),
        new SerialCommand("JOG_Z", cmdJOG_Z),
        new SerialCommand("HOME", cmdHOME),
        new SerialCommand("LOAD", cmdLOAD),
        new SerialCommand("UNLOAD", cmdUNLOAD),
        new SerialCommand("EXTRUDE", cmdEXTRUDE),
        new SerialCommand("SET_BED_TEMP", cmdSET_BED_TEMP),
        new SerialCommand("SET_TOOL_TEMP", cmdSET_TOOL_TEMP),
        new SerialCommand("SET_FOLDER", cmdSET_FOLDER),
        new SerialCommand("GET_FOLDER_ENTRIES", cmdGET_FOLDER_ENTRIES),
        new SerialCommand("SEL_FOLDER_ENTRY", cmdSEL_FOLDER_ENTRY),
        new SerialCommand("PRINT", cmdPRINT),
        new SerialCommand("PAUSE", cmdPAUSE),
        new SerialCommand("RESUME", cmdRESUME),
        new SerialCommand("CANCEL", cmdCANCEL),
        new SerialCommand("INIT", cmdINIT),
        NULL };


void cmdUnknown(SerialCommands* sender, const char* cmd)
{
    sender->GetSerial()->print("ERROR: Unrecognized command [");
    sender->GetSerial()->print(cmd);
    sender->GetSerial()->println("]");
}

void sendBedTemperatures(float act, float tar) {
    char buf[128];
    snprintf(buf,128,"TEMP_BED_ACT %0.1f\nTEMP_BED_TAR %0.1f\n", act, tar);
    Serial.print(buf);
}

void sendToolTemperatures(float act, float tar) {
    char buf[128];
    snprintf(buf,128,"TEMP_TOOL_ACT %0.1f\nTEMP_TOOL_TAR %0.1f\n", act, tar);
    Serial.print(buf);
}

void sendZHeight(float z) {
    char buf[128];
    snprintf(buf,128,"Z %0.2f\n", z);
    Serial.print(buf);
}

void sendProgress(float p) {
    char buf[128];
    snprintf(buf,128,"PROGRESS %0.2f\n", p);
    Serial.print(buf);
}

void sendPrintTime(uint16_t t) {
    char buf[128];
    snprintf(buf,128,"TIME %d\n", t);
    Serial.print(buf);
}

void sendPrintTimeLeft(uint16_t t) {
    char buf[128];
    snprintf(buf,128,"TIME_LEFT %d\n", t);
    Serial.print(buf);
}

void sendJob(const char* name) {
    char buf[300];
    snprintf(buf,300,"JOB %s\n", name);
    Serial.print(buf);
}

void sendStatus(const char* status) {
  char buf[128];
  snprintf(buf,128,"STATUS %s\n", status);
  Serial.print(buf);
}

void mqttCallback(char* topic, uint8_t* payload, unsigned int length) {
    const size_t BUFFER_SIZE = 1024;
    char buf[BUFFER_SIZE+1];

    // Allocate a temporary memory pool
    DynamicJsonBuffer jsonBuffer(BUFFER_SIZE);

    if ((!strcmp(topic, MQTT_ROOT "/temperature/bed")) && (length < BUFFER_SIZE)) {
        memcpy(buf, payload, length);
        buf[length] = '\0';

        JsonObject& root = jsonBuffer.parseObject(buf);
        if (!root.success()) {
            Debug.println("JSON parsing failed!");
            return;
        }

        temperatureBedActual = (root["actual"])?(float)(root["actual"]):0;
        temperatureBedTarget = (root["target"])?(float)(root["target"]):0;
        sendBedTemperatures(temperatureBedActual, temperatureBedTarget);
    } else if ((!strcmp(topic, MQTT_ROOT "/temperature/tool0")) && (length < BUFFER_SIZE)) {
        memcpy(buf, payload, length);
        buf[length] = '\0';

        JsonObject& root = jsonBuffer.parseObject(buf);
        if (!root.success()) {
            Debug.println("JSON parsing failed!");
            return;
        }

        temperatureToolActual = (root["actual"])?(float)(root["actual"]):0;
        temperatureToolTarget = (root["target"])?(float)(root["target"]):0;
        sendToolTemperatures(temperatureToolActual, temperatureToolTarget);
    }  else if ((!strcmp(topic, MQTT_ROOT "/event/ZChange")) && (length < BUFFER_SIZE)) {
        memcpy(buf, payload, length);
        buf[length] = '\0';

        JsonObject& root = jsonBuffer.parseObject(buf);
        if (!root.success()) {
            Debug.println("JSON parsing failed!");
            return;
        }

        if ((float)root["new"] > 0.0) {
          float newZ = (float)(root["new"]);
          sendZHeight(newZ);
        }
    } else if ((!strcmp(topic, "octoscreen/estimate")) && (length < BUFFER_SIZE)) {
        // { "progress": 15.2109997241, "printtime": 60, "printtimeleft": 3509 }
        memcpy(buf, payload, length);
        buf[length] = '\0';

        JsonObject& root = jsonBuffer.parseObject(buf);
        if (!root.success()) {
            Debug.println("JSON parsing failed!");
            return;
        }

        sendProgress((float)root["progress"]);
        sendPrintTime((uint16_t)root["printtime"]);
        sendPrintTimeLeft((uint16_t)root["printtimeleft"]);
    } else if ((!strcmp(topic, MQTT_ROOT "/event/FileSelected")) && (length < BUFFER_SIZE)) {
        memcpy(buf, payload, length);
        buf[length] = '\0';

        JsonObject& root = jsonBuffer.parseObject(buf);
        if (!root.success()) {
            Debug.println("JSON parsing failed!");
            return;
        }

        if ((const char*)root["name"] != NULL) {
          sendJob((const char*)root["name"]);
        }
    } else if ((!strcmp(topic, MQTT_ROOT "/event/PrinterStateChanged")) && (length < BUFFER_SIZE)) {
        memcpy(buf, payload, length);
        buf[length] = '\0';

        JsonObject& root = jsonBuffer.parseObject(buf);
        if (!root.success()) {
            Debug.println("JSON parsing failed!");
            return;
        }
        if ((const char*)root["state_string"] != NULL) {
          sendStatus((const char*)root["state_string"]);
        }
    } else {
        Debug.print("Unhandled MQTT event, topic: ");
        Debug.println(topic);
    }
}

void wifiCallback() {
    sendInit();
}


void unsubscribe() {
    myEsp.removeSubscription(MQTT_ROOT "/temperature/#");
    myEsp.removeSubscription(MQTT_ROOT "/event/#");
    myEsp.removeSubscription("octoscreen/#");
}


void setup() {
    Debug.begin("octohub"); // Initiaze the telnet server

    Serial.begin(38400, SERIAL_8N1);	//start the serial line
    delay(500);
    Debug.println("Starting Up, Please Wait...");

    myEsp.OTA_enable();
    myEsp.OTA_setHostname("octohub");

    myEsp.addSubscription(MQTT_ROOT "/temperature/#");
    myEsp.addSubscription(MQTT_ROOT "/event/#");
    myEsp.addSubscription("octoscreen/#");

    myEsp.setMQTTCallback(mqttCallback);
    myEsp.setWifiCallback(wifiCallback);

    myEsp.begin();

    for (int i=0;;i++) {
        if (gcodeCommandList[i] == NULL) {
            break;
        } else {
            serialCommands.AddCommand(gcodeCommandList[i]);
        }
    }

    serialCommands.SetDefaultHandler(&cmdUnknown);

    c_folder_content.dir_count = 0;
    c_folder_content.file_count = 0;
    for (int i=0; i<MAX_FILE_COUNT; i++) {
        c_folder_content.directories[i] = NULL;
        c_folder_content.files[i].name = NULL;
        c_folder_content.files[i].success = 0;
    }

    Debug.println("Initialization Finished.");
}



void loop() {
    myEsp.loop();
    serialCommands.ReadSerial();
    Debug.handle();
}
