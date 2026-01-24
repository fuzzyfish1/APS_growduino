/**
* System -
 *   OS:  [Linux Mint 22.1 x86 Cinnamon]
 *   IDE: [CLion + PlatformIO] or [Zellij + Micro]
 * Author: Zain Ali
 *
 * APS ReadySetCode GrowDuino
 * This code does the following
 *		- Measures Ambieint temperature and humidity using the DHT11/22
 *		- Measures soil moisture with a resistive soil moisture probe
 *		- uses the following information to water the plant
 *
 * This code is segmented to test/learn about things individually before connecting everything together
 *
 * I beg of thee, don't dump this into chatgpt i commented this out so a human can read it
 *
 * TODO: FIND BETTER NAME
 *		 FIND DISTRIBUTION LICENSE FOR THIS PROJECT, maybe apache?
 *		 UNIT TESTING
 *		 GARDUINO FORM FACTOR EXPANSION AND TURN INTO BULBASAUR
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


/** MACROS for adjusting and tuning the watering algorithm
 * WATERING_TIME - defines how often to check dryness of soil and water plant (ms)
 * WATERING_PWR - Controls the pump power/flow rate, running full speed might create big splash (PWM 0-255)
 * WATERING_OTHER_TIME - Controls how long to water the plant for (ms)
 * WATERING_RESISTANE - arbitrary resistance threshold for deciding if WET or DRY, less resistance means dryer, (Ohms)
 */
#define WATERING_TIME 3 * 3600 * 1000 // 3h * 3600 s/h * 1000 ms/s
#define WATERING_PWR 50
#define WATERING_OTHER_TIME 1000
#define WATERING_RESISTANCE 85000

DHT dht(DHT11_PIN, DHT_TYPE);

void setup() {
	pinMode(PWM_PIN, OUTPUT);
	dht.begin();

	Serial.begin(9600);
	Serial.println("Chicken");
}

void loop() {

	/** STEP 1: LEARNING HOW TO USE THE DHT11
	 * I put this here first because it is the simplest, most forgiving sensor, requiring only VCC, GND, and one digital pin
	 * they can learn how to breadboard things here as well as a little bit electricity and reading schematics that you will show them
	 */

	/** put on slide Deck
	 * grab Single Wire Protocol screenshots from here for your slide deck it would be cool to show kids
	 * https://www.mouser.com/datasheet/2/758/DHT11-Technical-Data-Sheet-Translated-Version-1143054.pdf
	 */

	/** understanding excersize for kids
	 * do not use doubles in code, explain that floats are decimal points written in binary
	 * have kids encode 32 bit IEEE754 floats manually, it sounds scarier than it is
	 * help the kids understand NaN, Inf, decimals, and bit of math
	 * helps the kids to understand why we are using NaN for errors
	 */

	// these functions are relatively simple, just give these to them
	float t = dht.readTemperature();
	float h = dht.readHumidity();

	// t and h are NaN if there is an error, potentially indicating faulty wiring or some other error
	// we should account for that in code anyway so if it triggers the Instructor knows to check on the kid
	if (isnan(h) || isnan(t)) {
		Serial.println("Failed to read from DHT sensor! Check your Wiring, or notify an Instructor.");
		return; // restarts the void loop() to try again
	}

	/** put on the slides
	 * RH Scale
	 * the RH scale is a measure of water dissolved in the air at a specific temperature
	 * higher temperature means more water can dissolve in the air
	 * 0% is no dissolved humidity in the air
	 * 100% is the dew point, or the point where no more water can dissolve, so liquid condenses on surfaces
	 * find some cool diagram or somn for this
	 */

	/** kids excersize
	 * have the kids find creative ways to display humidity and temperature, ex: a text based percentage bar for RH
	 */
	Serial.print("Humidity: ");
	Serial.print(h);
	Serial.print(" %\t");
	Serial.print("Temperature: ");
	Serial.print(t);
	Serial.print(" *C\t");

	/** STEP 2: LEARNING HOW TO USE THE SOIL SENSOR
	 * This is potentially more messy, less intuitive and more difficult to wire
	 */

	/** put in the slides
	 * something to explain ohms law
	 * electrons are in a super position of moving everywhere at once at 0V
	 * applying a voltage shifts the electron waveform to move towards electron holes
	 * when electrons encounter atoms and other electrons in their travels
	 * they create electron interference waveforms (think the double slit experiment) and
	 * the finite barrier problem (https://qm1.quantumtinkerer.tudelft.nl/8_finite-barrier/)
	 * some electrons are reflected backwards, which creates resistance
	 * now V = IR
	 */

	/** soil moisture sensor as a resistor
	 * soil dissolved in water creates more free moving ions
	 * which leads to less resistance for flowing electrons
	 */

	/** excersize for the kids to understand this shiii
	 * have cups filled with soil with different levels of dampness
	 * have kids measure the resistance of soil moisture sensor in every different cup with multimeter
	 * have them free measure nothing on the soil sensor as well, write down all the resistance values for every cup so everyone can
	 * look at all the datapoints to see what different dampness resistances look like so they can decide
	 * ~ how much resistance triggers when the plant needs to be watered
	 *
	 * then have kids use 3.3V on the arduino, an LED, and try a bunch of different resistors to see how bright the LED is
	 * don't let them plug in anything without a resistor things will fry
	 */

	 /** Resistor divider circuit
	 * I would preface this by explaining a circuit diagram of what a resistor divider is
	 * For Instructor/Curriculum guy/Andy:
	 *		- a resistor divider is a circuit of 2 resistors in series, at the midpoint where they are connected has a calculatable output voltage
	 *		- https://ohmslawcalculator.com/voltage-divider-calculator << show to kids for circuit
	 *		- this is a ciruit used to find resistances, create voltage references from higher voltages, and step logic from higher to lower voltages
	 *		- this equation assumes no current is drawn from the midpoint and not efficient for large currents, but useful for measurement and scaling
	 *
	 * This segment is about teaching AnalogRead, and The Resistor divider Circuit
	 *
	 * I made an Engineering Desgn decision to forgo the board that comes with the soil sensor and use a resistor divider to measure it's resistance
	 * the boards only advantage is that it is a really weird component with a higher accuracy voltage comparator that we do not need
	 *
	 * have the kids try measuring extra resistors around 10k ohm with the resistor divider and code below to understand how the resistor divider measures
	 * resistance
	 */

	const float R1 = 100000; // 10k Ohm
	float Vin = 5.0; // voltage level of the resistor divider, il make a more clear documentation on how to word this for kids
	float Vout = analogRead(RSTV_MSTR_PIN) * (5.0 / 1023.0);// freaky math 5 is from 5 volt, 1023 is maximum anologologologo ting
	float R2 = R1 * (Vin / Vout - 1);

	Serial.print("Resistance is: ");
	Serial.print(R2);
	Serial.println(" Ohms");

	/**watering algorithm
	 * this is more up to the kids the instructor and the plant
	 * but my algorithm says
	 * if soil is wet and watering_time has past then water the plant
	 */
	static unsigned long lastWatering = WATERING_TIME * .5;

	if (R2 > WATERING_RESISTANCE && (millis() - lastWatering > WATERING_TIME)) {

		lastWatering = millis();
		Serial.println("SLURP SLURP");

		/** PWM pump control
		 * use the S8050 transistor https://www.onsemi.com/download/data-sheet/pdf/ss8050-d.pdf
		 * there are alternative transistors in the case of supplier issues, I believe the 2N2222 might also work
		 * Arduino PWM pin to the Base
		 * Emitter to GND
		 * Base to GND cable of Pump
		 * pump to Vcc +5V
		 *
		 * you can replace the pump with an LED to try out the circuit without spraying water everywhere
		 * you can put this LED as a flyback diode to prevent future explosions after you kids figure the pump out
		 * or put it in series with the pump with a resistor so you have a status LED when it waters
		 */
		analogWrite(PWM_PIN, WATERING_PWR);
		delay(WATERING_OTHER_TIME);

		analogWrite(PWM_PIN, 0);
	}

}