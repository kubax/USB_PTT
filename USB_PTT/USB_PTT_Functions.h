void UpdateWhisperLists();

#ifdef _WIN32
#define _strcpy(dest, destSize, src) strcpy_s(dest, destSize, src)
#define snprintf sprintf_s
#else
#define _strcpy(dest, destSize, src) { strncpy(dest, src, destSize-1); dest[destSize-1] = '\0'; }
#endif

#define PLUGIN_API_VERSION 16

#define PATH_BUFSIZE 512
#define COMMAND_BUFSIZE 128
#define INFODATA_BUFSIZE 128
#define SERVERINFO_BUFSIZE 256
#define CHANNELINFO_BUFSIZE 512
#define RETURNCODE_BUFSIZE 128
#define REQUESTCLIENTMOVERETURNCODES_SLOTS 5

char iniPath[PATH_BUFSIZE];
char WhisperiniPath[PATH_BUFSIZE] = "";


static char* pluginID = NULL;

/* Array for request client move return codes. See comments within ts3plugin_processCommand for details */
static char requestClientMoveReturnCodes[REQUESTCLIENTMOVERETURNCODES_SLOTS][RETURNCODE_BUFSIZE];


static struct TS3Functions ts3Functions;
int m_glbEndThreadNow = 0;
BOOL TALK_PEDAL_TOGGLE = false;
BOOL WHISPER_PEDAL_TOGGLE[7];
BOOL USB_NOT_FOUND = false;
const int MAX_WHISPER_LISTS = 7;
const int MAX_WHISPER_LISTS_LENGTH = 20;
anyID targetClientIDArray[MAX_WHISPER_LISTS][MAX_WHISPER_LISTS_LENGTH];


class WHISPER_LIST_CLASS {
public:
	std::string UID;
	std::string Nickname;
	anyID ClientID;
};
WHISPER_LIST_CLASS WhisperLists[MAX_WHISPER_LISTS][MAX_WHISPER_LISTS_LENGTH];

int PTTUSB_LED(int LED,int status)
{
	usb_dev_handle      *handle = NULL;
const unsigned char rawVid[2] = {USB_CFG_VENDOR_ID}, rawPid[2] = {USB_CFG_DEVICE_ID};
char                vendor[] = {USB_CFG_VENDOR_NAME, 0}, product[] = {USB_CFG_DEVICE_NAME, 0};
char                buffer[4];
int                 cnt, vid, pid;


    usb_init();
	
	/* compute VID/PID from usbconfig.h so that there is a central source of information */
    vid = rawVid[1] * 256 + rawVid[0];
    pid = rawPid[1] * 256 + rawPid[0];
    /* The following function is in opendevice.c: */
    if(usbOpenDevice(&handle, vid, vendor, pid, product, NULL, NULL, NULL) != 0){
        fprintf(stderr, "PLUGIN: USB_PTT ERROR ( 0x%02X ): Could not find USB device \"%s\" with vid=0x%x pid=0x%x\n",VUSB_ERROR, product, vid, pid);
		return -1;
    }
    /* Since we use only control endpoint 0, we don't need to choose a
     * configuration and interface. Reading device descriptor and setting a
     * configuration and interface is done through endpoint 0 after all.
     * However, newer versions of Linux require that we claim an interface
     * even for endpoint 0. Enable the following code if your operating system
     * needs it: */
#if 0
    int retries = 1, usbConfiguration = 1, usbInterface = 0;
    if(usb_set_configuration(handle, usbConfiguration) && showWarnings){
        fprintf(stderr, "PLUGIN: USB_PTT ERROR ( 0x%02X ): Warning: could not set configuration: %s\n",VUSB_ERROR, usb_strerror());
    }
    /* now try to claim the interface and detach the kernel HID driver on
     * Linux and other operating systems which support the call. */
    while((len = usb_claim_interface(handle, usbInterface)) != 0 && retries-- > 0){
#ifdef LIBUSB_HAS_DETACH_KERNEL_DRIVER_NP
        if(usb_detach_kernel_driver_np(handle, 0) < 0 && showWarnings){
            fprintf(stderr, "PLUGIN: USB_PTT ERROR ( 0x%02X ): Warning: could not detach kernel driver: %s\n",VUSB_ERROR, usb_strerror());
        }
#endif
    }
#endif

    //if(strcasecmp(argv[1], "status") == 0){
        cnt = usb_control_msg(handle, USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_OUT, LED, status, 0, buffer, 0, 5000);
    usb_close(handle);
	return cnt;
}

void TS3_PTT_TOGGLE(int PTT_STATUS)
{
	unsigned int error;
	uint64 serverConnectionHandlerID = 0;
	serverConnectionHandlerID = ts3Functions.getCurrentServerConnectionHandlerID();
	
	if((error = ts3Functions.setClientSelfVariableAsInt(serverConnectionHandlerID, CLIENT_INPUT_DEACTIVATED,
																PTT_STATUS))
																!= ERROR_ok) {
				char* errorMsg;
				if(ts3Functions.getErrorMessage(error, &errorMsg) != ERROR_ok) {
					printf("PLUGIN: USB_PTT ERROR ( 0x%02X ): Error toggling push-to-talk: %s\n",PTT_TOGGLE_ERROR, errorMsg);
					ts3Functions.freeMemory(errorMsg);
				}
								
			}
									
			if(ts3Functions.flushClientSelfUpdates(serverConnectionHandlerID, NULL) != ERROR_ok) {
				char* errorMsg;
				if(ts3Functions.getErrorMessage(error, &errorMsg) != ERROR_ok) {
					printf("PLUGIN: USB_PTT ERROR ( 0x%02X ): Error flushing after toggling push-to-talk: %s\n",PTT_TOGGLE_ERROR, errorMsg);
					ts3Functions.freeMemory(errorMsg);
				}
			}
}

void TS3_WHISPER_TOGGLE(int PTT_STATUS,int channel)
{
	UpdateWhisperLists();
	unsigned int error;
	int Client_Talking;
	uint64 serverConnectionHandlerID = 0;
	anyID clientID; 
	//anyID targetClientIDArray[100];

	serverConnectionHandlerID = ts3Functions.getCurrentServerConnectionHandlerID();

	ts3Functions.getClientSelfVariableAsInt(serverConnectionHandlerID,CLIENT_FLAG_TALKING,&Client_Talking);
	

	if((error = ts3Functions.getClientID(serverConnectionHandlerID, &clientID)) != ERROR_ok) {
									char* errorMsg;
									if(ts3Functions.getErrorMessage(error, &errorMsg) != ERROR_ok) {
										printf("PLUGIN: USB_PTT ERROR ( 0x%02X ): ERROR: Problem while getClientID (%s)\n",WHISPER_TOGGLE_ERROR, errorMsg);
										ts3Functions.freeMemory(errorMsg);
									}
								
								}
	//targetClientIDArray[0] = 2097;
	//targetClientIDArray[1] = 0;

	if (PTT_STATUS == INPUT_ACTIVE) {
	if((error = ts3Functions.requestClientSetWhisperList(serverConnectionHandlerID,0, NULL,targetClientIDArray[channel],NULL)) != ERROR_ok) {
									char* errorMsg;
									if(ts3Functions.getErrorMessage(error, &errorMsg) != ERROR_ok) {
										printf("PLUGIN: USB_PTT ERROR ( 0x%02X ): ERROR: Problem while requestClientSetWhisperList (%s)\n",WHISPER_TOGGLE_ERROR, errorMsg);
										ts3Functions.freeMemory(errorMsg);
									}
									

								
								}
	if (Client_Talking != STATUS_TALKING) {
		TS3_PTT_TOGGLE(PTT_STATUS);
	}
	} else if (PTT_STATUS == INPUT_DEACTIVATED) {
			if((error = ts3Functions.requestClientSetWhisperList(serverConnectionHandlerID,0, NULL,NULL,NULL)) != ERROR_ok) {
									char* errorMsg;
									if(ts3Functions.getErrorMessage(error, &errorMsg) != ERROR_ok) {
										printf("PLUGIN: USB_PTT ERROR ( 0x%02X ): ERROR: Problem while requestClientSetWhisperList (%s)\n",WHISPER_TOGGLE_ERROR, errorMsg);
										ts3Functions.freeMemory(errorMsg);
									}
									

								
								}
	if (Client_Talking != STATUS_NOT_TALKING) {
		TS3_PTT_TOGGLE(PTT_STATUS);
	}
	}
}


void PTTUSB_POTI (int channel)
{
	usb_dev_handle      *handle = NULL;
const unsigned char rawVid[2] = {USB_CFG_VENDOR_ID}, rawPid[2] = {USB_CFG_DEVICE_ID};
char                vendor[] = {USB_CFG_VENDOR_NAME, 0}, product[] = {USB_CFG_DEVICE_NAME, 0};
char                buffer[10];
int                 cnt, vid, pid;

int Client_Talking;
uint64 serverConnectionHandlerID = 0;

	serverConnectionHandlerID = ts3Functions.getCurrentServerConnectionHandlerID();
	ts3Functions.getClientSelfVariableAsInt(serverConnectionHandlerID,CLIENT_FLAG_TALKING,&Client_Talking);

    usb_init();

    /* compute VID/PID from usbconfig.h so that there is a central source of information */
    vid = rawVid[1] * 256 + rawVid[0];
    pid = rawPid[1] * 256 + rawPid[0];
    /* The following function is in opendevice.c: */
    if(usbOpenDevice(&handle, vid, vendor, pid, product, NULL, NULL, NULL) != 0){
		if (USB_NOT_FOUND == false) {
			fprintf(stderr, "PLUGIN: USB_PTT ERROR ( 0x%02X ): Could not find USB device \"%s\" with vid=0x%x pid=0x%x\n", VUSB_ERROR, product, vid, pid);
			TS3_PTT_TOGGLE(INPUT_DEACTIVATED);
			USB_NOT_FOUND = true;
			TALK_PEDAL_TOGGLE = false;
			WHISPER_PEDAL_TOGGLE[channel] = false;
		}
		return;
    } else {
		USB_NOT_FOUND = false;
	}

	        cnt = usb_control_msg(handle, USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_IN, RQ_GET_POTI_CHANNEL, channel, 0, buffer, sizeof(buffer), 5000);
        if(cnt < 1){
            if(cnt < 0){
                fprintf(stderr, "PLUGIN: USB_PTT ERROR ( 0x%02X ): USB error: %s\n",VUSB_ERROR, usb_strerror());
					TS3_PTT_TOGGLE(INPUT_DEACTIVATED);
            }else{
                fprintf(stderr, "PLUGIN: USB_PTT ERROR ( 0x%02X ): only %d bytes received.\n",VUSB_ERROR, cnt);
					TS3_WHISPER_TOGGLE(INPUT_DEACTIVATED,channel);
            }
        }else{
            int test = atoi(buffer);
			if (test > 0) {
				//cnt = PTTUSB_LED(RQ_SET_PTT_LED,1);
			    if(PTTUSB_LED(RQ_SET_PTT_LED,1)<0) {
		            fprintf(stderr, "PLUGIN: USB_PTT ERROR ( 0x%02X ): USB error: %s\n",VUSB_ERROR, usb_strerror());
					TS3_PTT_TOGGLE(INPUT_DEACTIVATED);
				}
				//cnt = usb_control_msg(handle, USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_OUT, RQ_SET_PTT_LED, 1, 0, buffer, 0, 5000);
					//printf("Recv: %d\n",test);
					if (channel == 3) {
						if (Client_Talking != STATUS_TALKING && TALK_PEDAL_TOGGLE != TRUE) {
							TS3_WHISPER_TOGGLE(INPUT_DEACTIVATED,channel); // Erst Whisper Deaktivieren, vor dem Normalem Reden...
							TS3_PTT_TOGGLE(INPUT_ACTIVE);
							printf("PLUGIN: USB_PTT INFO  ( 0x%02X ): TALK: Status Talking = %i, TALK_PEDAL_TOGGLE = %i\n",PTT_TOGGLE_INFO,STATUS_TALKING,TALK_PEDAL_TOGGLE);
							TALK_PEDAL_TOGGLE = true;
						}
					} else if (channel == 0) {
						if (Client_Talking != STATUS_TALKING && WHISPER_PEDAL_TOGGLE[channel] != TRUE) {
							TS3_WHISPER_TOGGLE(INPUT_ACTIVE,channel);
							printf("PLUGIN: USB_PTT INFO  ( 0x%02X ): WHISPER: Status Talking = %i, WHISPER_PEDAL_TOGGLE = %i\n",WHISPER_TOGGLE_INFO,STATUS_TALKING,WHISPER_PEDAL_TOGGLE);
							WHISPER_PEDAL_TOGGLE[channel] = true;
						}
					} else {
						if (Client_Talking != STATUS_TALKING && WHISPER_PEDAL_TOGGLE[channel] != TRUE) {
							TS3_WHISPER_TOGGLE(INPUT_ACTIVE,channel);
							printf("PLUGIN: USB_PTT INFO  ( 0x%02X ): WHISPER: Status Talking = %i, WHISPER_PEDAL_TOGGLE = %i\n",WHISPER_TOGGLE_INFO,STATUS_TALKING,WHISPER_PEDAL_TOGGLE);
							WHISPER_PEDAL_TOGGLE[channel] = true;
						}
					}
				
			} else { 
			    if(PTTUSB_LED(RQ_SET_PTT_LED,0)<0) {
		            fprintf(stderr, "PLUGIN: USB_PTT ERROR ( 0x%02X ): USB error: %s\n",VUSB_ERROR, usb_strerror());
					TS3_PTT_TOGGLE(INPUT_DEACTIVATED);
				}
				//cnt = usb_control_msg(handle, USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_OUT, RQ_SET_PTT_LED, 0, 0, buffer, 0, 5000);
					//printf("Recv: %d\n",test);
					if (channel == 3) {
						if (Client_Talking != STATUS_NOT_TALKING && TALK_PEDAL_TOGGLE != false) {
							TS3_PTT_TOGGLE(INPUT_DEACTIVATED);
							printf("PLUGIN: USB_PTT INFO  ( 0x%02X ): TALK: Status Talking = %i, TALK_PEDAL_TOGGLE = %i\n",PTT_TOGGLE_INFO,STATUS_TALKING,TALK_PEDAL_TOGGLE);
							TALK_PEDAL_TOGGLE = false;
						}
				} else if (channel == 0) {
						if (Client_Talking != STATUS_NOT_TALKING && WHISPER_PEDAL_TOGGLE[channel] != false) {
							TS3_WHISPER_TOGGLE(INPUT_DEACTIVATED,channel);
							printf("PLUGIN: USB_PTT INFO  ( 0x%02X ): WHISPER: Status Talking = %i, WHISPER_PEDAL_TOGGLE = %i\n",WHISPER_TOGGLE_INFO,STATUS_TALKING,WHISPER_PEDAL_TOGGLE);
							WHISPER_PEDAL_TOGGLE[channel] = false;
						}
				} else  {
					if (Client_Talking != STATUS_NOT_TALKING && WHISPER_PEDAL_TOGGLE[channel] != false) {
						TS3_WHISPER_TOGGLE(INPUT_DEACTIVATED,channel);
						printf("PLUGIN: USB_PTT INFO  ( 0x%02X ): WHISPER: Status Talking = %i, WHISPER_PEDAL_TOGGLE = %i\n",WHISPER_TOGGLE_INFO,STATUS_TALKING,WHISPER_PEDAL_TOGGLE);
						WHISPER_PEDAL_TOGGLE[channel] = false;
					}
				}
			}
		}
		usb_close(handle);
}



void READ_USB_Button (int channel)
{
	usb_dev_handle      *handle = NULL;
const unsigned char rawVid[2] = {USB_CFG_VENDOR_ID}, rawPid[2] = {USB_CFG_DEVICE_ID};
char                vendor[] = {USB_CFG_VENDOR_NAME, 0}, product[] = {USB_CFG_DEVICE_NAME, 0};
char                buffer[10];
int                 cnt, vid, pid;

int Client_Talking;
uint64 serverConnectionHandlerID = 0;

	serverConnectionHandlerID = ts3Functions.getCurrentServerConnectionHandlerID();
	ts3Functions.getClientSelfVariableAsInt(serverConnectionHandlerID,CLIENT_FLAG_TALKING,&Client_Talking);

    usb_init();

    /* compute VID/PID from usbconfig.h so that there is a central source of information */
    vid = rawVid[1] * 256 + rawVid[0];
    pid = rawPid[1] * 256 + rawPid[0];
    /* The following function is in opendevice.c: */
    if(usbOpenDevice(&handle, vid, vendor, pid, product, NULL, NULL, NULL) != 0){
		if (USB_NOT_FOUND == false) {
			fprintf(stderr, "PLUGIN: USB_PTT ERROR ( 0x%02X ): Could not find USB device \"%s\" with vid=0x%x pid=0x%x\n",VUSB_ERROR, product, vid, pid);
			TS3_PTT_TOGGLE(INPUT_DEACTIVATED);
			USB_NOT_FOUND = true;
			TALK_PEDAL_TOGGLE = false;
			WHISPER_PEDAL_TOGGLE[channel] = false;
		}
		return;
    } else {
		USB_NOT_FOUND = false;
	}

	        cnt = usb_control_msg(handle, USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_IN, RQ_GET_POTI_CHANNEL, channel, 0, buffer, sizeof(buffer), 5000);
        if(cnt < 1){
            if(cnt < 0){
                fprintf(stderr, "PLUGIN: USB_PTT ERROR ( 0x%02X ): USB error: %s\n",VUSB_ERROR, usb_strerror());
					TS3_PTT_TOGGLE(INPUT_DEACTIVATED);
            }else{
                fprintf(stderr, "PLUGIN: USB_PTT ERROR ( 0x%02X ): only %d bytes received.\n",VUSB_ERROR, cnt);
					TS3_WHISPER_TOGGLE(INPUT_DEACTIVATED,channel);
            }

		}
}


void timertest(void *dummy)
{
	while (1) {
		PTTUSB_POTI(0);
		PTTUSB_POTI(1);
		PTTUSB_POTI(2);
		PTTUSB_POTI(3);
		PTTUSB_POTI(4);
		PTTUSB_POTI(5);
		PTTUSB_POTI(6);
		PTTUSB_POTI(7);

		if(m_glbEndThreadNow == 1)
		{
			TS3_PTT_TOGGLE(INPUT_DEACTIVATED);
			printf("PLUGIN: USB_PTT INFO  ( 0x%02X ): Plugin Beendet\n",CLIENTINFO_INFO);
			_endthread();
			//exit(0);

		}
	}
}

char* GetClientUIDfromClientID(anyID serverConnectionHandlerID,UINT64 ClientID)
{
	char* uid = NULL;
	char* name = NULL;
	unsigned int error = NULL;
	anyID* clients = NULL;
				
	if(ts3Functions.getClientList(serverConnectionHandlerID, &clients) == ERROR_ok) {
		for(int i=0; clients[i] != NULL; i++) {
			if (clients[i] == ClientID) {
				ts3Functions.getClientVariableAsString(serverConnectionHandlerID,clients[i],CLIENT_UNIQUE_IDENTIFIER,&uid);
				ts3Functions.getClientVariableAsString(serverConnectionHandlerID,clients[i],CLIENT_NICKNAME,&name);
				printf("PLUGIN: USB_PTT       INFO  ( 0x%02X ): GetClientUIDfromClientID: UID for %s = %s\n",CLIENTINFO_INFO,name, uid);
				return (uid);
			}
		}
	}
	return(NULL);
}

anyID GetClientIDfromUID(anyID serverConnectionHandlerID,const char* Client_UID)
{

	char* uid = NULL;
	char* name = NULL;
	unsigned int error = NULL;
	anyID* clients = NULL;

	if(ts3Functions.getClientList(serverConnectionHandlerID, &clients) == ERROR_ok) {
		for(int i=0; clients[i] != NULL; i++) {
			ts3Functions.getClientVariableAsString(serverConnectionHandlerID,clients[i],CLIENT_UNIQUE_IDENTIFIER,&uid);
			if ( strcmp(uid, Client_UID) == 0) {
				ts3Functions.getClientVariableAsString(serverConnectionHandlerID,clients[i],CLIENT_NICKNAME,&name);
				printf("PLUGIN: USB_PTT       INFO  ( 0x%02X ): GetClientIDfromUID: ClientID for %s = %i\n",CLIENTINFO_INFO,name, clients[i]);
				return (clients[i]);
				
			}
		}
	}
	return(NULL);
}

static inline LPCSTR NextToken( LPCSTR pArg )
{
   // find next null with strchr and
   // point to the char beyond that.
   return strchr( pArg, '\0' ) + 1;
}


void ClearWhisperLists()
{
		for (int x = 0;x <= MAX_WHISPER_LISTS-1;x++)
	{
		for (int y = 0; y <= MAX_WHISPER_LISTS_LENGTH-1;y++)
		{
				WhisperLists[x][y].UID = "";
				WhisperLists[x][y].ClientID = 0;
				WhisperLists[x][y].Nickname = "";
	
		}
	}
}


void ReadWhisperIni(char WhisperIniPath[PATH_BUFSIZE])
{
	ClearWhisperLists();
	printf("ReadWhisperIni Reagind %s\n",WhisperIniPath);
	char channel_buffer[5] = "0";

	char uid_buffer[5] = "0";
	for (int i = 0;i<=MAX_WHISPER_LISTS;i++)
	{
		/*
		for (int o = 0;o<=20;o++)
		{
			_itoa(i,channel_buffer,10);
			_itoa(o,uid_buffer,10);
			//GetPrivateProfileString(channel_buffer,uid_buffer,NULL, buffer,100,WhisperIniPath);
			GetPrivateProfileString(channel_buffer,uid_buffer,"0", buffer,100,WhisperIniPath);
			WhisperLists[i][o].UID = buffer;
			printf("%i:%i\r",i,o);
			if (strcmp(WhisperLists[i][o].UID,"0"))
			{
				printf("Whisper List %i Position %i = %s\n",i,o,WhisperLists[i][o].UID);
			}
		}
		*/
		//_itoa(i,channel_buffer,10);
		_itoa_s(i,channel_buffer,10);
			char * pBigString= new char[2048];

	GetPrivateProfileSection(
   channel_buffer,
   pBigString,
   2048,
   WhisperIniPath );
		int o = 0;
			for ( LPCSTR pToken = pBigString; pToken && *pToken; pToken = NextToken(pToken) )
		{
			std::string buffer1;
			std::string buffer2;
			buffer1 = pToken;
			buffer2 = pToken;
			int pos = buffer1.find("=");
			int error = 0;
			buffer1 = buffer1.substr(pos+1);
			buffer2 = buffer2.substr(0,pos);
			MAX_WHISPER_LISTS_LENGTH;
			if (( atoi(buffer2.c_str()) <= MAX_WHISPER_LISTS_LENGTH ) && (atoi(buffer2.c_str()) >= 0)) {
			WhisperLists[i][atoi(buffer2.c_str())].UID = buffer1.c_str();
			
			if (!WhisperLists[i][atoi(buffer2.c_str())].UID.empty()) //strcmp(WhisperLists[i][atoi(buffer2.c_str())].UID.c_str(),"0"))
			{
				//printf("%i = %s\n",atoi(buffer2.c_str()),buffer1.c_str());
				printf("PLUGIN: USB_PTT INFO  ( 0x%02X ): Whisper List %i Position %i = %s\n",WHISPER_INI_INFO,i,atoi(buffer2.c_str()),WhisperLists[i][atoi(buffer2.c_str())].UID.c_str());
				/*
				char returnCode[RETURNCODE_BUFSIZE];
				ts3Functions.createReturnCode(pluginID, returnCode, RETURNCODE_BUFSIZE);

				ts3Functions.requestClientIDs(1,WhisperLists[i][atoi(buffer2.c_str())].UID.c_str(),returnCode);
				printf("PLUGIN: requestClientIDs %s = %c\n",WhisperLists[i][atoi(buffer2.c_str())].UID.c_str(), returnCode);							
				*/
				}				
			} else { printf("PLUGIN: USB_PTT ERROR ( 0x%02X ): Whisper List(%i) Entry(%i) irgnored, because it is negative\n                                or greater than maximum Whisper List length of %i\n",WHISPER_INI_ERROR,i,atoi(buffer2.c_str()),MAX_WHISPER_LISTS_LENGTH);}
			
		   //cout << ; // Do something with string
		}
	} 
}

void UpdateWhisperLists() {
	for (int x = 0;x <= MAX_WHISPER_LISTS-1;x++)
	{
		int targetClientIDArray_POS = 0;
		printf("PLUGIN: WHISPER_LISTS INFO ( 0x%02X ): Reading Whisper List %i\n",WHISPER_LISTS_INFO,x);
		for (int y = 0; y <= MAX_WHISPER_LISTS_LENGTH-1;y++)
		{
			if (!WhisperLists[x][y].UID.empty()) {//strcmp(WhisperLists[x][y].UID.c_str(),"")) {
				printf("PLUGIN: WHISPER_LISTS INFO  ( 0x%02X ): Whisper List %i Position %2i UID = %s\n",WHISPER_LISTS_INFO,x,y,WhisperLists[x][y].UID.c_str());
				WhisperLists[x][y].ClientID = GetClientIDfromUID(1,WhisperLists[x][y].UID.c_str());
				if (WhisperLists[x][y].ClientID != 0) {
					targetClientIDArray[x][targetClientIDArray_POS] = WhisperLists[x][y].ClientID;
					targetClientIDArray[x][targetClientIDArray_POS+1] = 0;
					targetClientIDArray_POS++;
					printf("PLUGIN: WHISPER_LISTS INFO  ( 0x%02X ):                            CID = %i\n",WHISPER_LISTS_INFO,WhisperLists[x][y].ClientID);
				}
				char* name = NULL;
				if(ts3Functions.getClientVariableAsString(1,WhisperLists[x][y].ClientID,CLIENT_NICKNAME,&name) == ERROR_ok) {
				WhisperLists[x][y].Nickname = name;
				printf("PLUGIN: WHISPER_LISTS INFO  ( 0x%02X ):                           NAME = %s\n",WHISPER_LISTS_INFO,WhisperLists[x][y].Nickname.c_str());
				}
			}
		}
	}


}