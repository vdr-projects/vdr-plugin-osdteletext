/***************************************************************************
 *       Copyright (c) 2003,2004 by Marcel Wiesweg                         *
 *       (autogenerated code (c) Klaus Schmidinger)
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <vdr/plugin.h>
#include <vdr/keys.h>
#include <vdr/config.h>

#include <getopt.h>

using namespace std;

#include "menu.h"
#include "txtrecv.h"
#include "setup.h"

#if defined(APIVERSNUM) && APIVERSNUM < 10600
#error "VDR-1.6.0 API version or greater is required!"
#endif

static const char *VERSION        = "0.6.0";
static const char *DESCRIPTION    = trNOOP("Displays teletext on the OSD");
static const char *MAINMENUENTRY  = trNOOP("Teletext (OSD)");

class cPluginTeletextosd : public cPlugin {
private:
  // Add any member variables or functions you may need here.
  cTxtStatus *txtStatus;
  ChannelStatus *channelStatus;
  bool startReceiver;
  void initTexts();
public:
  cPluginTeletextosd(void);
  virtual ~cPluginTeletextosd();
  virtual const char *Version(void) { return VERSION; }
  virtual const char *Description(void) { return tr(DESCRIPTION); }
  virtual const char *CommandLineHelp(void);
  virtual bool ProcessArgs(int argc, char *argv[]);
  virtual bool Start(void);
  virtual void Housekeeping(void);
  virtual const char *MainMenuEntry(void) { return tr(MAINMENUENTRY); }
  virtual cOsdObject *MainMenuAction(void);
  virtual cMenuSetupPage *SetupMenu(void);
  virtual bool SetupParse(const char *Name, const char *Value);
};

class cTeletextSetupPage;
class ActionEdit {
   public:
      void Init(cTeletextSetupPage*, int, cMenuEditIntItem  *, cMenuEditStraItem *);
      cMenuEditStraItem *action;
      cMenuEditIntItem  *number;
      bool visible;
   };
   
struct ActionKeyName {
   const char *internalName;
   const char *userName;
};

class cTeletextSetupPage : public cMenuSetupPage {
friend class ActionEdit;
private:
   TeletextSetup temp;
   int tempPageNumber[LastActionKey];
   int tempConfiguredClrBackground; //must be a signed int
protected:
   virtual void Store(void);
   ActionEdit ActionEdits[LastActionKey];
   virtual eOSState ProcessKey(eKeys Key);
public:
   cTeletextSetupPage(void);
   static const ActionKeyName *actionKeyNames;
   static const char **modes;
   //~cTeletextSetupPage(void);
   //void SetItemVisible(cOsdItem *Item, bool visible, bool callDisplay=false);
};

const ActionKeyName *cTeletextSetupPage::actionKeyNames = 0;
const char **cTeletextSetupPage::modes = 0;

/*class MenuEditActionItem : public cMenuEditStraItem {
public:
   MenuEditActionItem(cTeletextSetupPage *parentMenu, cMenuEditIntItem *pageNumberMenuItem,
                           const char *Name, int *Value, int NumStrings, const char * const *Strings);
protected:
   virtual eOSState ProcessKey(eKeys Key);
   cTeletextSetupPage *parent;
   cMenuEditIntItem *pageNumberItem;
};*/



cPluginTeletextosd::cPluginTeletextosd(void)
{
  // Initialize any member variables here.
  // DON'T DO ANYTHING ELSE THAT MAY HAVE SIDE EFFECTS, REQUIRE GLOBAL
  // VDR OBJECTS TO EXIST OR PRODUCE ANY OUTPUT!
  txtStatus=0;
  channelStatus=0;
  startReceiver=true;
}

cPluginTeletextosd::~cPluginTeletextosd()
{
   // Clean up after yourself!
   if (txtStatus)
      delete txtStatus;
   if (channelStatus)
      delete channelStatus;
   Storage::instance()->cleanUp();
}

const char *cPluginTeletextosd::CommandLineHelp(void)
{
  // Return a string that describes all known command line options.
  return "  -d        --directory=DIR    The directory where the temporary\n"
         "                               files will be stored.\n"
         "                               (default: /var/cache/vdr/vtx)\n"
         "                               Ensure that the directory exists and is writable.\n"
         "  -n        --max-cache=NUM    Maximum size in megabytes of cache used\n"
         "                               to store the pages on the harddisk.\n"
         "                               (default: a calculated value below 50 MB)\n"
         "  -s        --cache-system=SYS Set the cache system to be used.\n"
         "                               Choose \"legacy\" for the traditional\n"
         "                               one-file-per-page system.\n"
         "                               Default is \"packed\" for the \n"
         "                               one-file-for-a-few-pages system.\n"
         "  -R,       --no-receive       Do not receive and store teletext\n"
         "                               (deprecated - plugin will be useless).\n"
         "  -r,       --receive          (obsolete)\n";
}

bool cPluginTeletextosd::ProcessArgs(int argc, char *argv[])
{
  // Implement command line argument processing here if applicable.
   static struct option long_options[] = {
       { "directory",    required_argument,       NULL, 'd' },
       { "max-cache",    required_argument,       NULL, 'n' },
       { "cache-system",    required_argument,       NULL, 's' },
       { "no-receiver",  no_argument,       NULL, 'R' },
       { "receive",      no_argument,       NULL, 'r' },
       { NULL }
       };
     
   int c;
   int maxStorage=-1;
   while ((c = getopt_long(argc, argv, "s:d:n:Rr", long_options, NULL)) != -1) {
        switch (c) {
          case 's': 
                    if (!optarg)
                       break;
                    if (strcasecmp(optarg, "legacy")==0)
                       Storage::setSystem(Storage::StorageSystemLegacy);
                    else if (strcasecmp(optarg, "packed")==0)
                       Storage::setSystem(Storage::StorageSystemPacked);
                    break;
          case 'd': Storage::setRootDir(optarg);
                    break;
          case 'n': if (isnumber(optarg)) {
                       int n = atoi(optarg);
                       maxStorage=n;
                    }
                    break;
          case 'R': startReceiver=false;
                    break;
          case 'r': startReceiver=true;
                    break;
        }
   }
   //do this here because the option -s to change the storage system might be given
   // after -n, and then -s would have no effect
   if (maxStorage>=0)
      Storage::instance()->setMaxStorage(maxStorage);
   return true;
}

bool cPluginTeletextosd::Start(void)
{
  // Start any background activities the plugin shall perform.
   //Clean any files which might be remaining from the last session, 
   //perhaps due to a crash they have not been deleted.
   Storage::instance()->init();
   initTexts();
   if (startReceiver)
      txtStatus=new cTxtStatus();
   channelStatus=new ChannelStatus();
   if (ttSetup.OSDheight<=100)  ttSetup.OSDheight=Setup.OSDHeight;
   if (ttSetup.OSDwidth<=100)   ttSetup.OSDwidth=Setup.OSDWidth;
  
   return true;
}

void cPluginTeletextosd::initTexts() {
   if (cTeletextSetupPage::actionKeyNames)
      return;
   //TODO: rewrite this in the xy[0].cd="fg"; form
   static const ActionKeyName st_actionKeyNames[] =
   {
      { "Action_kRed",      trVDR("Key$Red") },
      { "Action_kGreen",    trVDR("Key$Green") },
      { "Action_kYellow",   trVDR("Key$Yellow") },
      { "Action_kBlue",     trVDR("Key$Blue") },
      { "Action_kPlay",     trVDR("Key$Play") },
      { "Action_kStop",     trVDR("Key$Stop") },
      { "Action_kFastFwd",  trVDR("Key$FastFwd") },
      { "Action_kFastRew",  trVDR("Key$FastRew") }
   };
   
   cTeletextSetupPage::actionKeyNames = st_actionKeyNames;
   
   static const char *st_modes[] =
   {
      tr("Zoom"),
      tr("Half page"),
      tr("Change channel"),
      tr("Switch background"),
      //tr("Suspend receiving"),
      tr("Jump to...")
   };
   
   cTeletextSetupPage::modes = st_modes;
}

void cPluginTeletextosd::Housekeeping(void)
{
  // Perform any cleanup or other regular tasks.
}

cOsdObject *cPluginTeletextosd::MainMenuAction(void)
{
   // Perform the action when selected from the main VDR menu.
   return new TeletextBrowser(txtStatus);
}

cMenuSetupPage *cPluginTeletextosd::SetupMenu(void)
{
  // Return a setup menu in case the plugin supports one.
  return new cTeletextSetupPage;
}


     
bool cPluginTeletextosd::SetupParse(const char *Name, const char *Value)
{    
  initTexts();
  // Parse your own setup parameters and store their values.
  //Stretch=true;
  if (!strcasecmp(Name, "configuredClrBackground")) ttSetup.configuredClrBackground=( ((unsigned int)atoi(Value)) << 24);
  else if (!strcasecmp(Name, "showClock")) ttSetup.showClock=atoi(Value);
     //currently not used
  else if (!strcasecmp(Name, "suspendReceiving")) ttSetup.suspendReceiving=atoi(Value);
  else if (!strcasecmp(Name, "autoUpdatePage")) ttSetup.autoUpdatePage=atoi(Value);
  else if (!strcasecmp(Name, "OSDheight")) ttSetup.OSDheight=atoi(Value);
  else if (!strcasecmp(Name, "OSDwidth")) ttSetup.OSDwidth=atoi(Value);
  else if (!strcasecmp(Name, "OSDHAlign")) ttSetup.OSDHAlign=atoi(Value);
  else if (!strcasecmp(Name, "OSDVAlign")) ttSetup.OSDVAlign=atoi(Value);
  else if (!strcasecmp(Name, "inactivityTimeout")) /*ttSetup.inactivityTimeout=atoi(Value)*/;
  else {
     for (int i=0;i<LastActionKey;i++) {
        if (!strcasecmp(Name, cTeletextSetupPage::actionKeyNames[i].internalName)) {
           ttSetup.mapKeyToAction[i]=(eTeletextAction)atoi(Value);
           
           //for migration to 0.4
           if (ttSetup.mapKeyToAction[i]<100 && ttSetup.mapKeyToAction[i]>=LastAction)
              ttSetup.mapKeyToAction[i]=LastAction-1;
              
           return true;
        }
     }
     
     //for migration to 0.4
     char act[7];
     strncpy(act, Name, 7);
     if (!strcasecmp(act, "Action_"))
        return true;
     
     return false;
  }
  return true;
}

void cTeletextSetupPage::Store(void) {
   //copy table
   for (int i=0;i<LastActionKey;i++) {
      if (temp.mapKeyToAction[i] >= LastAction) //jump to page selected
         ttSetup.mapKeyToAction[i]=(eTeletextAction)tempPageNumber[i];
      else //one of the other modes selected
         ttSetup.mapKeyToAction[i]=temp.mapKeyToAction[i];
   }
   ttSetup.configuredClrBackground=( ((unsigned int)tempConfiguredClrBackground) << 24);
   ttSetup.showClock=temp.showClock;
   ttSetup.suspendReceiving=temp.suspendReceiving;
   ttSetup.autoUpdatePage=temp.autoUpdatePage;
   ttSetup.OSDheight=temp.OSDheight;
   ttSetup.OSDwidth=temp.OSDwidth;
   ttSetup.OSDHAlign=temp.OSDHAlign;
   ttSetup.OSDVAlign=temp.OSDVAlign;
   //ttSetup.inactivityTimeout=temp.inactivityTimeout;
   
   for (int i=0;i<LastActionKey;i++) {
      SetupStore(actionKeyNames[i].internalName, ttSetup.mapKeyToAction[i]);
   }
   SetupStore("configuredClrBackground", (int)(ttSetup.configuredClrBackground >> 24));
   SetupStore("showClock", ttSetup.showClock);
      //currently not used
   //SetupStore("suspendReceiving", ttSetup.suspendReceiving);
   SetupStore("autoUpdatePage", ttSetup.autoUpdatePage);
   SetupStore("OSDheight", ttSetup.OSDheight);
   SetupStore("OSDwidth", ttSetup.OSDwidth);
   SetupStore("OSDHAlign", ttSetup.OSDHAlign);
   SetupStore("OSDVAlign", ttSetup.OSDVAlign);
   //SetupStore("inactivityTimeout", ttSetup.inactivityTimeout);
}


cTeletextSetupPage::cTeletextSetupPage(void) {
   cString buf;
   cOsdItem *item;

   //init tables
   for (int i=0;i<LastActionKey;i++) {
      if (ttSetup.mapKeyToAction[i] >= LastAction) {//jump to page selected  
         temp.mapKeyToAction[i]=LastAction; //to display the last string
         tempPageNumber[i]=ttSetup.mapKeyToAction[i];
      } else { //one of the other modes selected
         temp.mapKeyToAction[i]=ttSetup.mapKeyToAction[i];
         tempPageNumber[i]=100;
      }
   }
   tempConfiguredClrBackground=(ttSetup.configuredClrBackground >> 24);
   temp.showClock=ttSetup.showClock;
   temp.suspendReceiving=ttSetup.suspendReceiving;
   temp.autoUpdatePage=ttSetup.autoUpdatePage;
   temp.OSDheight=ttSetup.OSDheight;
   temp.OSDwidth=ttSetup.OSDwidth;
   temp.OSDHAlign=ttSetup.OSDHAlign;
   temp.OSDVAlign=ttSetup.OSDVAlign;
   //temp.inactivityTimeout=ttSetup.inactivityTimeout;
      
   Add(new cMenuEditIntItem(tr("Background transparency"), &tempConfiguredClrBackground, 0, 255)); 
   
   Add(new cMenuEditBoolItem(tr("Show clock"), &temp.showClock ));
   
   //Add(new cMenuEditBoolItem(tr("Setup$Suspend receiving"), &temp.suspendReceiving ));
   
   Add(new cMenuEditBoolItem(tr("Auto-update pages"), &temp.autoUpdatePage ));
   
   Add(new cMenuEditIntItem(tr("OSD height"), &temp.OSDheight, 250, MAXOSDHEIGHT));
   Add(new cMenuEditIntItem(tr("OSD width"), &temp.OSDwidth, 320, MAXOSDWIDTH));
   
   Add(new cMenuEditIntItem(tr("OSD horizontal align"), &temp.OSDHAlign, 0, 100));
   Add(new cMenuEditIntItem(tr("OSD vertical align"), &temp.OSDVAlign, 0, 100));
   
   //Using same string as VDR's setup menu
   //Add(new cMenuEditIntItem(tr("Setup.Miscellaneous$Min. user inactivity (min)"), &temp.inactivityTimeout));

   buf = cString::sprintf("%s:", tr("Key bindings"));
   item = new cOsdItem(*buf);
   item->SetSelectable(false);
   Add(item);

   for (int i=0;i<LastActionKey;i++) {
      ActionEdits[i].Init(this, i, new cMenuEditIntItem(tr("  Page number"), &tempPageNumber[i], 100, 899),
         new cMenuEditStraItem(actionKeyNames[i].userName, (int*)&temp.mapKeyToAction[i], LastAction+1, modes) );
   }
}

eOSState cTeletextSetupPage::ProcessKey(eKeys Key) {
   eOSState state = cMenuSetupPage::ProcessKey(Key);
   if (Key != kRight && Key!=kLeft) 
      return state;
   cOsdItem *item = Get(Current());
   for (int i=0;i<LastActionKey;i++) {
      if (ActionEdits[i].action==item) { //we have a key left/right and one of our items as current
         //eOSState state = item->ProcessKey(Key);
         //if (state != osUnknown) { //really should not return osUnknown I think
            if (temp.mapKeyToAction[i] == LastAction && !ActionEdits[i].visible) {
               //need to make it visible
               if (i+1<LastActionKey)
                  //does not work for i==LastAction-1
                  Ins( ActionEdits[i].number, false, ActionEdits[i+1].action);
               else
                  Add( ActionEdits[i].number, false );
                  
               ActionEdits[i].visible=true;
               Display();
            } else if (temp.mapKeyToAction[i] != LastAction && ActionEdits[i].visible) {
               //need to hide it
               cList<cOsdItem>::Del(ActionEdits[i].number, false);
               ActionEdits[i].visible=false;
               Display();
            }
            break;
            //return state;
         //}
     }
   }
      
   return state;   
   //return cMenuSetupPage::ProcessKey(Key);
}


void ActionEdit::Init(cTeletextSetupPage* s, int num, cMenuEditIntItem  *p, cMenuEditStraItem * a) {
   action=a;
   number=p;
   s->Add(action);
   if (s->temp.mapKeyToAction[num] == LastAction) {
      s->Add(number);
      visible=true;
   } else 
      visible=false;
}




VDRPLUGINCREATOR(cPluginTeletextosd); // Don't touch this!
