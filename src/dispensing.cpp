// #include <FirebaseESP32.h>

// extern FirebaseData fbdo;
// extern FirebaseConfig config;
// extern FirebaseAuth auth;
// extern FirebaseStream stream;



// void dispensingLoop() {
//   // Base path with wildcard for user name
//   String basePath = "/petFeedingSchedule/*";

//   // Check for users under "petFeedingSchedule" using `beginStream()`
//   if (Firebase.beginStream(fbdo, basePath, &stream)) { // Pass a reference to the global stream variable
//     Serial.println("Inner stream initialized successfully for dispensingLoop");
//     while (Firebase.available(stream)) {
//       if (Firebase.get(fbdo, stream.path())) {
//         // Check if data type is an object
//         if (fbdo.dataType() == "object") {
//           FirebaseJsonData smartFeedingData = jsonData.getJson(stream.path());

//           // Iterate through child nodes within "smartFeeding" (assuming it's an array)
//           FirebaseJsonDataIterator iter = smartFeedingData.iterator();
//           while (FirebaseJsonData::hasNext(iter)) {
//             FirebaseJsonData currentObject = FirebaseJsonData::getObject(iter);

//             // Access and print "amountToDispensePerServingPerDay" (modify if needed)
//             if (currentObject.get("amountToDispensePerServingPerDay")) {
//               Serial.println(stream.path());  // Print user path for context
//               Serial.println("smartFeeding data:");
//               Serial.println(currentObject.get("amountToDispensePerServingPerDay"));
//             } else {
//               Serial.println("amountToDispensePerServingPerDay not found in current object");
//             }

//             FirebaseJsonData::next(iter);
//           }
//         } else {
//           Serial.println("smartFeeding data is not an object");
//           fbdo.errorReason();
//         }
//       }
//     }
//   } else {
//     Serial.println("Error initializing stream for dispensingLoop:");
//     Serial.println(fbdo.errorReason());
//   }
// }