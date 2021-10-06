/*
 CONECCIONES Control 1.0
 
4.-NRF24->  GND->GND
            VCC->LD33v->5v
            CSN->10
            CE->8
            MOSI->11
            SCK->13
            IRQ(noAPLICA)
            MISO->12
----------------------------------
2.-DTH22 -> 6,5V,GND
----------------------------------
*/ 
#include <SPI.h>  // incluye libreria SPI para comunicacion con el modulo
#include <RH_NRF24.h> // incluye la seccion NRF24 de la libreria RadioHead
#include <SimpleDHT.h>  //librería sensor humedad temperatura
#include <BH1750.h>   //librería luxómetro

#define ReleLuz 2
#define ReleExtractores 3
#define ReleBombaPaneles 4
#define ReleCalefaccion 5
#define ReleVentilacion 7

int pinDHT22 = 6; //pin donde se conecta el pin de Data de sensor DTH22
SimpleDHT22 dht22(pinDHT22);
RH_NRF24 nrf24;   // crea objeto con valores por defecto para bus SPI
      // y pin digital numero 8 para CE 
BH1750 Luxometro;
      
int MedicionesDia=0, MedicionesNoche=0;
float DLI=0, SumPAR=0, horaOff=0, amanecer=0, puestaSol=0, duracionDia=0, HoraUltimoControl=-1;
bool RefrigeracionOn = false, RefrigeracionSecaOn = false, CalefaccionOn = false, VentilacionOn=true, MeterHumedadOn = false, MeterHumedadPanelOn = false; 

void setup() 
{
  Wire.begin(); //la biblioteca del luxómetro no hace esta función automático sin embargo puede qe ka del lcd lo haga, igual se incluye
  Serial.begin(9600);   // inicializa monitor serie a 9600 bps
  //-----Inicio Relés----------------------------------------------------------------------------------------------------------------------------------
  pinMode(ReleLuz, OUTPUT);
  pinMode(ReleExtractores, OUTPUT);
  pinMode(ReleBombaPaneles, OUTPUT);
  pinMode(ReleCalefaccion, OUTPUT);
  pinMode(ReleVentilacion, OUTPUT);

  digitalWrite(ReleLuz, LOW);
  digitalWrite(ReleExtractores, LOW);
  digitalWrite(ReleBombaPaneles, LOW);
  digitalWrite(ReleCalefaccion, LOW);
  digitalWrite(ReleVentilacion, HIGH);
    
  //------Inicio NRF24-----------------------------------------------------------------------------------------------------------------------------------
  if (!nrf24.init())    // si falla inicializacion de modulo muestra texto
    Serial.println("fallo de inicializacion");
  if (!nrf24.setChannel(2)) // si falla establecer canal muestra texto
    Serial.println("fallo en establecer canal");
  if (!nrf24.setRF(RH_NRF24::DataRate250kbps, RH_NRF24::TransmitPower0dBm)) // si falla opciones 
    Serial.println("fallo en opciones RF");             // RF muestra texto
     
    Serial.println("Base iniciada");  // texto para no comenzar con ventana vacia
    Serial.println("=========================");  // muestra texto
    delay(1000);
  //------Inicio luxómetro-----------------------------------------------------------------------------------------------------------------------------------
  //iniciamos el luxómetro ONE_TIME permite que el luxometro se active para una medición sin estar de manera continua-----------------------
   if (Luxometro.begin(BH1750::ONE_TIME_HIGH_RES_MODE)) {
    Serial.println("BH1750 iniciado");
  }
  else {
    Serial.println("Error iniciando BH1750");
  }
}

void loop()
{
    float Text=0, Hext=0; //temperatura y humedad exterior
    uint8_t buf[RH_NRF24_MAX_MESSAGE_LEN];     // buffer de max posiciones
    uint8_t buflen = sizeof(buf); // obtiene longitud del buffer
    float datos[6]; //arreglo de almacenamiento de los datos
    int j=0, k=0; // [j->indice que indica la última posición de la ","] [k->contador para posición de arreglo]
    
    if (nrf24.recv(buf, &buflen)) // si hay informacion valida disponible
    { 
      String str_datos = String((char*)buf);                  // almacena en str_datos datos recibidos
      for(int i = 0; i < str_datos.length(); i++){            //recorre toda la cadena de caracteres
        if(k<6){                                              //si el num. de posición es menor a 6
          if (str_datos.substring(i, i+1) == ",") {           //si hay una coma en el siguiente indice
            datos[k] = str_datos.substring(j, i).toFloat();   //guarda los datos en float desde la ultima posición de la coma hasta la siguiente, en el arreglo datos[k]
            j=i+1;                                            //j->nueva posición de coma
            k++;                                              //la posición del arreglo aumenta en 1        
          }
        }
        else break;                                           //si k=6 rompe el ciclo
      }
    }

    float hora=datos[0]+(datos[1]/60);
    if (hora>HoraUltimoControl){ //HoraUltimoControl=-1 inicia como una variable global (el "-1" se debe a que si la hora recibida es 00:00 entonces si cumple la condicional)
        HumTem(Text,Hext);
        FDLI(hora, datos[5]); //datos[5] es el PAR
        ControlTemperatura(Text, datos[3], Hext, datos[4]); //datos[3]->Temperatura Interior datos[4]->Humedad Interior
        Ventilacion();
        Humedad(Text, datos[3], Hext, datos[4]); //datos[3]->Temperatura Interior datos[4]->Humedad Interior
        HoraUltimoControl=hora;//establece última hora de control
        if (HoraUltimoControl>=23.983){ //después de las 23:59 horas (23.983h) el reloj regresa a las 00:00 horas 
                                        //por lo que la variable hora tambien regresa a 0 y hay que reiniciar la variable HoraUltimoControl=-1
                                        //para que la condicional de la linea 96 vuelva a cumplirse
          delay (60000);
          HoraUltimoControl=-1;
        }
    }
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

//======================================================Funciones de control de luz========================================================
void FDLI(float& hora, float& PAR){
  
  //prepara luxometro -----------------------------------------------------------------------------------------------------------------------
  float LuxExt=0;
  while (!Luxometro.measurementReady(true)) {
    yield(); // este comando asegura que no se haga la medición hasta que eñ luxómetro esté preparado
  }

  while (!Luxometro.measurementReady(true)) {
    yield();
  }
  Luxometro.configure(BH1750::ONE_TIME_HIGH_RES_MODE);

  //establece configuración de intensidad de luz automáticamente con una primera lectura---------------------------------------------------
  LuxExt=Luxometro.readLightLevel();
  if (LuxExt > 40000.0) {
    Luxometro.setMTreg(32); //configuración para luz solar directa (tiempo de lectura) valores sugeridos por fabricante
    Serial.println("Luz Intensa ");
  }
  if (LuxExt <10) {
    Luxometro.setMTreg(138); //configuración para luz escasa(tiempo de lectura)
    Serial.println("Luz Escasa ");
  }
  else {
    Luxometro.setMTreg(69); // configuración luz normal
    Serial.println("Luz Normal ");
  }
  //promedio de 3 lecturas-------------------------------------------------------------------------------------------------------------------
  LuxExt = 0;
  for (int i=0;i<3;i++) //promedio de 3 mediciones (cada medio segundo) 
  {
    Luxometro.configure(BH1750::ONE_TIME_HIGH_RES_MODE); // esta linea vuelve a configurar el luxómetro pues se apaga despues de realizar la medida
    LuxExt=LuxExt+Luxometro.readLightLevel();
    delay (500);
  }
  LuxExt=LuxExt/3; 
  //Control Luz----------------------------------------------------------------------------------------------------------
  if (LuxExt>51.3){ //LuxExt mayor a 51.3 indica que es de día
        SumPAR=SumPAR+PAR;
        MedicionesDia++;
        if (MedicionesDia==1) {
            amanecer=hora;
            MedicionesNoche=0;
            horaOff=0;
        }
    }
    if (LuxExt<51.3) { //LuxExt menor a 51.3 indica que es de noche
        float promPAR=SumPAR/MedicionesDia;
        MedicionesNoche++;
        if (MedicionesNoche=1) {
            puestaSol=hora;
            duracionDia=puestaSol-amanecer;
            DLI=(promPAR*duracionDia*3600)/1000000;
            luzArt();
            MedicionesDia=0;
            digitalWrite(ReleLuz, HIGH);
        }  
    }

    if (horaOff!=0 && hora>=horaOff){
        digitalWrite(ReleLuz, LOW);
    }
}
void luzArt() {
    //determina la hora en que se apaga la luz artificial
    float DLIart=26-DLI;  //26 DLI óptimo
    float horasArt=(1000000*DLIart)/(500.75*3600); 
    horaOff=puestaSol+horasArt;
    float totalHorasLuz=duracionDia+horasArt;
    if (totalHorasLuz>16) {//16 es el maximo de horas para el pimiento
        horaOff=horaOff-(totalHorasLuz-16);
    }
    if (horaOff>24){
        horaOff=horaOff-24;
    }
}
//======================================================Función de control de Temperatura========================================================
void ControlTemperatura(float& Text, float& Tint, float& Hext, float& Hint){
  //----------------------------Refrigeración--------------------------------------------------------
  //--------------------Día------------------------------------------------------------------------
  if (MedicionesDia>0 && Tint>29 && Text<28 && Hext>=50){ //si la temperatura exterior es menor a 28 y la humedad exterior es mayor a 50 no hara falta prender el agua en los paneles
     digitalWrite(ReleExtractores, HIGH); //prende extractores de refrigeración
     RefrigeracionSecaOn = true;     
  }
  else if (MedicionesDia>0 && RefrigeracionSecaOn == true && Tint<=28){
     digitalWrite(ReleExtractores, LOW); //apaga extractores de refrigeración
     RefrigeracionSecaOn = false;
  }
  else if (MedicionesDia>0 && Tint>29 && Text>29){
     digitalWrite(ReleExtractores, HIGH); //prende extractores de refrigeración
     digitalWrite(ReleBombaPaneles, HIGH); //prende bomba de los páneles humedos
     RefrigeracionOn = true;     
  }
  else if (MedicionesDia>0 && RefrigeracionOn == true && Tint<=25.5){
     digitalWrite(ReleExtractores, LOW); //apaga extractores de refrigeración
     digitalWrite(ReleBombaPaneles, LOW); //apaga bomba de los páneles humedos
     RefrigeracionOn = false;
  }
  //--------------------Noche------------------------------------------------------------------------
  else if (MedicionesNoche>0 && Tint>19 && Text<18 && Hext>=50){ //si la temperatura exterior es menor a 28 y la humedad exterior es mayor a 50 no hara falta prender el agua en los paneles
     digitalWrite(ReleExtractores, HIGH); //prende extractores de refrigeración
     RefrigeracionSecaOn = true;     
  }
  else if (MedicionesNoche>0 && RefrigeracionSecaOn == true && Tint<=18){
     digitalWrite(ReleExtractores, LOW); //apaga extractores de refrigeración
     RefrigeracionSecaOn = false;
  }
  else if (MedicionesNoche>0 && Tint>19 && Text>19){
     digitalWrite(ReleExtractores, HIGH); //prende extractores de refrigeración
     digitalWrite(ReleBombaPaneles, HIGH); //bomba de los páneles humedos
     RefrigeracionOn = true;     
  }
  else if (MedicionesNoche>0 && RefrigeracionOn == true && Tint<=17){
    digitalWrite(ReleExtractores, LOW); //apaga extractores de refrigeración
    digitalWrite(ReleBombaPaneles, LOW); //apaga bomba de los páneles humedos
    RefrigeracionOn = false;
  }
  //----------------------------Calefacción--------------------------------------------------------
  //--------------------Día------------------------------------------------------------------------
  else if (MedicionesDia>0 && Tint<22){
     digitalWrite(ReleCalefaccion, HIGH); //prende calefacción
     CalefaccionOn = true;     
  }
  else if (MedicionesDia>0 && CalefaccionOn == true && Tint>=24){
     digitalWrite(ReleCalefaccion, LOW); //apaga calefacción
     CalefaccionOn = false;
  }
  //--------------------Noche------------------------------------------------------------------------
  else if (MedicionesNoche>0 && Tint<15){
     digitalWrite(ReleCalefaccion, HIGH); //prende calefacción
     CalefaccionOn = true;     
  }
  else if (MedicionesNoche>0 && CalefaccionOn == true && Tint>=17){
    digitalWrite(ReleCalefaccion, LOW); //apaga calefacción
    CalefaccionOn = false;
  }
}
//======================================================Función de control de Temperatura========================================================
void Ventilacion(){
  if (RefrigeracionOn == true || RefrigeracionSecaOn == true){
    digitalWrite(ReleVentilacion, LOW); //apaga la ventilación en caso de que los extractores del sistema de refrigeración estén encendidos
    VentilacionOn=false;
  }
  else if (VentilacionOn=false && RefrigeracionOn==false && RefrigeracionSecaOn == false){
    digitalWrite(ReleVentilacion, HIGH); //prende la ventilación en caso de que los extractores del sistema de refrigeración estén apagados
    VentilacionOn=true;
  }  
}
void Humedad(float& Text, float& Tint, float& Hext, float& Hint){
   //----------------------------Meter Humedad del exterior--------------------------------------------------------
  //--------------------Día------------------------------------------------------------------------
  if (RefrigeracionOn == false && RefrigeracionSecaOn == false && CalefaccionOn == false && MedicionesDia>0 && Hint<50 && Tint>25.5 && Text>22 && Text<29 && Hext>50 && Hext<70){
     digitalWrite(ReleExtractores, HIGH); //prende extractores de refrigeración
     MeterHumedadOn = true;     
  }
  else if (MedicionesDia>0 && MeterHumedadOn == true && (Hint>50 || Tint<23)){
     digitalWrite(ReleExtractores, LOW); //apaga extractores de refrigeración
     MeterHumedadOn = false;  
  }
  //--------------------Noche------------------------------------------------------------------------
  else if (RefrigeracionOn == false && RefrigeracionSecaOn == false && CalefaccionOn == false && MedicionesNoche>0 && Hint<50 && Tint>17 && Text>15 && Text<19 && Hext>50 && Hext<70){
     digitalWrite(ReleExtractores, HIGH); //prende extractores de refrigeración
     MeterHumedadOn = true;     
  }
  else if (MedicionesNoche>0 && MeterHumedadOn == true && (Hint>50 || Tint<16)){
     digitalWrite(ReleExtractores, LOW); //apaga extractores de refrigeración
     MeterHumedadOn = false;  
  }
     //----------------------------Aumentar humedad con paneles--------------------------------------------------------
  //--------------------Día------------------------------------------------------------------------
  if (RefrigeracionOn == false && RefrigeracionSecaOn == false && CalefaccionOn == false && MedicionesDia>0 && Hint<50 && Tint>25.5 && Text>22 && Hext<50){
     digitalWrite(ReleExtractores, HIGH); //prende extractores de refrigeración
     digitalWrite(ReleBombaPaneles, HIGH); //prende bomba de los páneles humedos
     MeterHumedadOn = true;     
  }
  else if (MedicionesDia>0 && MeterHumedadOn == true && (Hint>50 || Tint<23)){
     digitalWrite(ReleExtractores, LOW); //apaga extractores de refrigeración
     digitalWrite(ReleBombaPaneles, LOW); //apaga bomba de los páneles humedos
     MeterHumedadPanelOn = false;   
  }
  //--------------------Noche------------------------------------------------------------------------
  else if (RefrigeracionOn == false && RefrigeracionSecaOn == false && CalefaccionOn == false && MedicionesNoche>0 && Hint<50 && Tint>17 && Text>15 && Hext<50){
     digitalWrite(ReleExtractores, HIGH); //prende extractores de refrigeración
     digitalWrite(ReleBombaPaneles, HIGH); //prende bomba de los páneles humedos
     MeterHumedadPanelOn = true;     
  }
  else if (MedicionesNoche>0 && MeterHumedadOn == true && (Hint>50 || Tint<16)){
     digitalWrite(ReleExtractores, LOW); //apaga extractores de refrigeración
     digitalWrite(ReleBombaPaneles, LOW); //apaga bomba de los páneles humedos
     MeterHumedadPanelOn = false;  
  }
}
