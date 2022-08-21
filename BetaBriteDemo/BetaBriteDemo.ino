void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial1.begin(2400,SERIAL_7E1);
  //Serial1.set_tx(15); //D6 ne fonctionne pas
  delay(1000);
}

int number = 1;

void loop() {
  // put your main code here, to run repeatedly:
  for (int N=0;  N<8; N++) Serial1.write(0);  // setup speed (mandatory)
  Serial1.print("\x01""Z00");  // all display (mandatory)
  Serial1.print("\x02""AA");   // write to A file (mandatory)
  Serial1.print("\x1b"" a");   // mode defilant
  Serial1.print("\x1c""3");    // couleur ambre
  Serial1.print("Le bruit de la mer ...  ");
  Serial1.print("\x1c""1");    // couleur ambre
  Serial1.print(number++);
  Serial1.print("                                     ");
  Serial.println("Le bruit de la mer ...     ");
  //Serial1.print("\x03");  (only for check sum)
  Serial1.print("\x04");  // fin de transmission
  delay(5000);

}
