/*********************************************************************
 MIDI over BLE.

 The board shows up as a Bluetooth MIDI device. Pair it from the host,
 then connect to it - on macOS that is Audio MIDI Setup, window
 "MIDI Studio", the Bluetooth button. Point any synth or DAW at it and
 the board plays a scale, one note every half second.

 It also listens: notes the host sends are printed on the serial port,
 so a keyboard or a DAW track can be checked from this side.

 Needs the MIDI Library by Francois Best (Library Manager: "MIDI
 Library"). That library does the message encoding; BLEMidi is the
 transport it writes through.

 baram-nrf54l-arduino - MIT license
*********************************************************************/
#include <bluefruit.h>
#include <MIDI.h>

BLEDis  bledis;
BLEMidi blemidi;

MIDI_CREATE_BLE_INSTANCE(blemidi);

/* One octave of C major, up and back down. */
const byte scale[] = { 60, 62, 64, 65, 67, 69, 71, 72, 71, 69, 67, 65, 64, 62 };
unsigned int position = 0;

void setup()
{
  Serial.begin(115200);
  Serial.println("BLE MIDI example");

  Bluefruit.begin();
  Bluefruit.setTxPower(4);
  Bluefruit.setName("BARAM MIDI");
  Bluefruit.autoConnLed(true);

  bledis.setManufacturer("BARAM");
  bledis.setModel("nRF54L MIDI");
  bledis.begin();

  /* MIDI.begin() calls blemidi.begin() through the Stream it was
   * created on, so the service is started here. */
  MIDI.begin(MIDI_CHANNEL_OMNI);
  MIDI.setHandleNoteOn(handleNoteOn);
  MIDI.setHandleNoteOff(handleNoteOff);

  startAdv();
  Serial.println("Advertising. Connect from the host's Bluetooth MIDI setup.");

  Scheduler.startLoop(midiRead);
}

void startAdv(void)
{
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  Bluefruit.Advertising.addService(blemidi);
  Bluefruit.ScanResponse.addName();
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.start(0);
}

void handleNoteOn(byte channel, byte pitch, byte velocity)
{
  Serial.printf("note on  ch=%u pitch=%u vel=%u\n", channel, pitch, velocity);
}

void handleNoteOff(byte channel, byte pitch, byte velocity)
{
  Serial.printf("note off ch=%u pitch=%u vel=%u\n", channel, pitch, velocity);
}

void loop()
{
  /* Nothing goes out until the host subscribes - until then the notify
   * is refused and the notes would be lost silently. */
  if ( !blemidi.notifyEnabled() ) { delay(100); return; }

  byte note = scale[position];
  position = (position + 1) % (sizeof(scale) / sizeof(scale[0]));

  MIDI.sendNoteOn(note, 100, 1);
  delay(250);
  MIDI.sendNoteOff(note, 0, 1);
  delay(250);
}

/* Reading happens on its own task so a slow handler cannot stall the
 * note sending above. */
void midiRead()
{
  if ( !Bluefruit.connected() ) { delay(100); return; }

  while ( MIDI.read() ) { }
  delay(1);
}
