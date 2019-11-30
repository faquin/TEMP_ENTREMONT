/* 13-11-2019
 Code marche sur Xenon
 
 15-11-2019
 Code marche sur Boron
 La sim card Free a 50Mo mensuels. J'ai du d'abord configurer la 3rd party sim card (cf docs Particle) avec comme APN: FREE

 17-11-2019
 Le capteur interne de température est trop influencé par le Boron: on utilise plutôt un TMP102

 18-11-2019
 Quelques soucis avec le TMP102. Certaines fois il renvoi 255.94, qui correspond à une lecture de xFFF sur l'I2C. 
 C'est surement dû à une mauvaise connection. J'ai soudé un nouveau TMP102 et ça a l'air de marcher
    Voir https://community.particle.io/t/tmp102-and-spark/1091/24?u=fabien
 J'en profite pour utiliser la librairie SparkFunTMP102
 Je supprime la deconnection du cloud avant sleep, cela ne sert plus à rien puisque le TMP102 est déport2 du NRF
 
 21-11-2019
 Toujours des envois de -0.1 (=0xFFFF). J'ai ajouté des logs pour mesurer les MSB et LSB quand ça arrive.
 J'ai ajouté un test pour vérifier qu'on a bien reçu les 2 bytes du TMP102
 J'ajoute une librairie pour savoir comment est alimentée l'Argon: chargeur ou batterie
 
 30-11-2019
 Ajouté des retry, et ca marche depuis plusieurs jours
 
 */\
 
#define VERSION 4
#define DEBUGMODE 0
 
#include <application.h>
#include <Wire.h> // Used to establied serial communication on the I2C bus
#include <SparkFunTMP102.h> // Used to send and recieve specific information from our sensor
#include <DiagnosticsHelperRK.h>
#include <ParticleWebLog.h>
// Install our logger
ParticleWebLog particleWebLog;



#define SEUIL_TEMP_BAS 5    // °C


int tmp102Address = 0x48; // 0x90; //0x48;

const int interval = 3600; // seconds => 1h
unsigned long TimeLastMeasure=0;
unsigned long TimeNextSleep=0;

bool bAlerte = false;


float getTemperature();


void setup(){
    Serial.begin(115200);

    // turn off core LED
//	RGB.control(true);
//	RGB.color(0, 0, 0);

    Wire.begin();

 
	TimeNextSleep=Time.now() + interval;
	
	if(DEBUGMODE){
	    Log.info("In setup v%d", VERSION);
	}
}

void loop(){
    
    int sleep_duration;
    float celsius=-100;
    
    
    checkPower();   // verifie qu'on est pas sur batterie

    //celsius = getTemperature102();
    celsius = getTemperature();
    if(DEBUGMODE){
        Log.info("Temp102=%.1f", celsius);

        Serial.print("loop celsius=");
        Serial.println(celsius);
    }
 
    Particle.publish("tempEntremont", String::format("%.2f",celsius), PRIVATE);
    
    if((celsius < SEUIL_TEMP_BAS) && (bAlerte==false)){
        bAlerte=true;
        Particle.publish("AlerteTempEntremont", String::format("%.1f",celsius), PRIVATE);   // on envoi un email/sms
        if(DEBUGMODE){
            Serial.print("AlerteTempEntremont");
            Serial.println(celsius);
        }
    }
    if((celsius > SEUIL_TEMP_BAS) && (bAlerte==true)){
        bAlerte=false;
    }
    
/*    
    // on coupe le cloud avant de partir en sleep, pour redemarrer plus vite
    Particle.disconnect();
    waitUntil(Particle.disconnected);
    Cellular.off();
    delay(6000);    // on attend la deconnection du cellular
*/ 
    delay(10000); //attente publish pour limitation cloud
    sleep_duration = TimeNextSleep - Time.now();
    TimeNextSleep = TimeNextSleep + interval;

     
    System.sleep({}, {}, sleep_duration); // on attends interval secondes entre deux sleep

}




void checkPower(){
    // https://community.particle.io/t/mesh-argon-xenon-detecting-usb-power-loss/46221/14?u=fabien
    
    int powerSource = DiagnosticsHelper::getValue(DIAG_ID_SYSTEM_POWER_SOURCE);
    FuelGauge fuel;
        // POWER_SOURCE_UNKNOWN = 0,
    	// POWER_SOURCE_VIN = 1,
    	// POWER_SOURCE_USB_HOST = 2,
    	// POWER_SOURCE_USB_ADAPTER = 3,
    	// POWER_SOURCE_USB_OTG = 4,
    	// POWER_SOURCE_BATTERY = 5
    
    if (powerSource == 5) {  // sur batterie
       Particle.publish("EntremontPower", String::format("%.0f",fuel.getSoC()), PRIVATE);
    }

}



float getTemperature(){
    float celsius;
    int nBoucles=0;
    int16_t digitalTemp;
    
    do{
        delay(250);
        Wire.requestFrom(tmp102Address,2);
        nBoucles++;
        if(nBoucles==30){    // 30 retry
            break;
        }
    }while(Wire.available()==0);
    
    
    if(nBoucles<30){

        char MSB = Wire.read();
        char LSB = Wire.read();
        //Log.info("MSB= %d", MSB);
        //Log.info("LSB= %d", LSB);
    
    	// Combine bytes to create a signed int 
        digitalTemp = (MSB << 4) | (LSB >> 4);
    	// Temperature data can be + or -, if it should be negative,
    	// convert 12 bit to 16 bit and use the 2s compliment.
        if(digitalTemp > 0x7FF)
    	{
          digitalTemp |= 0xF000;
        }
        
        celsius = digitalTemp * 0.0625;
    
    
        if(DEBUGMODE){
            Serial.print("getTemperature celsius=");
            Serial.println(celsius);
            Log.info("nBoucles=%d",nBoucles);
        }
    }
    else{
        Log.info("No I2C byte ready");
        celsius=100;
    }
    return celsius;
}
