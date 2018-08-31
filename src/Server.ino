#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>

int current, last = 0;
// Replace with your network credentials
const char *ssid = "OPTUS_35FFAF";
const char *password = "tunictaxor58193";
IPAddress ip(192, 168, 0, 177);
IPAddress subnet(255, 255, 255, 0);
IPAddress gateway(192, 168, 0, 1);
#define THERMISTORPIN 35
#define LIGHTPIN 12
// Series resistor value
#define SERIESRESISTOR 10000

// Number of samples to average
#define SAMPLERATE 5
// Nominal resistance at 25C
#define THERMISTORNOMINAL 9000
// Nominal temperature in degrees
#define TEMPERATURENOMINAL 25
// Beta coefficient
#define BCOEFFICIENT 3380

// Emergency shutdown timeout
// If there is no temerature change after this timeout, shut everything off
#define EMERGENCYMILLIS 600000 // 10 minutes

// Time to wait before turning lamp on/off again
#define DEFAULTDEBOUNCE 100000

int targetTemp = 23;
int debounce = DEFAULTDEBOUNCE;
bool lightOn = false;
int currentTemp = 0;

// Timer used to check how long the lamp has been on for
// and initiate shutdown if timeout expires
int timer = 0;
bool timerError = false;
int lastTemp = 0;

WebServer server(80); //instantiate server at port 80 (http port)

int getTemp()
{
  double thermalSamples[SAMPLERATE];
  double average, kelvin, resistance, celsius;
  int i;

  // Collect SAMPLERATE (default 5) samples
  for (i = 0; i < SAMPLERATE; i++)
  {
    thermalSamples[i] = analogRead(THERMISTORPIN);
    delay(10);
  }

  // Calculate the average value of the samples
  average = 0;
  for (i = 0; i < SAMPLERATE; i++)
  {
    average += thermalSamples[i];
  }
  average /= SAMPLERATE;

  // Convert to resistance
  resistance = 4095 / average - 1;
  resistance = SERIESRESISTOR / resistance;

  /*
     * Use Steinhart equation (simplified B parameter equation) to convert resistance to kelvin
     * B param eq: T = 1/( 1/To + 1/B * ln(R/Ro) )
     * T  = Temperature in Kelvin
     * R  = Resistance measured
     * Ro = Resistance at nominal temperature
     * B  = Coefficent of the thermistor
     * To = Nominal temperature in kelvin
     */
  kelvin = resistance / THERMISTORNOMINAL;                 // R/Ro
  kelvin = log(kelvin);                                    // ln(R/Ro)
  kelvin = (1.0 / BCOEFFICIENT) * kelvin;                  // 1/B * ln(R/Ro)
  kelvin = (1.0 / (TEMPERATURENOMINAL + 273.15)) + kelvin; // 1/To + 1/B * ln(R/Ro)
  kelvin = 1.0 / kelvin;                                   // 1/( 1/To + 1/B * ln(R/Ro) )

  // Convert Kelvin to Celsius
  celsius = kelvin - 273.15;

  // Send the value back to be displayed
  return celsius;
}

void handleRoot()
{
  server.send(200, "text/html", String(page()));
}

void handleNotFound()
{
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++)
  {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

String page()
{
  return "<!DOCTYPE html>\n"
         "<html lang='en'>\n"
         "\n"
         "<head>\n"
         "  <meta charset='UTF-8'>\n"
         "  <meta name='viewport' content='width=device-width, initial-scale=1.0'>\n"
         "  <meta http-equiv='X-UA-Compatible' content='ie=edge'>\n"
         "  <title>Proving Box Monitor</title>\n"
         "  <link href='https://fonts.googleapis.com/css?family=Roboto' rel='stylesheet'>\n"
         "  <link rel='stylesheet' href='https://cdn.jsdelivr.net/npm/bulma-extensions@2.2.1/bulma-slider/dist/css/bulma-slider.min.css'>\n"
         "</head>\n"
         "\n"
         "<body>\n"
         "  <div class='light-icon'>\n"
         "    <svg xmlns='http://www.w3.org/2000/svg' xmlns:xlink='http://www.w3.org/1999/xlink' version='1.1' id='Capa_1' x='0px' y='0px'\n"
         "      viewBox='0 0 486 486' style='enable-background:new 0 0 486 486;' xml:space='preserve' width='512px' height='512px'>\n"
         "      <path class='light' id='XMLID_49_' d='M298.4,424.7v14.2c0,11.3-8.3,20.7-19.1,22.3l-3.5,12.9c-1.9,7-8.2,11.9-15.5,11.9h-34.7   c-7.3,0-13.6-4.9-15.5-11.9l-3.4-12.9c-10.9-1.7-19.2-11-19.2-22.4v-14.2c0-7.6,6.1-13.7,13.7-13.7h83.5   C292.3,411,298.4,417.1,298.4,424.7z M362.7,233.3c0,32.3-12.8,61.6-33.6,83.1c-15.8,16.4-26,37.3-29.4,59.6   c-1.5,9.6-9.8,16.7-19.6,16.7h-74.3c-9.7,0-18.1-7-19.5-16.6c-3.5-22.3-13.8-43.5-29.6-59.8c-20.4-21.2-33.1-50-33.4-81.7   c-0.7-66.6,52.3-120.5,118.9-121C308.7,113.1,362.7,166.9,362.7,233.3z M256.5,160.8c0-7.4-6-13.5-13.5-13.5   c-47.6,0-86.4,38.7-86.4,86.4c0,7.4,6,13.5,13.5,13.5c7.4,0,13.5-6,13.5-13.5c0-32.8,26.7-59.4,59.4-59.4   C250.5,174.3,256.5,168.3,256.5,160.8z M243,74.3c7.4,0,13.5-6,13.5-13.5V13.5c0-7.4-6-13.5-13.5-13.5s-13.5,6-13.5,13.5v47.3   C229.5,68.3,235.6,74.3,243,74.3z M84.1,233.2c0-7.4-6-13.5-13.5-13.5H23.3c-7.4,0-13.5,6-13.5,13.5c0,7.4,6,13.5,13.5,13.5h47.3   C78.1,246.7,84.1,240.7,84.1,233.2z M462.7,219.7h-47.3c-7.4,0-13.5,6-13.5,13.5c0,7.4,6,13.5,13.5,13.5h47.3   c7.4,0,13.5-6,13.5-13.5C476.2,225.8,470.2,219.7,462.7,219.7z M111.6,345.6l-33.5,33.5c-5.3,5.3-5.3,13.8,0,19.1   c2.6,2.6,6.1,3.9,9.5,3.9s6.9-1.3,9.5-3.9l33.5-33.5c5.3-5.3,5.3-13.8,0-19.1C125.4,340.3,116.8,340.3,111.6,345.6z M364.9,124.8   c3.4,0,6.9-1.3,9.5-3.9l33.5-33.5c5.3-5.3,5.3-13.8,0-19.1c-5.3-5.3-13.8-5.3-19.1,0l-33.5,33.5c-5.3,5.3-5.3,13.8,0,19.1   C358,123.5,361.4,124.8,364.9,124.8z M111.6,120.8c2.6,2.6,6.1,3.9,9.5,3.9s6.9-1.3,9.5-3.9c5.3-5.3,5.3-13.8,0-19.1L97.1,68.2   c-5.3-5.3-13.8-5.3-19.1,0c-5.3,5.3-5.3,13.8,0,19.1L111.6,120.8z M374.4,345.6c-5.3-5.3-13.8-5.3-19.1,0c-5.3,5.3-5.3,13.8,0,19.1   l33.5,33.5c2.6,2.6,6.1,3.9,9.5,3.9s6.9-1.3,9.5-3.9c5.3-5.3,5.3-13.8,0-19.1L374.4,345.6z'\n"
         "        fill='#FFDA44' />\n"
         "    </svg>\n"
         "  </div>\n"
         "  <section id='error'>\n"
         "    There was an error fetching from the server!\n"
         "  </section>\n"
         "  <section>\n"
         "    <h4>Current Temparature</h4>\n"
         "    <h1 id='currentval'>\n"
         "      Loading...\n"
         "    </h1>\n"
         "  </section>\n"
         "  <section>\n"
         "    <h4>Target Temparature</h4>\n"
         "    <h1 id='sliderval'>\n"
         "    </h1>\n"
         "    <input id='slider' class='slider is-fullwidth' step='1' min='0' max='34' value='26' type='range'>\n"
         "  </section>\n"
         "</body>\n"
         "<script>\n"
         "  const slider = document.querySelector('#slider');\n"
         "  const sliderValue = document.querySelector('#sliderval');\n"
         "  const currentValue = document.querySelector('#currentval');\n"
         "  const light = document.querySelector('.light');\n"
         "  const degrees = '°c';\n"
         "  let sliderActive = false;\n"
         "  let errorCount = 0;\n"
         "  sliderValue.innerHTML = slider.value + degrees;\n"
         "  slider.oninput = (e) => {\n"
         "    sliderActive = true;\n"
         "    sliderValue.innerHTML = slider.value + degrees;\n"
         "  };\n"
         "  slider.onchange = (e) => {\n"
         "    sliderActive = false;\n"
         "    sliderValue.innerHTML = slider.value + degrees;\n"
         "    fetch('/update', {\n"
         "      method: 'POST',\n"
         "      body: slider.value,\n"
         "    }).catch(e => {\n"
         "      error.classList.add('visible');\n"
         "      error.innerHTML = 'There was an error updating the target temperature.';\n"
         "    });\n"
         "  };\n"
         "  setInterval(() => {\n"
         "    fetch('/current').then(res => res.json()).then(data => {\n"
         "      currentValue.innerHTML = data.current + degrees;\n"
         "      if (!sliderActive) {\n"
         "        slider.value = data.target;\n"
         "        sliderValue.innerHTML = data.target + degrees;\n"
         "      }\n"
         "      if (data.lightOn) {\n"
         "        light.classList.remove('off');\n"
         "      } else {\n"
         "        light.classList.add('off');\n"
         "      };\n"
         "      if (data.timerError) {\n"
         "        error.classList.add('visible');\n"
         "        error.innerHTML = 'There was a timeout error! The lamp was on but the temperature did not change. Check the warmer and restart the server.';\n"
         "      } else {\n"
         "        error.classList.remove('visible');\n"
         "      };\n"
         "    }).catch(e => {\n"
         "      if (errorCount > 2) {\n"
         "        error.classList.add('visible');\n"
         "        error.innerHTML = 'There was an error communicating with the server.';\n"
         "        errorCount = 0;\n"
         "      } else {\n"
         "        errorCount++;\n"
         "      }\n"
         "    });\n"
         "  }, 2000);\n"
         "</script>\n"
         "<style>\n"
         "  * {\n"
         "    box-sizing: border-box;\n"
         "  }\n"
         "\n"
         "  body {\n"
         "    font-family: 'Roboto';\n"
         "    background: #161a2d;\n"
         "  }\n"
         "\n"
         "  section {\n"
         "    border-radius: 5px;\n"
         "    display: flex;\n"
         "    flex-direction: column;\n"
         "    align-items: center;\n"
         "    padding: 20px;\n"
         "    box-shadow: 0px 1px 4px rgba(0, 0, 0, 0.5);\n"
         "    margin: 20px;\n"
         "    background: #393e4f;\n"
         "    color: white;\n"
         "  }\n"
         "\n"
         "  .slider {\n"
         "    width: 80%;\n"
         "  }\n"
         "\n"
         "  .light-icon {\n"
         "    display: flex;\n"
         "    justify-content: center;\n"
         "  }\n"
         "\n"
         "  svg {\n"
         "    max-height: 20vw;\n"
         "    margin: auto;\n"
         "    max-width: 80px;\n"
         "  }\n"
         "\n"
         "  .off {\n"
         "    fill: grey !important;\n"
         "  }\n"
         "\n"
         "  #error {\n"
         "    background: red;\n"
         "    display: none;\n"
         "  }\n"
         "\n"
         "  #error.visible {\n"
         "    display: block;\n"
         "  }\n"
         "</style>\n"
         "\n"
         "</html>";
}

void checkTemp()
{
  if (debounce == 0 && !timerError)
  {
    // Save the last temp so we can check that it is going up in the timer function
    lastTemp = currentTemp;
    currentTemp = getTemp();
    if (currentTemp < targetTemp)
    {
      lightOn = true;
    }
    else
    {
      lightOn = false;
    }
    debounce = DEFAULTDEBOUNCE;
  }
  else
  {
    debounce--;
  }
}

void updateTimer()
{
  if (lightOn && timer == 0)
  {
    // start the timer
    timer = millis();
  }
  else if (lightOn == true && timer > 0 && lastTemp <= currentTemp)
  {
    // check to see if the light has been on too long
    if (millis() - timer > EMERGENCYMILLIS)
    {
      lightOn = false;
      timerError = true;
    }
  }
  else if (!lightOn)
  {
    timer = 0;
  }
}

void updateLight()
{
  if (lightOn && !timerError)
  {
    digitalWrite(LIGHTPIN, HIGH);
  }
  else
  {
    digitalWrite(LIGHTPIN, LOW);
  }
}

void setup(void)
{
  last = millis();
  pinMode(LIGHTPIN, OUTPUT);
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.config(ip, gateway, subnet);
  WiFi.begin(ssid, password);
  Serial.println("");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  if (MDNS.begin("esp32"))
  {
    Serial.println("MDNS responder started");
  }

  server.on("/", handleRoot);

  server.on("/current", []() {
    server.send(200, "text/json", "{ \"current\":" + String(currentTemp) + ", \"target\": " + targetTemp + ", \"lightOn\": " + lightOn + ", \"timerError\":" + timerError + "}");
  });

  server.on("/update", HTTP_POST, []() {
    String newValue = server.arg("plain");
    if (isNumeric(newValue))
    {
      targetTemp = newValue.toInt();
      server.send(200, "text/plain", "message received");
    }
    else
    {
      server.send(500, "text/plain", "bad requestd");
    }
  });

  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP server started");
}

boolean isNumeric(String str)
{
  unsigned int stringLength = str.length();

  if (stringLength == 0)
  {
    return false;
  }

  boolean seenDecimal = false;

  for (unsigned int i = 0; i < stringLength; ++i)
  {
    if (isDigit(str.charAt(i)))
    {
      continue;
    }

    if (str.charAt(i) == '.')
    {
      if (seenDecimal)
      {
        return false;
      }
      seenDecimal = true;
      continue;
    }
    return false;
  }
  return true;
}

void loop(void)
{
  checkTemp();
  updateTimer();
  updateLight();
  server.handleClient();
}
