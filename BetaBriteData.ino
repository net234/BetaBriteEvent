/*************************************************
 *************************************************
    Sketch BetaBriteData.ino   // interface avec un afficheur betabrite

   
    Copyright 20201 Pierre HENRY net23@frdev.com .
    https://github.com/betamachine-org/BetaPorteV2   net23@frdev.com

  This file is part of betabriteEvent.

    betabriteEvent is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    betabriteEvent is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with betaEvents.  If not, see <https://www.gnu.org/licenses/lglp.txt>.



*************************************************/








void betaBriteWrite(const String& aMessage) {
  Serial.print(F("Send Message BetaBrite : "));
  Serial.println(aMessage);

  lcdMessage = aMessage;
  lcdMessage.replace(F("\x1c"
                       "3"),
                     "");
  lcdMessage.replace(F("\x1c"
                       "2"),
                     "");
  lcdMessage.replace(F("\x1c"
                       "1"),
                     "");
  lcdMessage.replace(F("\x1c"
                       "8"),
                     "");
  lcdMessage.replace(F("\x1c"
                       "9"),
                     "");
  if (lcdOk) Events.delayedPushMillis(500, evLcdRefresh, 0);




  for (int N = 0; N < 8; N++) Serial1.write(0);  // setup speed (mandatory)
Serial1.print(F("\x01"
                "Z00"));  // all display (mandatory)
  Serial1.print(F("\x02"
                  "AA"));  // write to A file (mandatory)
  Serial1.print(F("\x1b"
                  " a"));  // mode defilant
  Serial1.print(F("\x1c"
                  "3"));  // couleur ambre
  Serial1.print(aMessage);
  //Serial1.print(F("\x1c""1"));    // couleur rouge
  //Serial1.print(number++);
  Serial1.print(F("   "));

  //Serial1.print("\x03");  (only for check sum)
  Serial1.print(F("\x04"));  // fin de transmission
  //• 1CH + “1” (31H) = Red
  //• 1CH + “2” (32H) = Green
  //• 1CH + “3” (33H) = Amber
  //• 1CH + “4” (34H) = Dim red
  //• 1CH + “5” (35H) = Dim green
  //• 1CH + “6” (36H) = Brown
  //• 1CH + “7” (37H) = Orange
  //• 1CH + “8” (38H) = Yellow
  //• 1CH + “9” (39H) = Rainbow 1
  //• 1CH + “A” (41H) = Rainbow 2
  //• 1CH + “B” (42H) = Color mix
  //• 1CH + “C” (43H) = Autocolor
}
