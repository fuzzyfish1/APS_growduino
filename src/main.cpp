/**
* System -
 *   OS:  [Linux Mint 22.1 x86 Cinnamon]
 *   IDE: [CLion + PlatformIO] or [Zellij + Micro]
 * Author: Zain Ali
 *
 * APS ReadySetCode GrowDuino
 * This code does the following
 *		- Measures Ambient temperature and humidity using the DHT11/22
 *		- Measures soil moisture with a resistive soil moisture probe
 *		- uses the following information to water the plant
 *
 * TODO: Tadgh explain Ohms Law
 *		something to explain ohms law
 *		electrons are in a super position of moving everywhere at once at 0V
 *		applying a voltage shifts the electron waveform to move towards electron holes
 *		when electrons encounter atoms and other electrons in their travels
 *		they create electron interference waveforms (think the double slit experiment) and
 *		the finite barrier problem (https://qm1.quantumtinkerer.tudelft.nl/8_finite-barrier/)
 *		some electrons are reflected backwards, which creates resistance, V = IR
 *
 * TODO: Tadgh explain resistor dividers
 *		have the kids go to https://ohmslawcalculator.com/voltage-divider-calculator
 *		derive the resistor divider equation and explain how the resistance is measured
 *
 * TODO: Tadgh explain RH scale and add to slides
 *		for a specific temperature,
 *		maximum amount of water air can hold is called the dew point (extra water in the air will condensate)
 *		RH is the percentage of water in the air, more than 100% water begins to condensate
 *		RH = mass water in air / mass water that can be in the air
 *
 *
 * Docs + links:
 * parts documentation links
 * https://www.mouser.com/datasheet/2/758/DHT11-Technical-Data-Sheet-Translated-Version-1143054.pdf
 * https://www.onsemi.com/download/data-sheet/pdf/ss8050-d.pdf
 *
 * https://image-ppubs.uspto.gov/dirsearch-public/print/downloadPdf/5696671 << calculating water lost by TH sensors, pure IOT approach
 * https://pmc.ncbi.nlm.nih.gov/articles/PMC8237332/ << ML approach to the project
 * https://iwaponline.com/jh/article/26/12/3224/106261/Development-of-a-smart-irrigation-monitoring << edge compute approach
 *
 * TODO: GARDUINO FORM FACTOR EXPANSION AND TURN INTO BULBASAUR
 *		 PHOTODIODE AND NEOPIXEL EXPANSION
 *		 3DP CHECK VALVE
 *		 HAVE MORE VARIETY OF PLANTS TO TRY OUT
*/

#include <Arduino.h>
#include <math.h>

#include "DHT.h"

#define PWM_PIN 10
#define DHT11_PIN 7

#define RSTV_MSTR_PIN A0

#define DHT_TYPE DHT11
#define IS_VALID(x) (!isnan(x) && !isinf(x))

// prgrm ctrl defines
#define WATERING_PWR 100 // Controls the pump power/flow rate, running full speed might create big splash (PWM 0-255)
// #define WATERING_OTHER_TIME 1000 // Controls how long to water the plant for (ms)
// #define WATERING_RESISTANCE 85000 // arbitrary resistance threshold for deciding if WET or DRY, less resistance means dryer, (Ohms)

DHT dht(DHT11_PIN, DHT_TYPE);

void setup() {
	pinMode(PWM_PIN, OUTPUT);
	dht.begin();

	Serial.begin(115200);
	Serial.println("hello world");
}

/**
 * Calculate the soil moisture percentage based on soil resistance.
 * @param soilResistance resistance value of the soil in ohms
 * @return soil moisture percentage constrained between 0 and 100
 */
unsigned long soilPercent(const float& soilResistance) {
	// step 2: give values for DRY RESIST and WET RESIST in ohms, keep the UL at the end pls

	// I went with dry soil reading first, labeled that 0%
	// I then went with a slightly watered soil reading and called it 40%
	// this is not the conventional tuning method which is in water, in air, but you can do that as well
	// I had better luck with this because 35% is the dryness where it decides to water the plant
	const unsigned long DRYRESIST = 4000UL;
	const unsigned long WETRESIST = 2700UL;
	const unsigned long percentage = map(soilResistance, DRYRESIST, WETRESIST, 0UL, 40UL);
	return max(0UL, min(percentage, 100UL));
}

/**
 * take an average analog reading for a higher accuracy
 * @param pinNum analog pin to read from
 * @return analog value to return
 */
unsigned long avgReading(const int& pinNum) {

	const unsigned long NUM_READINGS = 256UL;
	// pick any power of 2 for fast division later

	unsigned long sum = 0;
	for (int i = 0; i < NUM_READINGS; i++) {
		sum += analogRead(pinNum);
	}

	return sum / NUM_READINGS;
}

/**
 * Calculates the resistance of the second resistor (R2) in a voltage divider circuit.
 *
 * @param reading The ADC reading, representing the voltage measured at the output of the voltage divider.
 * @return The calculated resistance of the second resistor (R2) in ohms.
 */
float resistorDivCalc(const unsigned long& reading) {

	const float R1 = 10000.f; // 10K ohm resistor that we are using
	const float Vin = 5.f; // voltage input level of the resistor divider
	const float Vout = reading * (Vin / 1023.f); // 1023 is the max value that can come out of the ADC
	const float R2 = R1 * (Vin / Vout - 1); // the calculated unknown resistance

	return R2;
}

/**
 * Waters the plant for a specified duration using a water pump and visual feedback through LED blinking.
 *
 * Activates a pump with some pwm power for some length of time
 * blinks LED_BUILTIN to show it is actively trying to water a plant
 *
 * @param pumpPin - PWM pin to control pump
 * @param wateringPwr - PWM power to water with
 * @param wateringTime - length of time to water the plant
 */
void waterPlant(const int& pumpPin, const int& wateringPwr, const int& wateringTime) {
	const unsigned long del = 50UL;

	Serial.println("SLURP SLURP");

	for (long start = millis(); millis() - start < wateringTime;) {

		digitalWrite(LED_BUILTIN, millis() % (del*2) > del);
		// you could do an experiment with the LED to show how this works
		analogWrite(pumpPin, wateringPwr);

	}
	analogWrite(pumpPin, 0);
}

/**
 * Adjust the watering threshold based on temperature and humidity conditions.
 * @param tempC the current temperature in degrees Celsius
 * @param humidity the current humidity level as a percentage
 * @return adjusted watering threshold constrained between 20 and 60
 */
unsigned long adjustedWateringThreshold(const float& tempC, const float& humidity) {
	int t = 35; // you should water the plant as little as possible and use that to tune this

	// btw these numbers are entirely arbitrary
	if (tempC > 28) {
		t +=8;
	} else if (tempC < 18) {
		t -= 5;
	}

	if (humidity < 40) {
		t += 5;
	} else if (humidity > 70) {
		t -= 3;
	}

	return constrain(t, 20, 60);
}

void loop() {

	// step 1: read the sensor, and see it's output before and after you breath on it
	const float t = dht.readTemperature();
	const float h = dht.readHumidity();

	// soil moisture is constantly active, which is bad for it but this is intentional
	// PLA can trap moisture in infill as well as decay. The kit doesn't last too long hopefully
	// use a capacitive soil moisture sensor in real life

	const float soilResistance = resistorDivCalc(avgReading(RSTV_MSTR_PIN));
	// step 2: read the resistance of wet and dry soil to calibrate soilPercent
	const float soilMoisture = soilPercent(soilResistance);
	// step3: calibrate soilMoisture, I tuned to the value of 35% after an extremely light watering

	if (!IS_VALID(t) || !IS_VALID(h) || !IS_VALID(soilResistance)) {
		// error protection code, dw about this :)
		// if your code isn't printing anything / executing below,
		// you should add debug prints here and check wiring
		return;
	}

	// reading all the sensors for graphing or other.
	static unsigned long lastPrint = 0;
	if (millis() - lastPrint > 500) {
		Serial.print("Humidity: ");
		Serial.print(h);
		Serial.print("%,\tTemperature: ");
		Serial.print(t);
		Serial.print("*C,\tSoil Resistance: ");
		Serial.print(soilResistance);
		Serial.print("Ohms,\tSoil Moisture: ");
		Serial.print(soilMoisture);
		Serial.println("%");
		lastPrint = millis();
	}

	static unsigned long lastWatering = 0UL;
	constexpr unsigned long WATERING_PERIOD = (1UL * 60UL * 60UL * 1000UL); // every 1 hour in ms, btw the UL is necessary to avoid overflow
	const unsigned long WATERING_TIME = 500UL; // water for .5 seconds, it helps to set this to 2 just to prime it

	// if it wasn't watered before, water anyway
	// or if the time elapsed since last water is more than the watering period
	// and the soil is below the watering threshold
	if (!lastWatering || (millis() - lastWatering > WATERING_PERIOD && soilMoisture <= adjustedWateringThreshold(t,h))) {
		waterPlant(PWM_PIN, WATERING_PWR, WATERING_TIME);

		lastWatering = millis();
	}
}
