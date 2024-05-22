#include <FirebaseESP32.h>
#include <ArduinoJson.h>
#include <HX711_ADC.h>
#include <WiFi.h>
#include <time.h>

using namespace std;

// Definitions for WiFi
const char *WIFI_SSID = "WIFIWIFIWIFI";
const char *WIFI_PASSWORD = "LorJun21";

// Definitions for Firebase
const char *FIREBASE_HOST = "https://petness-92c55-default-rtdb.asia-southeast1.firebasedatabase.app/"; // Replace with your Firebase Realtime Database URL
const char *FIREBASE_AUTH = "bhvzGLuvbjReHlQjk77UwWGtCVdBBUBABE3X4PQ2";                                 // Replace with your Firebase Database secret

// Definitions for NTP
const char *NTP_SERVER = "2.asia.pool.ntp.org";
const long GMT_OFFSET_SEC = 28800;
const int DAYLIGHT_OFFSET_SEC = 0;

// Definitions for HX711
const int HX711_dout = 13;
const int HX711_sck = 27;
HX711_ADC LoadCell(HX711_dout, HX711_sck);

// Declare the FirebaseData objects at the global scope
FirebaseData fbdo1;
FirebaseData fbdo2;
FirebaseConfig config;
FirebaseAuth auth;

// Declare the paths at the global scope
String pathGetPetWeight = "/trigger/getPetWeight/status";
String pathAmountToDispense = "/petFeedingSchedule";

void petWeightTare()
{
  LoadCell.begin();
  long stabilizingtime = 2000; // tare preciscion can be improved by adding a few seconds of stabilizing time
  LoadCell.start(stabilizingtime);
  if (LoadCell.getTareTimeoutFlag())
  {
    Serial.println("Timeout, check MCU>HX711 wiring and pin designations");
    while (1)
      ;
  }
  else
  {
    LoadCell.setCalFactor(-20.84); // calibration value, you should get this from calibration sketch
    Serial.println("Startup + tare is complete");
  }
}

float samplesForGettingWeight()
{
  // TEMP Variables in Getting Pets Weight
  float totalWeight = 0;    // Variable to store the total weight
  int sampleCount = 0;      // Counter for the number of samples collected
  int numIgnoreSamples = 5; // Number of samples to ignore
  int numSamples = 10;      // Total number of samples to collect

  // IGNORING the numIgnoreSamples
  for (int i = 0; i < numIgnoreSamples; i++)
  {
    bool newDataReady = false; // Flag to indicate new data

    while (!newDataReady)
    {
      if (LoadCell.update())
        newDataReady = true;
    }

    if (newDataReady)
    {
      float weightSample = LoadCell.getData(); // Get the weight sample
      Serial.print("Ignoring sample: ");
      Serial.println(weightSample);
    }

    delay(200);
  }

  // COLLECTING the needed numSamples
  for (int i = 0; i < numSamples - numIgnoreSamples; i++)
  {
    bool newDataReady = false; // Flag to indicate new data

    while (!newDataReady)
    {
      if (LoadCell.update())
        newDataReady = true;
    }

    if (newDataReady)
    {
      float weightSample = LoadCell.getData(); // Get the weight sample
      totalWeight += weightSample;             // Add the weight to the total
      sampleCount++;                           // Increment the sample count
      Serial.print("Load_cell output val: ");
      Serial.println(weightSample);
    }

    delay(500); // Delay for 1 second between samples
  }

  // CALCULATING the Average of Collected NumSamples
  float petsWeight = totalWeight / sampleCount;
  // Convert weight to Kilograms
  // float petsWeightKg = petsWeight / 1000.0;
  Serial.print("PETS WEIGHT:  ");
  Serial.println(petsWeight);

  return petsWeight;
}

void setPetWeight()
{
  float petsWeight = samplesForGettingWeight();

  // Send weight to Firebase
  String path = "/getWeight/loadCell/weight";
  if (Firebase.setFloat(fbdo1, path.c_str(), petsWeight))
  {
    Serial.println("Weight data updated successfully.");
  }
  else
  {
    Serial.print("Failed to update weight data, reason: ");
    Serial.println(fbdo1.errorReason());
  }

  // Set the trigger to false
  path = "/trigger/getPetWeight/status";
  if (Firebase.setBool(fbdo1, path.c_str(), false))
  {
    Serial.println("Trigger status updated successfully.");
  }
  else
  {
    Serial.print("Failed to update trigger status, reason: ");
    Serial.println(fbdo1.errorReason());
  }
}

void petWeightStream()
{

  if (Firebase.readStream(fbdo1))
  {
    if (fbdo1.streamTimeout())
    {
      Serial.println("Stream timeout, no data received from Firebase");
    }
    else if (fbdo1.dataType() == "boolean")
    {
      bool status = fbdo1.boolData();
      Serial.print("Status of Weight Stream: ");
      Serial.println(String(status).c_str());
      if (status)
      {
        setPetWeight();
      }
    }
  }
  else
  {
    Serial.print("Failed to read stream, reason: ");
    Serial.println(fbdo1.errorReason());
  }
}

String getCurrentDate()
{
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo))
  {
    Serial.println("Failed to obtain Current Date");
    return "";
  }
  char dateStr[11]; // "YYYY-MM-DD\0"
  strftime(dateStr, sizeof(dateStr), "%Y-%m-%d", &timeinfo);
  return String(dateStr);
}

String getCurrentTime()
{
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo))
  {
    Serial.println("Failed to obtain Hour & Minute time");
    return "";
  }
  char timeStr[6]; // "HH:MM\0"
  strftime(timeStr, sizeof(timeStr), "%H:%M", &timeinfo);
  return String(timeStr);
}

int getCurrentHour()
{
  struct tm timeinfo;
  int retries = 10;
  while (!getLocalTime(&timeinfo))
  {
    Serial.println("Failed to obtain time, retrying...");
    delay(1000); // wait for a second
    retries--;
    if (retries == 0)
    {
      Serial.println("Failed to obtain time");
      return -1;
    }
  }
  return timeinfo.tm_hour;
}

void smartDispenseFood(float totalAmount, int servings)
{
  Serial.println("smartDispenseFood function called");
  // Calculate the amount per serving
  float amountPerServing = totalAmount / servings;

  // Get the current time
  int currentHour = getCurrentHour();

  // Determine when to dispense the food based on the number of servings
  switch (servings)
  {
  case 1:
    if (currentHour == 12)
    {
      Serial.print("Dispensing ");
      Serial.print(amountPerServing);
      Serial.println(" at 12pm");
    }
    else
    {
      Serial.print("Current Hour: ");
      Serial.println(currentHour);
    }
    break;
  case 2:
    if (currentHour == 12 || currentHour == 18)
    {
      Serial.print("Dispensing ");
      Serial.print(amountPerServing);
      Serial.print(" at ");
      Serial.print(currentHour);
      Serial.println("pm");
    }
    else
    {
      Serial.print("Current Hour: ");
      Serial.println(currentHour);
    }
    break;
  case 3:
    if (currentHour == 8 || currentHour == 15 || currentHour == 20)
    {
      Serial.print("Dispensing ");
      Serial.print(amountPerServing);
      Serial.print(" at ");
      Serial.print(currentHour);
      Serial.println("pm");
    }
    else
    {
      Serial.print("Current Hour: ");
      Serial.println(currentHour);
    }
    break;
  default:
    Serial.println("Invalid number of servings");
    break;
  }
}

void scheduledDispenseFood(float amountToDispense, String scheduledDate, String scheduledTime)
{
  Serial.println("scheduledDispenseFood function called");
  // Convert amountToDispense to float
  float amount = amountToDispense;

  // Get the current date and time
  String currentDate = getCurrentDate(); // You need to implement this function
  String currentTime = getCurrentTime(); // You need to implement this function
  // Check if it's the scheduled date and time
  if (currentDate == scheduledDate && currentTime == scheduledTime)
  {
    Serial.print("Dispensing ");
    Serial.print(amount);
    Serial.print(" at ");
    Serial.print(scheduledDate);
    Serial.println(scheduledTime);
  }
  else
  {
    Serial.println("Not the scheduled date and time");
  }
}

void feedingStream()
{
  FirebaseData fbdo2;

  if (!Firebase.get(fbdo2, pathAmountToDispense))
  {
    Serial.print("Failed to get data, reason: ");
    Serial.println(fbdo2.errorReason());
    return;
  }

  if (fbdo2.dataType() == "json")
  {
    FirebaseJson *json = fbdo2.jsonObjectPtr();
    String jsonString;
    json->toString(jsonString);

    DynamicJsonDocument doc(1024);
    deserializeJson(doc, jsonString);

    for (JsonPair kv : doc.as<JsonObject>())
    {
      String userName = kv.key().c_str();
      JsonObject userObject = kv.value().as<JsonObject>();

      // FOR SMART FEEDING
      if (userObject.containsKey("smartFeeding"))
      {
        JsonObject smartFeedingObject = userObject["smartFeeding"].as<JsonObject>();
        bool smartFeedingStatus = smartFeedingObject["smartFeedingStatus"].as<bool>();

        if (smartFeedingStatus)
        {
          for (JsonPair kv2 : smartFeedingObject)
          {
            if (kv2.value().is<JsonObject>())
            {
              JsonObject arrayObject = kv2.value().as<JsonObject>();
              String amountToDispense = arrayObject["amountToDispensePerServingPerDay"].as<String>();
              float totalAmount = amountToDispense.toFloat();
              int servings = arrayObject["servings"].as<int>();

              // Pass the amountPerServing and servings to the dispensing mechanism
              smartDispenseFood(totalAmount, servings);
            }
          }
        }
      }

      // FOR SCHEDULED FEEDING
      if (userObject.containsKey("scheduledFeeding"))
      {
        JsonObject scheduledFeedingObject = userObject["scheduledFeeding"].as<JsonObject>();
        bool scheduledFeedingStatus = scheduledFeedingObject["scheduledFeedingStatus"].as<bool>();

        if (scheduledFeedingStatus)
        {
          for (JsonPair kv2 : scheduledFeedingObject)
          {
            if (kv2.value().is<JsonObject>())
            {
              JsonObject arrayObject = kv2.value().as<JsonObject>();
              float amountToFeed = arrayObject["amountToFeed"].as<float>();
              String scheduledDate = arrayObject["scheduledDate"].as<String>();
              String scheduledTime = arrayObject["scheduledTime"].as<String>();

              // Pass the amountToFeed, scheduledDate, and scheduledTime to the dispensing mechanism
              scheduledDispenseFood(amountToFeed, scheduledDate, scheduledTime);
            }
          }
        }
      }
    }
  }
}

void setup()
{
  Serial.begin(115200);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(300);
  }
  Serial.println("Connected to WiFi");

  config.host = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH; // Set your Firebase RTDB secret here
  Firebase.begin(&config, &auth);

  // Init and get the time
  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);

  // Call the function to tare the load cell
  petWeightTare();

  // Set up stream for getPetWeight
  Firebase.beginStream(fbdo1, pathGetPetWeight);

  // Set up stream for smartFeeding
  Firebase.beginStream(fbdo2, pathAmountToDispense);
}
unsigned long lastPetWeightStreamTime = 0;
unsigned long lastFeedingStreamTime = 0;

void loop()
{
  unsigned long currentMillis = millis();

  if (currentMillis - lastPetWeightStreamTime >= 1000)
  {
    petWeightStream();
    lastPetWeightStreamTime = currentMillis;
  }

  if (currentMillis - lastFeedingStreamTime >= 3000)
  {
    feedingStream();
    lastFeedingStreamTime = currentMillis;
  }
}