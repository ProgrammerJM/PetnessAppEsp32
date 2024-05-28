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

float getPetWeightRecord()
{
  float petsWeight = samplesForGettingWeight();
  return petsWeight;
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

// void sendPetRecordsToFirebase(String userName, JsonObject &petRecords)
// {
//   FirebaseData fbdo;

//   // Iterate over all keys in the JsonObject
//   for (JsonPair kv : petRecords)
//   {
//     // Print the key and value to the serial monitor for debugging
//     Serial.print("Key: ");
//     Serial.println(kv.key().c_str());

//     // Check the type of the value
//     if (kv.value().is<float>())
//     {
//       // If the value is a float, convert it to a string
//       Serial.print("Value: ");
//       Serial.println(kv.value().as<float>());
//       String value = String(kv.value().as<float>());

//       // Construct the path
//       String path = "/petFeedingSchedule/";
//       path.concat(userName);
//       path.concat("/petRecords/");
//       path.concat(kv.key().c_str());

//       // Send data to Firebase
//       if (!Firebase.set(fbdo, path, value))
//       {
//         Serial.print("Failed to send data, reason: ");
//         Serial.println(fbdo.errorReason());
//         return;
//       }
//     }
//     else if (kv.value().is<const char *>())
//     {
//       // If the value is a string, convert it to a const char *
//       Serial.print("Value: ");
//       Serial.println(kv.value().as<const char *>());

//       // Construct the path
//       String path = "/petFeedingSchedule/";
//       path.concat(userName);
//       path.concat("/petRecords/");
//       path.concat(kv.key().c_str());

//       // Send data to Firebase
//       if (!Firebase.set(fbdo, path, String(kv.value().as<const char *>())))
//       {
//         Serial.print("Failed to send data, reason: ");
//         Serial.println(fbdo.errorReason());
//         return;
//       }
//     }
//   }
// }

void sendPetRecordsToFirebase(String userName, JsonObject &petRecords)
{
  FirebaseData fbdo;

  // Iterate over all keys in the JsonObject
  for (JsonPair kv : petRecords)
  {
    // Print the key and value to the serial monitor for debugging
    Serial.print("Key: ");
    Serial.println(kv.key().c_str());

    // Construct the path
    String path = "/petFeedingSchedule/";
    path.concat(userName);
    path.concat("/petRecords/");
    path.concat(kv.key().c_str());

    // Check the type of the value and send it to Firebase
    if (kv.value().is<float>())
    {
      // If the value is a float, send it directly
      Serial.print("Value: ");
      Serial.println(kv.value().as<float>());

      if (!Firebase.set(fbdo, path, kv.value().as<float>()))
      {
        Serial.print("Failed to send data, reason: ");
        Serial.println(fbdo.errorReason());
        return;
      }
    }
    else if (kv.value().is<const char *>())
    {
      // If the value is a string, send it directly
      Serial.print("Value: ");
      Serial.println(kv.value().as<const char *>());

      if (!Firebase.set(fbdo, path, kv.value().as<const char *>()))
      {
        Serial.print("Failed to send data, reason: ");
        Serial.println(fbdo.errorReason());
        return;
      }
    }
  }
}

void scheduledDispenseFood(String userName, float amountToDispense, String scheduledDate, String scheduledTime)
{
  // flag to check if the function has run
  static bool hasRun = false;

  Serial.println("scheduledDispenseFood function called");
  // Convert amountToDispense to float
  float amount = amountToDispense;

  // Get the current date and time
  String currentDate = getCurrentDate(); // You need to implement this function
  String currentTime = getCurrentTime(); // You need to implement this function

  // Create a JsonObject for the pet record
  DynamicJsonDocument doc(1024);
  JsonObject petRecord = doc.to<JsonObject>();

  // Check if it's the scheduled date and time & the function has not run
  if (currentDate == scheduledDate && currentTime == scheduledTime && !hasRun)
  {
    Serial.print("Dispensing ");
    Serial.print(amount);
    Serial.print(" at ");
    Serial.print(scheduledDate);
    Serial.println(scheduledTime);

    // Add the dispensing date and time to the pet record
    petRecord["weight"] = getPetWeightRecord(); // Replace getPetWeight() with the actual function or value
    Serial.print("petRecord[\"weight\"] before sending: ");
    Serial.println(petRecord["weight"].as<float>());
    // Add the fields to the pet record
    petRecord["amountDispensed"] = amountToDispense;

    // Add the dispensing date and time to the pet record
    petRecord["dispensingDate"] = currentDate;
    petRecord["dispensingTime"] = currentTime;

    // Send the pet record to Firebase
    sendPetRecordsToFirebase(userName, petRecord);

    // Set the flag to true
    hasRun = true;
  }
  else if (currentDate != scheduledDate || currentTime != scheduledTime)
  {
    // If it's not the scheduled date and time, reset hasRun to false
    hasRun = false;
  }
  else
  {
    Serial.println("Not the scheduled date and time");
  }
}

void smartDispenseFood(String userName, float totalAmount, int servings)
{
  Serial.println("smartDispenseFood function called");
  // Calculate the amount per serving
  float amountPerServing = totalAmount / servings;

  // Get the current time
  int currentHour = getCurrentHour();

  // Create a JsonObject for the pet record
  DynamicJsonDocument doc(1024);
  JsonObject petRecord = doc.to<JsonObject>();

  // Determine when to dispense the food based on the number of servings
  switch (servings)
  {
  case 1:
    if (currentHour == 12)
    {
      Serial.print("Dispensing ");
      Serial.print(amountPerServing);
      Serial.println(" at 12pm");

      // Add the weight to the pet record
      petRecord["weight"] = getPetWeightRecord(); // Replace getPetWeight() with the actual function or value
      // Add the dispensing time to the pet record
      petRecord["feedingTime"] = "12pm";
      // Add the fields to the pet record
      petRecord["amountDispensed"] = totalAmount;
      petRecord["feedingFrequency"] = servings;

      // Send the pet record to Firebase
      sendPetRecordsToFirebase(userName, petRecord);
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

      // Add the weight to the pet record
      petRecord["weight"] = getPetWeightRecord(); // Replace getPetWeight() with the actual function or value
      // Add the fields to the pet record
      petRecord["amountDispensed"] = totalAmount;
      petRecord["feedingFrequency"] = servings;
      // Add the dispensing time to the pet record
      String dispensingTime = String(currentHour);
      dispensingTime.concat("pm");
      petRecord["dispensingTime"] = dispensingTime.c_str();

      // Send the pet record to Firebase
      sendPetRecordsToFirebase(userName, petRecord);
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

      // Add the weight to the pet record
      petRecord["weight"] = getPetWeightRecord(); // Replace getPetWeight() with the actual function or value
      // Add the fields to the pet record
      petRecord["amountDispensed"] = totalAmount;
      petRecord["feedingFrequency"] = servings;
      // Add the dispensing time to the pet record
      String dispensingTime = String(currentHour);
      dispensingTime.concat("pm");
      petRecord["dispensingTime"] = dispensingTime.c_str();

      // Send the pet record to Firebase
      sendPetRecordsToFirebase(userName, petRecord);
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
              smartDispenseFood(userName, totalAmount, servings);
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
              scheduledDispenseFood(userName, amountToFeed, scheduledDate, scheduledTime);
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