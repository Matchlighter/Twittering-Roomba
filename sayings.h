#define NumPickedUpStrings 2 //Number of Strings in OnPickedUp. It is unfortunate that we must do this, but there is no way of getting the number of items in an array
char* OnPickedUp[NumPickedUpStrings] = {"Hey! Put me down!","I don't like being picked up!"}; //Messages to choose from when the Roomba is picked up

#define NumDockedStrings 1
char* OnDocked[NumDockedStrings] = {"Ahh! Dock sweet dock"}; //Same but for Docked

#define NumLowBatStrings 2
char* OnBatteryLow[NumLowBatStrings] = {"Oh my, my battery is almost dead...","I am getting tired..."}; //When the battery is low

#define NumChargeFinishStrings 2
char* OnChargeFinish[NumChargeFinishStrings] = {"I feel so refreshed now!","It feels good to be recharged!"}; //When charge has completed

#define NumCliffStrings 1
char* OnCliffDetect[NumCliffStrings] = {"Whoa! Just about fell off a cliff!"}; //When a cliff detect sensor indicates a cliff

#define NumCleanStrings 1
char* OnClean[NumCleanStrings] = {"Time to clean!"}; //Clean Button is pressed

#define NumCleanOnLowStrings 1
char* OnCleanOnLowBat[NumCleanOnLowStrings] = {"Come on! I am too tired to clean now!"}; //Asked to clean when the battery is low

#define NumSpotCleanStrings 1
char* OnSpotClean[NumSpotCleanStrings] = {"Spot clean!"}; //Spot Clean Button
