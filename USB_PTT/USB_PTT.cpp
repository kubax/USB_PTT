/*
 * TeamSpeak 3 demo plugin
 *
 * Copyright (c) 2008-2011 TeamSpeak Systems GmbH
 */

#ifdef _WIN32
#pragma warning (disable : 4100)  /* Disable Unreferenced parameter warning */
#include <Windows.h>
#endif

#include "Error_codes.h"
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <string.h>
#include <atlbase.h>
#include <string.h>
#include <windows.h>
#include <process.h>
#include <assert.h>
#include "public_errors.h"
#include "public_errors_rare.h"
#include "public_definitions.h"
#include "public_rare_definitions.h"
#include "ts3_functions.h"
#include "plugin_events.h"
#include <lusb0_usb.h>
#include "opendevice.h" /* common code moved to separate module */
#include "..\firmware\requests.h"   /* custom request numbers */
#include "..\firmware\usbconfig.h"  /* device's VID/PID and names */

#include "USB_PTT.h"
#include "USB_PTT_Functions.h"


#ifdef _WIN32
/* Helper function to convert wchar_T to Utf-8 encoded strings on Windows */
static int wcharToUtf8(const wchar_t* str, char** result) {
	int outlen = WideCharToMultiByte(CP_UTF8, 0, str, -1, 0, 0, 0, 0);
	*result = (char*)malloc(outlen);
	if(WideCharToMultiByte(CP_UTF8, 0, str, -1, *result, outlen, 0, 0) == 0) {
		*result = NULL;
		return -1;
	}
	return 0;
}
#endif


/*********************************** Required functions ************************************/
/*
 * If any of these required functions is not implemented, TS3 will refuse to load the plugin
 */

/* Unique name identifying this plugin */
const char* ts3plugin_name( ) {
#ifdef _WIN32
	/* TeamSpeak expects UTF-8 encoded characters. Following demonstrates a possibility how to convert UTF-16 wchar_t into UTF-8. */
	static char* result = NULL;  /* Static variable so it's allocated only once */
	if(!result) {
		const wchar_t* name = L"USB PTT";
		if(wcharToUtf8(name, &result) == -1) {  /* Convert name into UTF-8 encoded result */
			result = "USB PTT";  /* Conversion failed, fallback here */
		}
	}
	return result;
#else
	
	return "USB PTT";
#endif
}

/* Plugin version */
const char* ts3plugin_version() {
    return "0.2";
}

/* Plugin API version. Must be the same as the clients API major version, else the plugin fails to load. */
int ts3plugin_apiVersion() {
	return PLUGIN_API_VERSION;
}

/* Plugin author */
const char* ts3plugin_author() {
	/* If you want to use wchar_t, see ts3plugin_name() on how to use */
    return "Kubax - Arven en Aske - Kil'jaeden";
}

/* Plugin description */
const char* ts3plugin_description() {
	/* If you want to use wchar_t, see ts3plugin_name() on how to use */
    return "This plugin connects to the USB PTT Device and listen for Messages to activate Push-to-Talk or Whispers to Whisper lists.";
}

/* Set TeamSpeak 3 callback functions */
void ts3plugin_setFunctionPointers(const struct TS3Functions funcs) {
    ts3Functions = funcs;
}

/*
 * Custom code called right after loading the plugin. Returns 0 on success, 1 on failure.
 * If the function returns 1 on failure, the plugin will be unloaded again.
 */
int ts3plugin_init() {
    char appPath[PATH_BUFSIZE];
    char resourcesPath[PATH_BUFSIZE];
    char configPath[PATH_BUFSIZE];
	char pluginPath[PATH_BUFSIZE];

    /* Your plugin init code here */
    printf("PLUGIN: init\n");

	/* Show API versions */
	//printf("PLUGIN: Client API Version: %d, Plugin API Version: %d\n", ts3Functions.getAPIVersion(), ts3plugin_apiVersion());

    /* Example on how to query application, resources and configuration paths from client */
    /* Note: Console client returns empty string for app and resources path */
    ts3Functions.getAppPath(appPath, PATH_BUFSIZE);
    ts3Functions.getResourcesPath(resourcesPath, PATH_BUFSIZE);
    ts3Functions.getConfigPath(configPath, PATH_BUFSIZE);
	ts3Functions.getPluginPath(pluginPath, PATH_BUFSIZE);

	printf("PLUGIN: App path: %s\nResources path: %s\nConfig path: %s\nPlugin path: %s\n", appPath, resourcesPath, configPath, pluginPath);

	/* Generate ini Path */
	strcat_s(iniPath,pluginPath);
	strcat_s(iniPath,"USB_PTT/");
	CreateDirectory(iniPath,NULL);

	strcpy_s(WhisperiniPath,iniPath);
	strcat_s(WhisperiniPath,"whisper.ini");
	ReadWhisperIni(WhisperiniPath);


	/* Initialize return codes array for requestClientMove */
	memset(requestClientMoveReturnCodes, 0, REQUESTCLIENTMOVERETURNCODES_SLOTS * RETURNCODE_BUFSIZE);

	// Hier wird der Thread zum Lesen der Werte aus dem Seriellen Anschluss beim Initialisieren des Plugins gestartet
	//_beginthread( ReadSerial, 0, (void*)12 );
	m_glbEndThreadNow = 0;
	_beginthread( timertest, 0, NULL );

	// USB Generic HID Tests
	        if(PTTUSB_LED(RQ_SET_STATUS_LED,1)<0) {
	
            fprintf(stderr, "PLUGIN: USB_PTT ERROR ( 0x%02X ): USB error: %s\n",VUSB_ERROR, usb_strerror());
        }
    return 0;  /* 0 = success, 1 = failure */
}

/* Custom code called right before the plugin is unloaded */
void ts3plugin_shutdown() {
    /* Your plugin cleanup code here */
    printf("PLUGIN: shutdown\n");
	PTTUSB_LED(RQ_SET_STATUS_LED,0);
	m_glbEndThreadNow = 1;
	Sleep(10);
	
	/*
	 * Note:
	 * If your plugin implements a settings dialog, it must be closed and deleted here, else the
	 * TeamSpeak client will most likely crash (DLL removed but dialog from DLL code still open).
	 */

	/* Free pluginID if we registered it */
	if(pluginID) {
		free(pluginID);
		pluginID = NULL;
	}
}

/****************************** Optional functions ********************************/
/*
 * Following functions are optional, if not needed you don't need to implement them.
 */

/* Tell client if plugin offers a configuration window. If this function is not implemented, it's an assumed "does not offer" (PLUGIN_OFFERS_NO_CONFIGURE). */
int ts3plugin_offersConfigure() {
	printf("PLUGIN: offersConfigure\n");
	/*
	 * Return values:
	 * PLUGIN_OFFERS_NO_CONFIGURE         - Plugin does not implement ts3plugin_configure
	 * PLUGIN_OFFERS_CONFIGURE_NEW_THREAD - Plugin does implement ts3plugin_configure and requests to run this function in an own thread
	 * PLUGIN_OFFERS_CONFIGURE_QT_THREAD  - Plugin does implement ts3plugin_configure and requests to run this function in the Qt GUI thread
	 */
	//return PLUGIN_OFFERS_NO_CONFIGURE;  /* In this case ts3plugin_configure does not need to be implemented */
	return PLUGIN_OFFERS_CONFIGURE_NEW_THREAD;  /* In this case ts3plugin_configure does not need to be implemented */
}

/* Plugin might offer a configuration window. If ts3plugin_offersConfigure returns 0, this function does not need to be implemented. */
void ts3plugin_configure(void* handle, void* qParentWidget) {
    printf("PLUGIN: configure\n");
}

/*
 * If the plugin wants to use error return codes or plugin commands, it needs to register a command ID. This function will be automatically
 * called after the plugin was initialized. This function is optional. If you don't use error return codes or plugin commands, the function
 * can be omitted.
 * Note the passed pluginID parameter is no longer valid after calling this function, so you must copy it and store it in the plugin.
 */
void ts3plugin_registerPluginID(const char* id) {
	const size_t sz = strlen(id) + 1;
	pluginID = (char*)malloc(sz * sizeof(char));
	_strcpy(pluginID, sz, id);  /* The id buffer will invalidate after exiting this function */
	printf("PLUGIN: registerPluginID: %s\n", pluginID);
}

/* Plugin command keyword. Return NULL or "" if not used. */
const char* ts3plugin_commandKeyword() {
	return "ptt";
}

/* Plugin processes console command. Return 0 if plugin handled the command, 1 if not handled. */
int ts3plugin_processCommand(uint64 serverConnectionHandlerID, const char* command) {
	char buf[COMMAND_BUFSIZE];
	char *s, *param[100] = {0};
	int i = 0,channel = 0;
	enum { CMD_NONE = 0, CMD_JOIN, CMD_COMMAND, CMD_SERVERINFO, CMD_CHANNELINFO, CMD_AVATAR, CMD_WHISPER } cmd = CMD_NONE;
#ifdef _WIN32
	char* context = NULL;
#endif
	char WhisperiniPath[PATH_BUFSIZE] = {0};
	char* uid,channel_buffer[5],i_buffer[5];
	unsigned int error = NULL;
	anyID* clients = NULL;

	printf("PLUGIN: process command: '%s'\n", command);

	/* Test command: "join <channelID> [password]" */
	_strcpy(buf, COMMAND_BUFSIZE, command);
#ifdef _WIN32
	s = strtok_s(buf, " ", &context);
#else
	s = strtok(buf, " ");
#endif
	while(s != NULL) {
		if(i == 0) {
			if(!strcmp(s, "whisper")) {
				cmd = CMD_WHISPER;
			}
		} else if (i == 1) {
			channel = atoi(s);
		
		} else {
			param[i-2] = s;
			
		}

#ifdef _WIN32
		s = strtok_s(NULL, " ", &context);
#else
		s = strtok(NULL, " ");
#endif
		i++;
	}

	switch(cmd) {
		case CMD_NONE:
			return 1;  /* Command not handled by plugin */
		case CMD_WHISPER: {
//			unsigned int error;

			
			strcat_s(WhisperiniPath,iniPath);
			strcat_s(WhisperiniPath,"whisper.ini");
			printf("PLUGIN: USB_PTT INFO  ( 0x%02X ): Whisper.ini Path = %s",WHISPER_INI_INFO,WhisperiniPath);
			_itoa_s(channel,channel_buffer,10);
			printf("PLUGIN: USB_PTT INFO  ( 0x%02X ): channel_buffer: %s\n",WHISPER_INI_INFO, channel_buffer);
			WritePrivateProfileString(channel_buffer,NULL,NULL, WhisperiniPath);	
			for (int i=0;i<=100;i++) {
				if (param[i] == NULL)
				{
						
						targetClientIDArray[channel][i] = 0;
						break;
				}
				
				int buffer = atoi(param[i]);
				UINT64 test = atoi(param[i]);

				uid = GetClientUIDfromClientID(serverConnectionHandlerID,atoi(param[i]));

				_itoa_s(i,i_buffer,10);
				printf("PLUGIN: USB_PTT INFO  ( 0x%02X ): i_buffer: %s\n",WHISPER_INI_INFO, i_buffer);
				WritePrivateProfileString(channel_buffer,i_buffer,uid, WhisperiniPath);

				targetClientIDArray[channel][i] = buffer;
			}
				// Hier wird über den Befehl "/appt ptt an" der Thread Manuell gestartet.
				//_beginthread( ReadSerial, 0, (void*)12 );
			} 

		}
	
	return 0;  /* Plugin handled command */
}

/* Client changed current server connection handler */
void ts3plugin_currentServerConnectionChanged(uint64 serverConnectionHandlerID) {
    printf("PLUGIN: currentServerConnectionChanged %llu (%llu)\n", (long long unsigned int)serverConnectionHandlerID, (long long unsigned int)ts3Functions.getCurrentServerConnectionHandlerID());
}

void ts3plugin_pluginEvent(unsigned short data, const char* message) {
	char type, subtype;

	printf("PLUGIN: pluginEvent data = %u\n", data);
	if(message) {
		printf("Message: %s\n", message);
	}

	type = data >> 8;
	subtype = data & 0xFF;
	printf("Type = %d, subtype = %d\n", type, subtype);

	switch(type) {
		case PLUGIN_EVENT_TYPE_HOTKEY:
			switch(subtype) {
				case PLUGIN_EVENT_SUBTYPE_HOTKEY_TOGGLE_MICRO_ON:
					printf("Micro on\n");
					break;
				case PLUGIN_EVENT_SUBTYPE_HOTKEY_TOGGLE_MICRO_OFF:
					printf("Micro off\n");
					break;
				case PLUGIN_EVENT_SUBTYPE_HOTKEY_TOGGLE_SPEAKER_ON:
					printf("Speaker on\n");
					break;
				case PLUGIN_EVENT_SUBTYPE_HOTKEY_TOGGLE_SPEAKER_OFF:
					printf("Speaker off\n");
					break;
				case PLUGIN_EVENT_SUBTYPE_HOTKEY_TOGGLE_AWAY_ON:
					printf("Away on\n");
					break;
				case PLUGIN_EVENT_SUBTYPE_HOTKEY_TOGGLE_AWAY_OFF:
					printf("Away off\n");
					break;
				case PLUGIN_EVENT_SUBTYPE_HOTKEY_ACTIVATE_MICRO:
					printf("Activate micro\n");
					break;
				case PLUGIN_EVENT_SUBTYPE_HOTKEY_ACTIVATE_SPEAKER:
					printf("Activate speaker\n");
					break;
				case PLUGIN_EVENT_SUBTYPE_HOTKEY_ACTIVATE_AWAY:
					printf("Activate away\n");
					break;
				case PLUGIN_EVENT_SUBTYPE_HOTKEY_DEACTIVATE_MICRO:
					printf("Deactivate micro\n");
					break;
				case PLUGIN_EVENT_SUBTYPE_HOTKEY_DEACTIVATE_SPEAKER:
					printf("Deactivate speakers\n");
					break;
				case PLUGIN_EVENT_SUBTYPE_HOTKEY_DEACTIVATE_AWAY:
					printf("Deactivate away\n");
					break;
				default:
					break;
			}
			break;
		default:
			break;
	}
}

/*
 * Implement the following three functions when the plugin should display a line in the server/channel/client info.
 * If any of ts3plugin_infoTitle, ts3plugin_infoData or ts3plugin_freeMemory is missing, the info text will not be displayed.
 */

/* Static title shown in the left column in the info frame */
const char* ts3plugin_infoTitle() {
	return "Test plugin info";
}

/*
 * Dynamic content shown in the right column in the info frame. Memory for the data string needs to be allocated in this
 * function. The client will call ts3plugin_freeMemory once done with the string to release the allocated memory again.
 * Check the parameter "type" if you want to implement this feature only for specific item types. Set the parameter
 * "data" to NULL to have the client ignore the info data.
 */
void ts3plugin_infoData(uint64 serverConnectionHandlerID, uint64 id, enum PluginItemType type, char** data) {
	char* name;
	/* For demonstration purpose, display the name of the currently selected server, channel or client. */
	switch(type) {
		case PLUGIN_SERVER:
			if(ts3Functions.getServerVariableAsString(serverConnectionHandlerID, VIRTUALSERVER_NAME, &name) != ERROR_ok) {
				printf("Error getting virtual server name\n");
				return;
			}
			break;
		case PLUGIN_CHANNEL:
			if(ts3Functions.getChannelVariableAsString(serverConnectionHandlerID, id, CHANNEL_NAME, &name) != ERROR_ok) {
				printf("Error getting channel name\n");
				return;
			}
			break;
		case PLUGIN_CLIENT:
			if(ts3Functions.getClientVariableAsString(serverConnectionHandlerID, (anyID)id, CLIENT_NICKNAME, &name) != ERROR_ok) {
				printf("Error getting client nickname\n");
				return;
			}
			break;
		default:
			printf("Invalid item type: %d\n", type);
			data = NULL;  /* Ignore */
			return;
	}

	*data = (char*)malloc(INFODATA_BUFSIZE * sizeof(char));  /* Must be allocated in the plugin! */
	snprintf(*data, INFODATA_BUFSIZE, "The nickname is \"%s\"", name);
	ts3Functions.freeMemory(name);
}

/* Required to release the memory for parameter "data" allocated in ts3plugin_infoData */
void ts3plugin_freeMemory(void* data) {
	free(data);
}

/*
 * Plugin requests to be always automatically loaded by the TeamSpeak 3 client unless
 * the user manually disabled it in the plugin dialog.
 * This function is optional. If missing, no autoload is assumed.
 */
int ts3plugin_requestAutoload() {
	return 0;  /* 1 = request autoloaded, 0 = do not request autoload */
}

/* Helper function to create a menu item */
static struct PluginMenuItem* createMenuItem(enum PluginMenuType type, int id, const char* text, const char* icon) {
	struct PluginMenuItem* menuItem = (struct PluginMenuItem*)malloc(sizeof(struct PluginMenuItem));
	menuItem->type = type;
	menuItem->id = id;
	_strcpy(menuItem->text, PLUGIN_MENU_BUFSZ, text);
	_strcpy(menuItem->icon, PLUGIN_MENU_BUFSZ, icon);
	return menuItem;
}

/* Some makros to make the code to create menu items a bit more readable */
#define BEGIN_CREATE_MENUS(x) const size_t sz = x + 1; size_t n = 0; *menuItems = (struct PluginMenuItem**)malloc(sizeof(struct PluginMenuItem*) * sz);
#define CREATE_MENU_ITEM(a, b, c, d) (*menuItems)[n++] = createMenuItem(a, b, c, d);
#define END_CREATE_MENUS (*menuItems)[n++] = NULL; assert(n == sz);

/*
 * Menu IDs for this plugin. Pass these IDs when creating a menuitem to the TS3 client. When the menu item is triggered,
 * ts3plugin_onMenuItemEvent will be called passing the menu ID of the triggered menu item.
 * These IDs are freely choosable by the plugin author. It's not really needed to use an enum, it just looks prettier.
 */
enum {
	MENU_ID_CLIENT_0 = 0,
	MENU_ID_CLIENT_1 = 1,
	MENU_ID_CLIENT_2 = 2,
	MENU_ID_CLIENT_3 = 3,

};

/*
 * Initialize plugin menus.
 * This function is called after ts3plugin_init and ts3plugin_registerPluginID. A pluginID is required for plugin menus to work.
 * Both ts3plugin_registerPluginID and ts3plugin_freeMemory must be implemented to use menus.
 * If plugin menus are not used by a plugin, do not implement this function or return NULL.
 */
void ts3plugin_initMenus(struct PluginMenuItem*** menuItems, char** menuIcon) {
	/*
	 * Create the menus
	 * There are three types of menu items:
	 * - PLUGIN_MENU_TYPE_CLIENT:  Client context menu
	 * - PLUGIN_MENU_TYPE_CHANNEL: Channel context menu
	 * - PLUGIN_MENU_TYPE_GLOBAL:  "Plugins" menu in menu bar of main window
	 *
	 * Menu IDs are used to identify the menu item when ts3plugin_onMenuItemEvent is called
	 *
	 * The menu text is required, max length is 128 characters
	 *
	 * The icon is optional, max length is 128 characters. When not using icons, just pass an empty string.
	 * Icons are loaded from a subdirectory in the TeamSpeak client plugins folder. The subdirectory must be named like the
	 * plugin filename, without dll/so/dylib suffix
	 * e.g. for "test_plugin.dll", icon "1.png" is loaded from <TeamSpeak 3 Client install dir>\plugins\test_plugin\1.png
	 */

	BEGIN_CREATE_MENUS(4);  /* IMPORTANT: Number of menu items must be correct! */
	CREATE_MENU_ITEM(PLUGIN_MENU_TYPE_CLIENT,  MENU_ID_CLIENT_0,  "PTT Whisper To (Channel 0)",  "Whisper.png");
	CREATE_MENU_ITEM(PLUGIN_MENU_TYPE_CLIENT,  MENU_ID_CLIENT_1,  "PTT Whisper To (Channel 1)",  "Whisper.png");
	CREATE_MENU_ITEM(PLUGIN_MENU_TYPE_CLIENT,  MENU_ID_CLIENT_2,  "Read Whisper Ini-file",  "Whisper.png");
	CREATE_MENU_ITEM(PLUGIN_MENU_TYPE_CLIENT,  MENU_ID_CLIENT_3,  "Update Whisper List",  "Whisper.png");
	END_CREATE_MENUS;  /* Includes an assert checking if the number of menu items matched */

	/*
	 * Specify an optional icon for the plugin. This icon is used for the plugins submenu within context and main menus
	 * If unused, set menuIcon to NULL
	 */
	*menuIcon = (char*)malloc(PLUGIN_MENU_BUFSZ * sizeof(char));
	_strcpy(*menuIcon, PLUGIN_MENU_BUFSZ, "red_button.png");

	/* All memory allocated in this function will be automatically released by the TeamSpeak client later by calling ts3plugin_freeMemory */
}

/*
 * Called when a plugin menu item (see ts3plugin_initMenus) is triggered. Optional function, when not using plugin menus, do not implement this.
 * 
 * Parameters:
 * - serverConnectionHandlerID: ID of the current server tab
 * - type: Type of the menu (PLUGIN_MENU_TYPE_CHANNEL, PLUGIN_MENU_TYPE_CLIENT or PLUGIN_MENU_TYPE_GLOBAL)
 * - menuItemID: Id used when creating the menu item
 * - selectedItemID: Channel or Client ID in the case of PLUGIN_MENU_TYPE_CHANNEL and PLUGIN_MENU_TYPE_CLIENT. 0 for PLUGIN_MENU_TYPE_GLOBAL.
 */
void ts3plugin_onMenuItemEvent(uint64 serverConnectionHandlerID, enum PluginMenuType type, int menuItemID, uint64 selectedItemID) {
						anyID* clients;
					char* uid = NULL;
					char* dbid = NULL;
					char* name = NULL;
					
	printf("PLUGIN: onMenuItemEvent: serverConnectionHandlerID=%llu, type=%d, menuItemID=%d, selectedItemID=%llu\n", (long long unsigned int)serverConnectionHandlerID, type, menuItemID, (long long unsigned int)selectedItemID);
	switch(type) {
		case PLUGIN_MENU_TYPE_CLIENT:
			/* Client contextmenu item was triggered. selectedItemID is the clientID of the selected client */
			switch(menuItemID) {
				case MENU_ID_CLIENT_0:
				targetClientIDArray[0][0] = selectedItemID;
				targetClientIDArray[0][1] = 0;
					break;
				case MENU_ID_CLIENT_1:
/*
					char pluginPath[PATH_BUFSIZE];
					ts3Functions.getPluginPath(pluginPath, PATH_BUFSIZE);
					strcat_s(pluginPath,"USB_PTT/");
					CreateDirectory(pluginPath,NULL);
					strcat_s(pluginPath,"test.ini");
					WritePrivateProfileString("0",NULL,NULL, pluginPath);
					if(ts3Functions.getClientList(serverConnectionHandlerID, &clients) == ERROR_ok) {
						
						for(int i=0; clients[i] != NULL; i++) {
							char buffer[5] = "";

							ts3Functions.getClientVariableAsString(serverConnectionHandlerID,clients[i],CLIENT_UNIQUE_IDENTIFIER,&uid);
							ts3Functions.getClientVariableAsString(serverConnectionHandlerID,clients[i],CLIENT_NICKNAME,&name);
							printf("PLUGIN: USB_PTT INFO  ( 0x%02X ): Client ID:\t %u\n",CLIENTINFO_INFO, clients[i]);
							
							printf("PLUGIN: USB_PTT INFO  ( 0x%02X ): \t\t %s\n",CLIENTINFO_INFO,uid);
							printf("PLUGIN: USB_PTT INFO  ( 0x%02X ): \t\t %s\n",CLIENTINFO_INFO,name);
							//sprintf(hallo,"%i",i);
							_itoa_s(i,buffer,10);


							WritePrivateProfileString("0",buffer,uid, pluginPath);
						}
						
					}
					ts3Functions.freeMemory(clients);
					
				//targetClientIDArray[1][0] = selectedItemID;
				//targetClientIDArray[1][1] = 0;
				*/
					break;
				case MENU_ID_CLIENT_2:
					ReadWhisperIni(WhisperiniPath);
					break;
				case MENU_ID_CLIENT_3:
					UpdateWhisperLists();
					break;
				default:
					break;
			}
			break;
		default:
			break;
	}
}