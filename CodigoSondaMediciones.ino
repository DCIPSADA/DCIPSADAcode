/*
 CONECCIONES SONDA 4.1

1.-LED ERROR -> 5,GND
----------------------------------
2.-DTH22 -> 6,5V,GND
----------------------------------
3.-BH1750 (LUXOMETRO)-> VCC-5V
     GND-GND
      SCL-A5
      SDA-A4
      ADDR-NO CONECTADO
----------------------------------
4.-RTC (RELOJ)->  GND->GND
                  VCC->5V
                  SDA->A4
                  SCL->A5
----------------------------------
5.-SD-> GND-GND
  VCC-5V
  CS-4
  MOSI-11
  MISO-12
  SCK-13
----------------------------------
6.-LCD->12C-> GND-GND
    VCC-5V
    SDA-A4
    SCL-A5
 */

#include <RTClib.h>  //libreria reloj ds3231
#include <Wire.h>  //libreria necesaria lcd Y LUXOMETRO
#include <LiquidCrystal_I2C.h>  //libreia lcd
#include <SimpleDHT.h>  //librería sensor humedad temperatura
#include <BH1750.h>   //librería luxómetro
#include <SPI.h>  //librería para SD
#include <SD.h>   //librería para SD
                                                                   
#define ELed 5 //definimos un led de error en el pin 5
int pinDHT22 = 6;

SimpleDHT22 dht22(pinDHT22);
RTC_DS3231 rtc;
BH1750 Luxometro;
LiquidCrystal_I2C lcd(0x27,16,2);   // dirección del LCD 0x27 lcd 16x2
File Archivo; 

//============================================================================================================================================
void setup() {
  Wire.begin(); //la biblioteca del luxómetro no hace esta función automático sin embargo puede qe ka del lcd lo haga, igual se incluye
  Serial.begin(9600);
  lcd.init();   // inicia lcd 
  lcd.backlight();

  //-----------------------------------------------------------------------------------------------------------------------------------------
  Serial.println("=================Iniciando Mediciones 4.1...=================");
  lcd.setCursor(0,0);
  lcd.print("Medicion4.1");
  delay(5000); //5 segundos me permite meter otro programa sin que inicie este
  //-----------------------------------------------------------------------------------------------------------------------------------------
  //--------------Led Error Guardando---------------------------------------------------------------------------------------
  pinMode(ELed,OUTPUT);
  digitalWrite(ELed,LOW);
  //iniciamos RTC-------------------------------------------------------------------------
  rtc.begin();  
  //------luxómetro-----------------------------------------------------------------------------------------------------------------------------------
  //iniciamos el luxómetro ONE_TIME permite que el luxometro se active para una medición sin estar de manera continua-----------------------
   if (Luxometro.begin(BH1750::ONE_TIME_HIGH_RES_MODE)) {
    Serial.println("BH1750 iniciado");
  }
  else {
    Serial.println("Error iniciando BH1750");
  }
  //---------Inicio SD----------------------------------------------------------------------------------------------------------------------
  Serial.print("Iniciando SD...");
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("IniciandoSD");
  if (!SD.begin(4)) {
    Serial.println("SD Error!");
    lcd.setCursor(0,1);
    lcd.print("ErrorInicioSD");
    digitalWrite(ELed,HIGH);
    while (1);
  }
  Serial.println("SD INiciada");
  lcd.setCursor(0,1);
  lcd.print("SD Iniciada");
  //---------Creando Documento----------------------------------------------------------------------------------------------------------------------
  if (SD.exists("DATA.CSV")) {
    Serial.println("DATA.CSV EXISTE");
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("DATA.CSV EXISTE");
    delay(1000);
  }
}

//============================================================================================================================================
void loop() {
    float T=0, H=0; //temperatura, humedad
    float lux=0, PAR=0; //luzÇ
    int fecha[3], hora[3];
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Tomando Lectura");
    Date(fecha,hora);
    HumTem(T,H);
    FPAR(lux,PAR);
    lcd.clear();
    Guardar(fecha,hora,T,H,lux,PAR);
    lcd.setCursor(0,0);
    lcd.print("Guardado");
    delay(500);
    lcd.clear();
    Imprimir(fecha,hora,T,H,lux,PAR);
    
    Serial.println("_________________________________________________");  
    delay(60000);  //<-------------------------TIEMPO ENTRE MEDICIONES
}

//============================================================================================================================================
void Date(int date[], int h[]) { //imprime la fecha y hora
  DateTime now = rtc.now();
  //fecha
  date[0]=now.day();
  date[1]=now.month();
  date[2]=now.year();
  //hora
  h[0]=now.hour();
  h[1]=now.minute();
  h[2]=now.second();
}

//============================================================================================================================================
void HumTem (float& Te, float& Hu) {

  Te=0;
  Hu=0;
  int err = SimpleDHTErrSuccess;
  for (int i=0;i<3;i++) //promedio de 3 mediciones (cada medio segundo) 
  {
    float temperature = 0;
    float humidity = 0;
    int err = SimpleDHTErrSuccess;
    if ((err = dht22.read2(&temperature, &humidity, NULL)) != SimpleDHTErrSuccess) {
      Serial.print("Read DHT22 failed, err=");
      Serial.print(SimpleDHTErrCode(err));
      Serial.print(",");
      Serial.println(SimpleDHTErrDuration(err));
      delay(2000);
      return;
    } 
    Te=Te+((float)temperature);
    Hu=Hu+((float)humidity);
    delay (500);
  }
  Te=Te/3;
  Hu=Hu/3;
}

//============================================================================================================================================
void FPAR (float& lux, float& PAR) {
  
  while (!Luxometro.measurementReady(true)) {
    yield(); // este comando asegura que no se haga la medición hasta que eñ luxómetro esté preparado
  }

  while (!Luxometro.measurementReady(true)) {
    yield();
  }
  Luxometro.configure(BH1750::ONE_TIME_HIGH_RES_MODE);

  //establece configuración de intensidad de luz automáticamente con una primera lectura---------------------------------------------------
  lux=Luxometro.readLightLevel();
  if (lux > 40000.0) {
    Luxometro.setMTreg(32); //configuración para luz solar directa (tiempo de lectura) valores sugeridos por fabricante
    lcd.setCursor(7,1);
    lcd.print("L:SOL");
    Serial.println("Luz Intensa ");
  }
  if (lux <10) {
    Luxometro.setMTreg(138); //configuración para luz escasa(tiempo de lectura)
    lcd.setCursor(7,1);
    lcd.print("L:LOW");
    Serial.println("Luz Escasa ");
  }
  else {
    Luxometro.setMTreg(69); // configuración luz normal
    lcd.setCursor(7,1);
    lcd.print("L:Normal"); 
    Serial.println("Luz Normal ");
  }
  //-------------------------------------------------------------------------------------------------------------------------------------
  
  ///promedio de lecturas-------------------------------------------------------------------------------------------------------------------
  lux = 0;
  for (int i=0;i<3;i++) //promedio de 3 mediciones (cada medio segundo) 
  {
    Luxometro.configure(BH1750::ONE_TIME_HIGH_RES_MODE); // esta linea vuelve a configurar el luxómetro pues se apaga despues de realizar la medida
    lux=lux+Luxometro.readLightLevel();
    delay (500);
  }
  lux=lux/3;
  PAR= lux*0.0185; //factor de luz solar
   //-------------------------------------------------------------------------------------------------------------------------------------
}
//============================================================================================================================================

//============================================================================================================================================
void Imprimir(int f[], int hora[], int T, int H, float lux, float PAR){
  //Serial------------------------------------------------------------------------------------------------------------------------------------
  //fecha
  Serial.print("Fecha: ");
  Serial.print(f[0]);
  Serial.print("/");
  Serial.print(f[1]);
  Serial.print("/");
  Serial.println(f[2]);
  //hora
  Serial.print("Hora: ");
  Serial.print(hora[0]);
  Serial.print(":");
  Serial.print(hora[1]);
  Serial.print(":");
  Serial.println(hora[2]);
  //Temperatura
  Serial.print("T: ");
  Serial.print(T);
  Serial.print("ºC");
  //Humedad
  Serial.print("    H: ");
  Serial.print(H);
  Serial.println("%");
  //luz
   //Temperatura
  Serial.print("L: ");
  Serial.print(lux);
  Serial.print("lm");
  //Humedad
  Serial.print("    PAR: ");
  Serial.print(PAR);
  Serial.println("umol m−2 s−1");
  
  //LCD------------------------------------------------------------------------------------------------------------------------------------
  //hora
  lcd.setCursor(0,0);
  lcd.print(hora[0]);
  lcd.print(":");
  lcd.print(hora[1]);
  lcd.print(":");
  lcd.print(hora[2]);
  //Temperatura
  lcd.setCursor(10,0);
  lcd.print("T:");
  lcd.print(T);
  lcd.print("C");
  //Humedad
  lcd.setCursor(0,1);
  lcd.print("H:");
  lcd.print(H);
  lcd.print("%");
  //Intensidad
  lcd.setCursor(6,1);
  lcd.print("PAR:");
  lcd.print(PAR); 
}
//============================================================================================================================================

void Guardar(int f[], int hora[], int T, int H, float lux, float PAR){
  // Su el archovo se abre correctamente guarda--------------------------------------------------------------------------------
  Archivo = SD.open("DATA.CSV", FILE_WRITE);
  if (Archivo) {
    Serial.println("Guardando en archivo");
    //fecha
    Archivo.print(f[0]);
    Archivo.print("/");
    Archivo.print(f[1]);
    Archivo.print("/");
    Archivo.print(f[2]);
    Archivo.print(",");  //coma para nueva columna

    //hora
    Archivo.print(hora[0]);
    Archivo.print(":");
    Archivo.print(hora[1]);
    Archivo.print(":");
    Archivo.print(hora[2]);
    Archivo.print(",");  //coma para nueva columna

    //Temperatura
    Archivo.print(T);
    Archivo.print(",");  //coma para nueva columna

    //Humedad
    Archivo.print(H);
    Archivo.print(",");  //coma para nueva columna

    //Luz
    Archivo.print(lux);
    Archivo.print(",");  //coma para nueva columna

    //PAR
    Archivo.println(PAR); //println para nueva linea
    
    //////////// close the file:
    Archivo.close();
    Serial.println("Datos Guardados");
    return;
  } 
  // Si hay error al abrir imprime error en el lcd y prende un led----------------------------------------------------------- 
  else {
    Serial.println("Error abriendo archivo");
    digitalWrite(ELed,HIGH);
    return;
  }
}
//============================================================================================================================================
