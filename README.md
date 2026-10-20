🚗 Direction-Aware Smart Vehicle

An ESP32-based smart vehicle project that brings together embedded control, sensor-based orientation, wireless connectivity, and AI-assisted guidance in a compact vehicle platform.

The project is designed around an ESP32 development board, an OLED interface, compass-based heading information, and the Google Gemini API. The repository also includes a hardware concept diagram for the vehicle’s electronic modules.
✨ Features
- ESP32-based control: A central microcontroller for the vehicle system.
- OLED status display: A 128×64 SSD1306 display for system status and text output.
- Wireless connectivity: Wi-Fi communication for online services.
- AI-assisted guidance: Gemini API integration for generating concise, context-aware text responses.
- Modular hardware design: A reference layout for orientation sensing, audio modules, and motor-control hardware.
- Arduino development workflow: Firmware organized as an Arduino C++ sketch.
🧠 System Overview
The system is organized around the ESP32, which coordinates the user-facing display, network communication, and application logic. Sensor information can be used as contextual input, while the OLED provides a local interface for status and response text.
High-level workflow
Power on
   ↓
Initialize ESP32 and OLED display
   ↓
Connect to Wi-Fi
   ↓
Collect orientation context
   ↓
Send a request to the Gemini API
   ↓
Process the response
   ↓
Display status and response

🧰 Hardware
The reference schematic includes the following components and modules:
Component	Purpose
ESP32 DevKit V1	Main controller and Wi-Fi connectivity
SSD1306 OLED (128×64, I²C)	Displays system status and text
QMC5883 compass module	Orientation and heading sensing
MAX98357A I²S amplifier and speaker	Audio output hardware
INMP441 microphone modules	Audio input hardware concept
L298N motor driver	Interface for DC motor control
DC motors and chassis	Vehicle movement platform
Power supply and regulator	Supplies power to the electronic modules


The exact modules and wiring should be matched to the hardware used in your build. Check voltage levels, current ratings, and the relevant module datasheets before powering the circuit.
💻 Software and Technologies
- Language: C++ (Arduino framework)
- Microcontroller platform: ESP32
- Display: Adafruit SSD1306 and Adafruit GFX
- JSON handling: ArduinoJson
- Connectivity: ESP32 Wi-Fi and HTTPS client libraries
- AI service: Google Gemini API
- Development environment: Arduino IDE or a compatible ESP32 toolchain
- 
📁 Project Structure
Direction-Aware-Smart-Vehicle/
├── docs/
│   └── direction aware.png   # Hardware wiring / system diagram
├── src/
│   └── main.ino              # ESP32 firmware
├── smart vehicle.png         # Project image
└── README.md

⚙️ Setup
1. Requirements
- ESP32 development board
- Arduino IDE with ESP32 board support
- USB data cable
- Wi-Fi network
- Google Gemini API key
- Required Arduino libraries listed below
2. Install libraries
Install these libraries through the Arduino IDE Library Manager or your preferred package-management workflow:
- Adafruit GFX Library
- Adafruit SSD1306
- ArduinoJson
- ESP8266Audio (for compatible I²S/audio classes)
The ESP32 core also provides the Wi-Fi, secure client, HTTP client, and I²C libraries used by the sketch.
3. Configure credentials
The firmware includes a secrets.h header for local configuration. Create this file beside main.ino and define your Wi-Fi and Gemini settings:
#pragma once

#define WIFI_SSID "YOUR_WIFI_NAME"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
#define GEMINI_API_KEY "YOUR_GEMINI_API_KEY"
#define GEMINI_MODEL "YOUR_SUPPORTED_GEMINI_MODEL"
#define TTS_WAV_URL_BASE ""
Replace the example values with your own credentials and a Gemini model available to your API project. Do not commit real passwords or API keys to GitHub. Keep secrets.h local and add it to .gitignore.
4. Build and upload
1. Clone the repository:
   git clone https://github.com/harshit6043/Direction-Aware-Smart-Vehicle.git
   cd Direction-Aware-Smart-Vehicle
2. Open src/main.ino in the Arduino IDE.
3. Select the appropriate ESP32 board and connected serial port.
4. Install the required libraries and configure secrets.h.
5. Compile and upload the sketch.
6. Open Serial Monitor at 115200 baud to view diagnostic output.
🔌 Pin Configuration
The firmware uses the following display and I²S pin definitions:
Signal	GPIO / Value
OLED SDA	GPIO 21
OLED SCL	GPIO 22
OLED I²C address	0x3C
I²S bit clock (BCLK)	GPIO 26
I²S word select (LRC)	GPIO 25
I²S data out (DOUT)	GPIO 27


Confirm these assignments against your ESP32 board and wiring before connecting the modules.
🖼️ Hardware Diagram
The repository includes a visual reference for the proposed hardware layout.

🔐 Security and Safety
- Keep Wi-Fi credentials and API keys out of public commits.
- Use appropriate voltage regulation and verify polarity before powering hardware.
- Do not drive motors directly from ESP32 GPIO pins; use a suitable motor driver and power supply.
- Validate any generated guidance before using it to control a physical vehicle.
- For deployment, use HTTPS certificate validation rather than disabling TLS verification.
🚀 Future Enhancements
- Compass calibration and improved heading processing
- Integrated audio input and voice output
- Motor control and vehicle movement logic
- Improved connectivity handling and diagnostic reporting
- Hardware validation, test documentation, and refined circuit diagrams
- 
🤝 Contributing
Contributions and suggestions are welcome. Keep changes focused, document hardware assumptions, and include setup or testing details when introducing new functionality.
