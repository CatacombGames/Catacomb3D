/* Catacomb 3-D Source Code
 * Copyright (C) 1993-2014 Flat Rock Software
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

// C3_PLAY.C

#include "C3_DEF.H"
#pragma hdrstop

/*
=============================================================================

						 LOCAL CONSTANTS

=============================================================================
*/

#define POINTTICS	6


/*
=============================================================================

						 GLOBAL VARIABLES

=============================================================================
*/

ControlInfo	c;
boolean		running,slowturn;

int			bordertime;
objtype objlist[MAXACTORS],*new,*obj,*player,*lastobj,*objfreelist;

unsigned	farmapylookup[MAPSIZE];
byte		*nearmapylookup[MAPSIZE];

boolean		singlestep,godmode;
int			extravbls;

//
// replacing refresh manager
//
unsigned	mapwidth,mapheight,tics;
boolean		compatability;
byte		*updateptr;
unsigned	mapwidthtable[64];
unsigned	uwidthtable[UPDATEHIGH];
unsigned	blockstarts[UPDATEWIDE*UPDATEHIGH];
#define	UPDATESCREENSIZE	(UPDATEWIDE*PORTTILESHIGH+2)
#define	UPDATESPARESIZE		(UPDATEWIDE*2+4)
#define UPDATESIZE			(UPDATESCREENSIZE+2*UPDATESPARESIZE)
byte		update[UPDATESIZE];

int		mousexmove,mouseymove;
int		pointcount,pointsleft;

/*
=============================================================================

						 LOCAL VARIABLES

=============================================================================
*/

void CalcBounds (objtype *ob);
void DrawPlayScreen (void);


//
// near data map array (wall values only, get text number from far data)
//
byte		tilemap[MAPSIZE][MAPSIZE];
byte		spotvis[MAPSIZE][MAPSIZE];
objtype		*actorat[MAPSIZE][MAPSIZE];

objtype dummyobj;

int bordertime;
int	objectcount;

void StopMusic(void);
void StartMusic(void);


//==========================================================================

///////////////////////////////////////////////////////////////////////////
//
//	CenterWindow() - Generates a window of a given width & height in the
//		middle of the screen
//
///////////////////////////////////////////////////////////////////////////

#define MAXX	264
#define MAXY	146

void	CenterWindow(word w,word h)
{
	US_DrawWindow(((MAXX / 8) - w) / 2,((MAXY / 8) - h) / 2,w,h);
}

//===========================================================================


/*
=====================
=
= CheckKeys
=
=====================
*/

void CheckKeys (void)
{
	if (screenfaded)			// don't do anything with a faded screen
		return;

//
// pause key wierdness can't be checked as a scan code
//
	if (Paused)
	{
		CenterWindow (8,3);
		US_PrintCentered ("PAUSED");
		VW_UpdateScreen ();
		SD_MusicOff();
		IN_Ack();
		SD_MusicOn();
		Paused = false;
		if (MousePresent) Mouse(MDelta);	// Clear accumulated mouse movement
	}

//
// F1-F7/ESC to enter control panel
//
	if ( (LastScan >= sc_F1 && LastScan <= sc_F7) || LastScan == sc_Escape)
	{
		StopMusic ();
		NormalScreen ();
		FreeUpMemory ();
		US_CenterWindow (20,8);
		US_CPrint ("Loading");
		VW_UpdateScreen ();
		US_ControlPanel();
		if (abortgame)
		{
			playstate = ex_abort;
			return;
		}
		StartMusic ();
		IN_ClearKeysDown();
		if (restartgame)
			playstate = ex_resetgame;
		if (loadedgame)
			playstate = ex_loadedgame;
		DrawPlayScreen ();
		CacheScaleds ();
		lasttimecount = TimeCount;
		if (MousePresent) Mouse(MDelta);	// Clear accumulated mouse movement
	}

//
// F10-? debug keys
//
	if (Keyboard[sc_F10])
	{
		DebugKeys();
		if (MousePresent) Mouse(MDelta);	// Clear accumulated mouse movement
		lasttimecount = TimeCount;
	}

}


//===========================================================================

/*
#############################################################################

				  The objlist data structure

#############################################################################

objlist containt structures for every actor currently playing.  The structure
is accessed as a linked list starting at *player, ending when ob->next ==
NULL.  GetNewObj inserts a new object at the end of the list, meaning that
if an actor spawn another actor, the new one WILL get to think and react the
same frame.  RemoveObj unlinks the given object and returns it to the free
list, but does not damage the objects ->next pointer, so if the current object
removes itself, a linked list following loop can still safely get to the
next element.

<backwardly linked free list>

#############################################################################
*/


/*
=========================
=
= InitObjList
=
= Call to clear out the entire object list, returning them all to the free
= list.  Allocates a special spot for the player.
=
=========================
*/

void InitObjList (void)
{
	int	i;

	for (i=0;i<MAXACTORS;i++)
	{
		objlist[i].prev = &objlist[i+1];
		objlist[i].next = NULL;
	}

	objlist[MAXACTORS-1].prev = NULL;

	objfreelist = &objlist[0];
	lastobj = NULL;

	objectcount = 0;

//
// give the player and score the first free spots
//
	GetNewObj (false);
	player = new;
}

//===========================================================================

/*
=========================
=
= GetNewObj
=
= Sets the global variable new to point to a free spot in objlist.
= The free spot is inserted at the end of the liked list
=
= When the object list is full, the caller can either have it bomb out ot
= return a dummy object pointer that will never get used
=
=========================
*/

void GetNewObj (boolean usedummy)
{
	if (!objfreelist)
	{
		if (usedummy)
		{
			new = &dummyobj;
			return;
		}
		Quit ("GetNewObj: No free spots in objlist!");
	}

	new = objfreelist;
	objfreelist = new->prev;
	memset (new,0,sizeof(*new));

	if (lastobj)
		lastobj->next = new;
	new->prev = lastobj;	// new->next is allready NULL from memset

	new->active = false;
	lastobj = new;

	objectcount++;
}

//===========================================================================

/*
=========================
=
= RemoveObj
=
= Add the given object back into the free list, and unlink it from it's
= neighbors
=
=========================
*/

void RemoveObj (objtype *gone)
{
	objtype **spotat;

	if (gone == player)
		Quit ("RemoveObj: Tried to remove the player!");

//
// fix the next object's back link
//
	if (gone == lastobj)
		lastobj = (objtype *)gone->prev;
	else
		gone->next->prev = gone->prev;

//
// fix the previous object's forward link
//
	gone->prev->next = gone->next;

//
// add it back in to the free list
//
	gone->prev = objfreelist;
	objfreelist = gone;
}

//==========================================================================

/*
===================
=
= PollControls
=
===================
*/

void PollControls (void)
{
	unsigned buttons;

	IN_ReadControl(0,&c);

	if (MousePresent)
	{
		Mouse(MButtons);
		buttons = _BX;
		Mouse(MDelta);
		mousexmove = _CX;
		mouseymove = _DX;

		if (buttons&1)
			c.button0 = 1;
		if (buttons&2)
			c.button1 = 1;

	}

	if (Controls[0]==ctrl_Joystick)
	{
		if (c.x>120 || c.x <-120 || c.y>120 || c.y<-120)
			running = true;
		else
			running = false;
		if (c.x>-48 && c.x<48)
			slowturn = true;
		else
			slowturn = false;
	}
	else
	{
		if (Keyboard[sc_RShift])
			running = true;
		else
			running = false;
		if (c.button0)
			slowturn = true;
		else
			slowturn = false;
	}
}

//==========================================================================

/*
=================
=
= StopMusic
=
=================
*/

void StopMusic(void)
{
	int	i;

	SD_MusicOff();
	for (i = 0;i < LASTMUSIC;i++)
		if (audiosegs[STARTMUSIC + i])
		{
			MM_SetPurge(&((memptr)audiosegs[STARTMUSIC + i]),3);
			MM_SetLock(&((memptr)audiosegs[STARTMUSIC + i]),false);
		}
}

//==========================================================================


/*
=================
=
= StartMusic
=
=================
*/

// JAB - Cache & start the appropriate music for this level
void StartMusic(void)
{
	musicnames	chunk;

	SD_MusicOff();
	chunk =	TOOHOT_MUS;
//	if ((chunk == -1) || (MusicMode != smm_AdLib))
//DEBUG control panel		return;

	MM_BombOnError (false);
	CA_CacheAudioChunk(STARTMUSIC + chunk);
	MM_BombOnError (true);
	if (mmerror)
		mmerror = false;
	else
	{
		MM_SetLock(&((memptr)audiosegs[STARTMUSIC + chunk]),true);
		SD_StartMusic((MusicGroup far *)audiosegs[STARTMUSIC + chunk]);
	}
}

//==========================================================================


/*
===================
=
= PlayLoop
=
===================
*/

void PlayLoop (void)
{
	int		give;

	void (*think)();

	ingame = true;
	playstate = TimeCount = 0;
	gamestate.shotpower = handheight = 0;
	pointcount = pointsleft = 0;

	DrawLevelNumber (gamestate.mapon);
	DrawBars ();

#ifndef PROFILE
	fizzlein = true;				// fizzle fade in the first refresh
#endif
	TimeCount = lasttimecount = lastnuke = 0;

	PollControls ();				// center mouse
	StartMusic ();
	do
	{
#ifndef PROFILE
		PollControls();
#else
		c.xaxis = 1;
		if (++TimeCount == 300)
			return;
#endif

		for (obj = player;obj;obj = obj->next)
			if (obj->active)
			{
				if (obj->ticcount)
				{
					obj->ticcount-=tics;
					while ( obj->ticcount <= 0)
					{
						think = obj->state->think;
						if (think)
						{
							think (obj);
							if (!obj->state)
							{
								RemoveObj (obj);
								goto nextactor;
							}
						}

						obj->state = obj->state->next;
						if (!obj->state)
						{
							RemoveObj (obj);
							goto nextactor;
						}
						if (!obj->state->tictime)
						{
							obj->ticcount = 0;
							goto nextactor;
						}
						if (obj->state->tictime>0)
							obj->ticcount += obj->state->tictime;
					}
				}
				think =	obj->state->think;
				if (think)
				{
					think (obj);
					if (!obj->state)
						RemoveObj (obj);
				}
nextactor:;
			}


		if (bordertime)
		{
			bordertime -= tics;
			if (bordertime<=0)
			{
				bordertime = 0;
				VW_ColorBorder (3);
			}
		}

		if (pointcount)
		{
			pointcount -= tics;
			if (pointcount <= 0)
			{
				pointcount += POINTTICS;
				give = (pointsleft > 1000)? 1000 :
						(
							(pointsleft > 100)? 100 :
								((pointsleft < 20)? pointsleft : 20)
						);
				SD_PlaySound (GETPOINTSSND);
				AddPoints (give);
				pointsleft -= give;
				if (!pointsleft)
					pointcount = 0;
			}
		}

		ThreeDRefresh ();

		CheckKeys();
		if (singlestep)
		{
			VW_WaitVBL(14);
			lasttimecount = TimeCount;
		}
		if (extravbls)
			VW_WaitVBL(extravbls);

	}while (!playstate);
	StopMusic ();

	ingame = false;
	if (bordertime)
	{
		bordertime = 0;
		VW_ColorBorder (3);
	}

	if (!abortgame)
		AddPoints (pointsleft);
	else
		abortgame = false;
}

