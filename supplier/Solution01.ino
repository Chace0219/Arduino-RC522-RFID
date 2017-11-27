
#include <SPI.h>
#include <MFRC522.h>

#include "Nextion.h"
#include "FiniteStateMachine.h"

#include "FBD.h"

const uint8_t RST_PIN = 9;
const uint8_t SS_PIN = 10;
MFRC522 mfrc522(SS_PIN, RST_PIN);  // Create MFRC522 instance

const uint8_t BUZZER = 7;
const uint8_t RELAY = 6;

#define RELAYON LOW
#define RELAYOFF HIGH

#define BUZZERON LOW
#define BUZZEROFF HIGH

#define DELAYINSCREEN 30000

TP buzzerTP;

// Prepare key - all keys are set to FFFFFFFFFFFFh at chip delivery from the factory.
MFRC522::MIFARE_Key key;
byte buffer[18];

typedef struct
{
	char userid[12];
	uint32_t restwater;
}UserInfo;

static uint32_t orderAmount = 0; //
static uint32_t currentAmount = 0; //
static uint32_t nStartRelayTime = 0;
static uint32_t prevTime = 0;

static uint32_t timePerLiter = 1500; // 
static bool bSupplying = false;


void homeEnter();
void homeUpdate();

void cardinfoEnter();
void cardinfoUpdate();

void splashEnter();
void splashUpdate();

void orderEnter();
void orderUpdate();

void supplyEnter();
void supplyUpdate();
void supplyExit();

void controlSleep(bool enable = true);

State splash(splashEnter, splashUpdate, NULL);

State home(homeEnter, homeUpdate, NULL);
State cardinfo(cardinfoEnter, cardinfoUpdate, NULL);

State order(orderEnter, orderUpdate, supplyExit);

State supply(supplyEnter, supplyUpdate, NULL);

FiniteStateMachine statusController(splash);
//FiniteStateMachine statusController(home);

UserInfo user;

/* Nextion variables */
NexPage splashPage = NexPage(0, 0, "splash");

NexPage homePage = NexPage(1, 0, "home");
NexButton actionButton = NexButton(1, 8, "actionsetting");


NexPage userinfoPage = NexPage(2, 0, "userinfo");
NexText useridInfoPage = NexText(2, 10, "userid");
NexNumber restInfoPage = NexNumber(2, 12, "restwater");
NexButton backInfoPage = NexButton(2, 8, "back");
NexButton orderInfoPage = NexButton(2, 9, "order");

NexPage orderPage = NexPage(3, 0, "purchasing");
NexText useridOrder = NexText(3, 9, "userid");
NexNumber restOrder = NexNumber(3, 16, "restwater");
NexNumber orderOrder = NexNumber(3, 17, "order");
NexButton backOrder = NexButton(3, 8, "back");
NexCrop statusOrder = NexCrop(3, 18, "status");

NexPage supplyPage = NexPage(4, 0, "supplying");
NexText useridSupply = NexText(4, 9, "userid");
NexNumber restwaterSupplyPage = NexNumber(4, 11, "restwater");
NexNumber orderSupplyPage = NexNumber(4, 12, "order");
NexNumber currentSupplyPage = NexNumber(4, 13, "current");
NexButton pauseSupplyPage = NexButton(4, 8, "pause");

NexTouch *nex_listen[] =
{
	&actionButton,
	&backInfoPage,
	&orderInfoPage,
	&backOrder,
	&pauseSupplyPage,
	NULL
};

void touchInit()
{
	backInfoPage.attachPop(backbuttonCallback);
	orderInfoPage.attachPop(orderInfoPageCallback);
	backOrder.attachPop(backbuttonCallback);

	pauseSupplyPage.attachPop(pauseCallback);
}


void backbuttonCallback(void *ptr)
{
	statusController.transitionTo(home);
}

void orderInfoPageCallback(void *ptr)
{
	statusController.transitionTo(order);
}


void initFBD()
{
	buzzerTP.IN = false;
	buzzerTP.Q = false;
	buzzerTP.PRE = false;
	buzzerTP.ET = millis();
	buzzerTP.PT = 200;
}

void setup() 
{
	Serial.begin(9600);		// Initialize serial communications with the PC
	SPI.begin();			// Init SPI bus
	mfrc522.PCD_Init();		// Init MFRC522
	delay(2500);

	nexInit();
	touchInit();

	SPI.begin();			// Init SPI bus
	mfrc522.PCD_Init();		// Init MFRC522
	Serial.println(F("Scan PICC to see UID, SAK, type, and data blocks..."));

	pinMode(BUZZER, OUTPUT);
	pinMode(RELAY, OUTPUT);
	digitalWrite(BUZZER, BUZZEROFF);
	digitalWrite(RELAY, RELAYOFF);

	initFBD();
	
	// key init
	for (byte i = 0; i < 6; i++)
		key.keyByte[i] = 0xFF;

}

void loop() 
{
	// 
	nexLoop(nex_listen);
	statusController.update();

	TPFunc(&buzzerTP);
	buzzerTP.IN = false;

	if (buzzerTP.Q)
		digitalWrite(BUZZER, BUZZERON);
	else
		digitalWrite(BUZZER, BUZZEROFF);
}

void homeEnter()
{
	Serial.println(F("home entered!"));
	homePage.show();
	controlSleep();
}

void homeUpdate()
{

	byte blockNumber = 4; // (sector 1 block 0)
	byte len = 18;

	MFRC522::StatusCode status;

	//-------------------------------------------
	// Look for new cards
	if (!mfrc522.PICC_IsNewCardPresent())
	{
		return;
	}

	// Select one of the cards
	if (!mfrc522.PICC_ReadCardSerial()) {
		return;
	}

	status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, blockNumber, &key, &(mfrc522.uid)); //line 834 of MFRC522.cpp file
	if (status != MFRC522::STATUS_OK)
	{
		Serial.print(F("Authentication failed: "));
		Serial.println(mfrc522.GetStatusCodeName(status));
		return;
	}

	status = mfrc522.MIFARE_Read(blockNumber, buffer, &len);
	if (status != MFRC522::STATUS_OK)
	{
		Serial.print(F("Reading failed: "));
		Serial.println(mfrc522.GetStatusCodeName(status));
		return;
	}

	memcpy((void*)&user, (void*)buffer, sizeof(UserInfo));

	// user information
	Serial.print(F("user id is "));
	Serial.println(user.userid);
	Serial.print(F("reserved water is "));
	Serial.print(user.restwater);
	Serial.println(F("liter."));
	delay(100); //change value if you want to read cards faster
	mfrc522.PICC_HaltA();
	mfrc522.PCD_StopCrypto1();
	statusController.transitionTo(cardinfo);
}

void cardinfoEnter()
{
	Serial.println(F("cardinfo entered!"));
	userinfoPage.show();
	
	useridInfoPage.setText(user.userid);
	restInfoPage.setValue(user.restwater);
	controlSleep(false);
	buzzerTP.IN = true;
}

void cardinfoUpdate()
{
	if (statusController.timeInCurrentState() > DELAYINSCREEN)
		statusController.transitionTo(home);
}

void splashEnter()
{
	splashPage.show();
}

void splashUpdate()
{
	if (statusController.timeInCurrentState() > 2000)
	{
		statusController.transitionTo(home);
	}
}


void orderEnter()
{
	orderPage.show();
	restOrder.setValue(user.restwater);
	useridOrder.setText(user.userid);

	orderOrder.setValue(0);
}

void orderUpdate()
{
	if (statusController.timeInCurrentState() > DELAYINSCREEN)
		statusController.transitionTo(home);

	byte blockNumber = 4; // (sector 1 block 0)
	byte len = 18;

	MFRC522::StatusCode status;

	//-------------------------------------------
	// Look for new cards
	if (!mfrc522.PICC_IsNewCardPresent())
	{
		return;
	}

	// Select one of the cards
	if (!mfrc522.PICC_ReadCardSerial()) {
		return;
	}

	status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, blockNumber, &key, &(mfrc522.uid)); //line 834 of MFRC522.cpp file
	if (status != MFRC522::STATUS_OK)
	{
		Serial.print(F("Authentication failed: "));
		Serial.println(mfrc522.GetStatusCodeName(status));
		return;
	}

	status = mfrc522.MIFARE_Read(blockNumber, buffer, &len);
	if (status != MFRC522::STATUS_OK)
	{
		Serial.print(F("Reading failed: "));
		Serial.println(mfrc522.GetStatusCodeName(status));
		return;
	}

	buzzerTP.IN = true;
	UserInfo tempuser;
	memcpy((void*)&tempuser, (void*)buffer, sizeof(UserInfo));

	if (strcmp(user.userid, tempuser.userid) != 0)
	{
		Serial.println(F("Invalid card detected!"));
		statusOrder.setPic(17);
	}
	else
	{
		statusOrder.setPic(4);
		uint32_t orderFlow;
		orderOrder.getValue(&orderFlow);
		if (tempuser.restwater < orderFlow)
		{
			Serial.println(F("Ordering amount is invalid!"));
			statusOrder.setPic(18);
		}
		else
		{
			user.restwater = tempuser.restwater - orderFlow;
			orderAmount = orderFlow;
			if (orderAmount > 0)
			{
				status = mfrc522.MIFARE_Write(blockNumber, (byte*)&user, 16);
				if (status != MFRC522::STATUS_OK)
				{
					Serial.print(F("writing failed: "));
					Serial.println(mfrc522.GetStatusCodeName(status));
					return;
				}
				statusController.transitionTo(supply);
			}
		}
	}

	delay(100); //change value if you want to read cards faster
	mfrc522.PICC_HaltA();
	mfrc522.PCD_StopCrypto1();
}

void supplyEnter()
{
	supplyPage.show();
	restwaterSupplyPage.setValue(user.restwater);
	useridSupply.setText(user.userid);
	orderSupplyPage.setValue(orderAmount);
	currentSupplyPage.setValue(0);
	currentAmount = 0;

	nStartRelayTime = millis();
	prevTime = 0;
	digitalWrite(RELAY, RELAYON);
	bSupplying = true;
}

void supplyUpdate()
{
	static uint32_t nLastDisplayTime = millis();
	if (bSupplying)
	{
		if ((millis() - nStartRelayTime) + prevTime > timePerLiter * orderAmount)
		{
			currentAmount = ((millis() - nStartRelayTime) + prevTime) / timePerLiter;
			currentSupplyPage.setValue(currentAmount);
			digitalWrite(RELAY, RELAYOFF);
			statusController.transitionTo(home);
		}

		if (millis() - nLastDisplayTime > 1000)
		{
			currentAmount = ((millis() - nStartRelayTime) + prevTime) / timePerLiter;;
			currentSupplyPage.setValue(currentAmount);
			nLastDisplayTime = millis();
		}
	}
}

void supplyExit()
{
	digitalWrite(RELAY, RELAYOFF);
	bSupplying = false;
}

void pauseCallback(void *ptr)
{
	if (statusController.isInState(supply))
	{
		if (bSupplying)
		{
			bSupplying = false;
			digitalWrite(RELAY, RELAYOFF);
			prevTime += millis() - nStartRelayTime;
			pauseSupplyPage.Set_background_image_pic(9);
			pauseSupplyPage.Set_background_crop_picc(8);

			Serial.println("supply paused");
		}
		else
		{
			bSupplying = true;
			digitalWrite(RELAY, RELAYON);
			nStartRelayTime = millis();
			pauseSupplyPage.Set_background_image_pic(7);
			pauseSupplyPage.Set_background_crop_picc(6);

			Serial.println("supply resumed");
		}
	}
}

void controlSleep(bool enable)
{
	String cmd = "";
	if (enable)
		cmd = "thsp=30";
	else
		cmd = "thsp=65535";
	sendCommand(cmd.c_str());
	recvRetCommandFinished();

	cmd = "thup=1";
	sendCommand(cmd.c_str());
	recvRetCommandFinished();
}
