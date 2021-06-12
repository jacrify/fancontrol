
/*
  Controls a SeaFlo blower hooked into a flight sim or racing sim.
  Consumes a steady stream of speeds passed overe serial
  (integers, series of digits one per byte, separated by \n)
  Automatically adjusts range of fan to range of data passed.
  When started, assumes first value passed is somewhere near middle of range.
  Max speed set as some percentage over that, min speed is set as some percentage under.
  Max and min are gradually reduced over time (60 seconds if 10 speeds are received per second)
  Whenever the received max or min is greater or less than the assume max or min, that value
  becomes the new max or min, and the reduction stops.
  In theory you can start flying, and if you cover most of the aircrafts envelope in the first
  60 seconds it will be dialled in.
  When changing aircraft, or if you start on the ground, hit the reset button once flying
  to restart calibration.
  Pot controls fan speed directly.

  Runs 20hz timer to reduce pwm squel.
*/


////////////////////////////////////////////////////////////////////////////////
//  DECLARATIONS
////////////////////////////////////////////////////////////////////////////////


//pins
int rPWM = 10; //we only use this one
int lPWM = 9;
int swi = 3;  //switch to clear history
int pot = A4; //pot for control


int potMax = 1024; //max pot value

int vel = 0; 
int newVel = 0;
const int MaxChars = 10;
char strValue[MaxChars + 1];
int index = 0;

//Store min and max values seen so far
int maxVelSeen = 0;
int minVelSeen = 0;

//Start with some overhead on min and max
int maxMultiplier = 2;
int minDivisor = 2;
int minAirSpeed=40;//in case they land
  
//until overhead is hit, shrink the max and min each cycle
int cycleShrinkDivisor = 20; //1% per cycle=100
int cycleCounter = 0;
int cycleSize = 10; //at 10 samples per second, this is 60 seconds. So 1% smaller/larger each minute
//int cycleSize = 1;

boolean maxVelHit = false;
boolean minVelHit = false;

boolean buttonHit = false; // reset when button hit to go to auto mode
boolean firstRun = true;

int fanpwmmin = 160; //Lowest speed that will run the fan
int fanpwmmax = 799; //based on timer voodoo. Reduce to drop fan max, or just use hardware slider
int fanvelraw = 0; //raw scaled vaue (0-799) before pot scaling
int fanveladjusted = 0; //actual timer value after pot scaling




////////////////////////////////////////////////////////////////////////////////
// INITIALIZATION
////////////////////////////////////////////////////////////////////////////////
void analogWriteSAH_Init( void )
{
  // Stop the timer while we muck with it

  TCCR1B = (0 << ICNC1) | (0 << ICES1) | (0 << WGM13) | (0 << WGM12) | (0 << CS12) | (0 << CS11) | (0 << CS10);

  // Set the timer to mode 14...
  //
  // Mode  WGM13  WGM12  WGM11  WGM10  Timer/Counter Mode of Operation  TOP   Update of OCR1x at TOV1  Flag Set on
  //              CTC1   PWM11  PWM10
  // ----  -----  -----  -----  -----  -------------------------------  ----  -----------------------  -----------
  // 14    1      1      1      0      Fast PWM                         ICR1  BOTTOM                   TOP

  // Set output on Channel A and B to...
  //
  // COM1z1  COM1z0  Description
  // ------  ------  -----------------------------------------------------------
  // 1       0       Clear OC1A/OC1B on Compare Match (Set output to low level).

  TCCR1A =
    (1 << COM1A1) | (0 << COM1A0) |   // COM1A1, COM1A0 = 1, 0
    (1 << COM1B1) | (0 << COM1B0) |
    (1 << WGM11) | (0 << WGM10);      // WGM11, WGM10 = 1, 0

  // Set TOP to...
  //
  // fclk_I/O = 16000000
  // N        = 1
  // TOP      = 799
  //
  // fOCnxPWM = fclk_I/O / (N * (1 + TOP))
  // fOCnxPWM = 16000000 / (1 * (1 + 799))
  // fOCnxPWM = 16000000 / 800
  // fOCnxPWM = 20000

  ICR1 = 799;

  // Ensure the first slope is complete

  TCNT1 = 0;

  // Ensure Channel A and B start at zero / off

  OCR1A = 0;
  OCR1B = 0;

  // We don't need no stinkin interrupts

  TIMSK1 = (0 << ICIE1) | (0 << OCIE1B) | (0 << OCIE1A) | (0 << TOIE1);

  // Ensure the Channel A and B pins are configured for output
  DDRB |= (1 << DDB1);
  DDRB |= (1 << DDB2);

  // Start the timer...
  //
  // CS12  CS11  CS10  Description
  // ----  ----  ----  ------------------------
  // 0     0     1     clkI/O/1 (No prescaling)

  TCCR1B =
    (0 << ICNC1) | (0 << ICES1) |
    (1 << WGM13) | (1 << WGM12) |              // WGM13, WGM12 = 1, 1
    (0 << CS12) | (0 << CS11) | (1 << CS10);
}




////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
void setup()
{
  analogWriteSAH_Init();
  // serial initialization
  Serial.begin(9600);

  // initialization of Arduino's pins

  pinMode(rPWM, OUTPUT);
  pinMode(lPWM, OUTPUT);
  pinMode(swi, INPUT_PULLUP);

  OCR1B = 0; //D10- fan off

}

void buttonReset()
{
  //reset auto mode
  buttonHit = false;
  firstRun = false;
  maxVelSeen = vel * maxMultiplier;
  minVelSeen = vel / minDivisor;
  maxVelHit = false;
  minVelHit = false;

  Serial.print("Reseting: new min:");
  Serial.print( minVelSeen);
  Serial.print(',');
  Serial.print(" new max:");
  Serial.print(maxVelSeen);
  Serial.write("\n");

}

////////////////////////////////////////////////////////////////////////////////
///////////////////////////////// Main Loop ////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

void loop()
{
  while (1 == 1) // to get the fastest loop possible
  {
    if (!buttonHit) { //once button is hit, leave it to reset to unflag
      int switchValue = digitalRead(swi);

      if (switchValue == 0) {
        buttonHit = true;
        Serial.print("Resetting\n");
      } else
      {
        buttonHit = false;
      }
    }


    int potVal = potMax - analogRead(pot);
    //int potMultiplier = (potMax - potVal) io/ potMax; //I wired it the wrong way round :)
    //Serial.print(potVal);
    //  Serial.write("\n");


    //       Serial.print(potMultiplier);
    //     Serial.write("\n");


    if (Serial.available() > 0) {
      char ch = Serial.read();
      //Serial.write(ch);
      if (index < MaxChars && isDigit(ch)) {
        strValue[index++] = ch;
      } else {

        strValue[index] = 0;
        vel = atoi(strValue);
        //Serial.print("Vel:");
        //Serial.print(vel);
        //Serial.print("\n");
        //      Serial.write("\nVelocity received:");
        //       Serial.print(vel);
        //     Serial.write("\n");
        strValue;
        index = 0;
        /*
                Serial.print(buttonHit);
                Serial.write("\n");
                Serial.print(firstRun);
                Serial.write("\n");
                Serial.write("\n");
        */
        if ((buttonHit ) || (firstRun)) {
          buttonReset();
        }

        if (vel != 0) {
          if (vel >= maxVelSeen) {
            maxVelSeen = vel;
            maxVelHit = true;
          }

          if (vel <= minVelSeen ) {
            minVelSeen = vel;
            minVelHit = true;
          }

          cycleCounter++;
          if (cycleCounter >= cycleSize) {
            cycleCounter = 0;
            if (!maxVelHit) {
              Serial.print("Cycle- shrinking top limit maxVelSeen\n");
              maxVelSeen = maxVelSeen - (((minVelSeen + maxVelSeen) / 2) / cycleShrinkDivisor);
            }
            if (!minVelHit||  vel <=minAirSpeed) {
              minVelSeen = minVelSeen + (((minVelSeen + maxVelSeen) / 2) / cycleShrinkDivisor);
            }
          }

        }
      }
      Serial.print("Vel: ");
      Serial.print(vel);

      Serial.print("\tMax: ");
      Serial.print(maxVelSeen);
      if (!maxVelHit)
        Serial.print("*");
      Serial.print("\tMin: ");
      Serial.print(minVelSeen);
      if (!minVelHit)
        Serial.print("*");


      Serial.print("\tFan raw: ");
      Serial.print(fanvelraw);


      Serial.print("\tFan adj: ");
      Serial.print(fanveladjusted);

      Serial.print("\tPot: ");
      Serial.print(potVal);

      Serial.write('\n');
    }
    if ((vel == 0) || (potVal <= 20)) {

      fanveladjusted = 0;
    } else {
      fanvelraw = constrain(map(vel, minVelSeen, maxVelSeen, 0, fanpwmmax - fanpwmmin), 0, fanpwmmax - fanpwmmin); //scale into available range
      fanveladjusted = fanpwmmin + (int)((float)fanvelraw * ((float)potVal / (float)potMax)); //pot only affects that range



    }
    OCR1B = fanveladjusted;
    /*
      Serial.print(vel);
          Serial.print(',');
      Serial.print(minVelSeen);
      Serial.print(',');
      Serial.print(maxVelSeen);
      Serial.print(',');
      Serial.print(fanvel);
      Serial.write("\n");
      //    Serial.print(',');
      //

    */

    //  Serial.print(maxVelSeen);
    //    Serial.print(',');
    //    Serial.print(minVelSeen);
    //    Serial.print(',');
    //   Serial.print(vel);
    //  Serial.write("\n");

    //Serial.print(vel);


    // Serial.write("\n");


    // OCR1B = constrain(map((int)fval, 0, 255, 0, fanpwmmax), 0, fanpwmmax); //D10

    // OCR1A = constrain(map(vel, 0, 255, 0, 799), 0, 799); //D9
    //  OCR1B = constrain(map(vel, 0, 255, 0, 799), 0, 799); //D10
  }
}
