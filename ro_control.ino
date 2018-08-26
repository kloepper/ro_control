void setup() {
  // initialize serial communication at 9600 bits per second:
  Serial.begin(9600);
  
  // initialize digital pin LED_BUILTIN as an output.
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(12, OUTPUT);

}

float dac_to_psi(int dac) {
  return (dac - 100) / 8.192;
}

void print(int shortTubeValue, int longTubeValue) {
  // print out the value you read:
  Serial.print(shortTubeValue);
  Serial.print(' ');
  Serial.print(longTubeValue);
  Serial.print(" -- ");
  Serial.print(dac_to_psi(shortTubeValue));
  Serial.print(' ');
  Serial.print(dac_to_psi(longTubeValue));
  Serial.println();
}


long total = 0;
long count = 0;
float longPsiEwma = 0.0;
float ewmaWeight = 0.5;
void printCsv(int shortTubeValue, int longTubeValue) {
  count += 1;
  total += shortTubeValue;
  float average = float(total) / float(count);
  float longPsi = dac_to_psi(longTubeValue);
  longPsiEwma = ewmaWeight * longPsi + (1-ewmaWeight) * longPsiEwma;
  
  Serial.print(millis()); Serial.print(',');
  Serial.print(dac_to_psi(shortTubeValue)); Serial.print(',');
  Serial.print(average); Serial.print(',');
  Serial.print(longPsi); Serial.print(',');
  Serial.print(longPsiEwma); Serial.print(',');
  Serial.println();
}

class SetPoints {
  public:
  SetPoints(int low, int high)
  : _low(low)
  , _high(high)
  { if (high < low) { int tmp = _low; _low = _high; _high = tmp; }}

  float low() { return _low; }
  float high() { return _high; }

  private:
  float _low;
  float _high;
};

class EwmaValue {
  public:
  EwmaValue(float w, float initialValue=0.0)
  : _w(w)
  , _value(initialValue)
  { 
    if (_w > 1) { _w = 1; }
    if (_w < 0.01) { _w = 0.01; }
  }

  float update(float value) {
    _value = value * _w + _value * (1 - _w);
    return _value;
  }

  float value() { return _value; }

  private:
  float _w;
  float _value;
};

class DeliveryPumpControl {
  public:
  DeliveryPumpControl(SetPoints in, SetPoints out)
  : isRunning(false)
  , isInputDelay(false) 
  , in(in)
  , out(out)
  , inSmooth(0.2, in.low())
  , outSmooth(0.05, out.high())
  , sampleCount(0)
  {}

  void update(float inPsi, float outPsi) {
    float inSmoothValue = inSmooth.update(inPsi);
    float outSmoothValue = outSmooth.update(outPsi);
    Serial.print(millis()); Serial.print(',');
    Serial.print(inPsi); Serial.print(',');
    Serial.print(inSmoothValue); Serial.print(','); 
    Serial.print(outPsi); Serial.print(',');
    Serial.print(outSmoothValue); Serial.print(','); 
    Serial.print(isRunning); Serial.print(','); 
    Serial.print(isInputDelay); Serial.print(',');
    Serial.println(); 
    // Do not change control while EWMA is stabilizing.
    if (sampleCount < 50) {
      sampleCount++;
      return;
    }
    if (isRunning) {
      if (outSmoothValue > out.high()) {
        isRunning = false;
      }
    } else {
      if (outSmoothValue < out.low()) {
        isRunning = true;
      }
    }
    if (isInputDelay) {
      if (inSmoothValue > in.high()) {
        isInputDelay = false;
      }
    } else {
      if (inSmoothValue < in.low()) {
        isInputDelay = true;
      }
    }
  }

  bool isPumpActive() { return isRunning && !isInputDelay; }
  
  private: 
  bool isRunning;
  bool isInputDelay;
  SetPoints in, out;
  EwmaValue inSmooth, outSmooth;
  int sampleCount;
};

DeliveryPumpControl ro_control(SetPoints(1, 10), SetPoints(60, 65));

void loop() {
  // read the input on analog pin 0:
  int shortTubeValue = analogRead(A0);
  int longTubeValue = analogRead(A1);

  //printCsv(shortTubeValue, longTubeValue);
  ro_control.update(dac_to_psi(shortTubeValue), dac_to_psi(longTubeValue));

  if (ro_control.isPumpActive()) {
    digitalWrite(LED_BUILTIN, HIGH);
    digitalWrite(12, HIGH); 
  } else {
    digitalWrite(LED_BUILTIN, LOW);
    digitalWrite(12, LOW); 
  }


  delay(100);        // delay in between reads for stability
}
