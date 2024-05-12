#include <FirebaseESP32.h>
#include <ArduinoJson.h>
#include <HX711_ADC.h>
#include <WiFi.h>

#define WIFI_SSID "virusX"
#define WIFI_PASSWORD "simacmacmagbabayad"
#define FIREBASE_HOST "https://petness-92c55-default-rtdb.asia-southeast1.firebasedatabase.app/" // Replace with your Firebase Realtime Database
#define FIREBASE_AUTH "bhvzGLuvbjReHlQjk77UwWGtCVdBBUBABE3X4PQ2"                                 // Replace with your Firebase Database secret

const int HX711_dout = 13;
const int HX711_sck = 27;
HX711_ADC LoadCell(HX711_dout, HX711_sck);

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
    LoadCell.setCalFactor(395); // calibration value, you should get this from calibration sketch
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
      Serial.print("Status: ");
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

void amountToDispenseStream()
{

  // Start Read Stream for the path petFeedingSchedule
  if (Firebase.readStream(fbdo2))
  {
    if (fbdo2.streamTimeout())
    {
      Serial.println("Stream timeout, no data received from Firebase");
    }
    else if (fbdo2.dataType() == "json")
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

        // Check if "smartFeeding" path exists
        if (userObject.containsKey("smartFeeding"))
        {
          // Get the array under "smartFeeding"
          JsonObject smartFeedingObject = userObject["smartFeeding"].as<JsonObject>();

          // Traverse the array
          for (JsonPair kv2 : smartFeedingObject)
          {
            String arrayKey = kv2.key().c_str();
            JsonObject arrayObject = kv2.value().as<JsonObject>();

            // Check the value of "amountToDispensePerServingPerDay" in the object
            if (arrayObject.containsKey("amountToDispensePerServingPerDay"))
            {
              String value = arrayObject["amountToDispensePerServingPerDay"].as<String>();
              Serial.println("smartFeeding data:");
              Serial.println(userName);
              Serial.println(value);
            }
          }
        }

        // Check if "scheduledFeeding" path exists
        if (userObject.containsKey("scheduledFeeding"))
        {
          // Get the array under "scheduledFeeding"
          JsonObject scheduledFeedingObject = userObject["scheduledFeeding"].as<JsonObject>();

          // Traverse the array
          for (JsonPair kv2 : scheduledFeedingObject)
          {
            String arrayKey = kv2.key().c_str();
            JsonObject arrayObject = kv2.value().as<JsonObject>();

            // Check the value of "amountToDispensePerServingPerDay" in the object
            if (arrayObject.containsKey("amountToDispensePerServingPerDay"))
            {
              String value = arrayObject["amountToDispensePerServingPerDay"].as<String>();
              Serial.println("scheduledFeeding data:");
              Serial.println(userName);
              Serial.println(value);
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

  // Call the function to tare the load cell
  petWeightTare();

  // Set up stream for getPetWeight
  Firebase.beginStream(fbdo1, pathGetPetWeight);

  // Set up stream for amountToDispense
  Firebase.beginStream(fbdo2, pathAmountToDispense);
}

void loop()
{

  petWeightStream();
  amountToDispenseStream();

  delay(1000);
}
