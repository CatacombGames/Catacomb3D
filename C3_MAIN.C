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

// C3_MAIN.C

#include "C3_DEF.H"
#pragma hdrstop

/*
=============================================================================

						   CATACOMB 3-D

					  An Id Software production

						   by John Carmack

=============================================================================
*/

/*
=============================================================================

						 LOCAL CONSTANTS

=============================================================================
*/


/*
=============================================================================

						 GLOBAL VARIABLES

=============================================================================
*/

memptr		scalesegs[NUMPICS];
char		str[80],str2[20];
unsigned	tedlevelnum;
boolean		tedlevel;
gametype	gamestate;
exittype	playstate;

/*
=============================================================================

						 LOCAL VARIABLES

=============================================================================
*/



//===========================================================================

// JAB Hack begin
#define	MyInterrupt	0x60
void interrupt (*intaddr)();
void interrupt (*oldintaddr)();
	char	*JHParmStrings[] = {"no386",nil};

void
jabhack(void)
{
extern void far jabhack2(void);
extern int far	CheckIs386(void);

	int	i;

	oldintaddr = getvect(MyInterrupt);

	for (i = 1;i < _argc;i++)
		if (US_CheckParm(_argv[i],JHParmStrings) == 0)
			return;

	if (CheckIs386())
	{
		jabhack2();
		setvect(MyInterrupt,intaddr);
	}
}

void
jabunhack(void)
{
	setvect(MyInterrupt,oldintaddr);
}
//	JAB Hack end

//===========================================================================

/*
=====================
=
= NewGame
=
= Set up new game to start from the beginning
=
=====================
*/

void NewGame (void)
{
	memset (&gamestate,0,sizeof(gamestate));
	gamestate.mapon = 0;
	gamestate.body = MAXBODY;
}

//===========================================================================

#define RLETAG	0xABCD

/*
==================
=
= SaveTheGame
=
==================
*/

boolean	SaveTheGame(int file)
{
	word	i,compressed,expanded;
	objtype	*o;
	memptr	bigbuffer;

	if (!CA_FarWrite(file,(void far *)&gamestate,sizeof(gamestate)))
		return(false);

	expanded = mapwidth * mapheight * 2;
	MM_GetPtr (&bigbuffer,expanded);

	for (i = 0;i < 3;i+=2)	// Write planes 0 and 2
	{
//
// leave a word at start of compressed data for compressed length
//
		compressed = (unsigned)CA_RLEWCompress ((unsigned huge *)mapsegs[i]
			,expanded,((unsigned huge *)bigbuffer)+1,RLETAG);

		*(unsigned huge *)bigbuffer = compressed;

		if (!CA_FarWrite(file,(void far *)bigbuffer,compressed+2) )
		{
			MM_FreePtr (&bigbuffer);
			return(false);
		}
	}

	for (o = player;o;o = o->next)
		if (!CA_FarWrite(file,(void far *)o,sizeof(objtype)))
		{
			MM_FreePtr (&bigbuffer);
			return(false);
		}

	MM_FreePtr (&bigbuffer);

	return(true);
}

//===========================================================================

/*
==================
=
= LoadTheGame
=
==================
*/

boolean	LoadTheGame(int file)
{
	unsigned	i,x,y;
	objtype		*obj,*prev,*next,*followed;
	unsigned	compressed,expanded;
	unsigned	far *map,tile;
	memptr		bigbuffer;

	if (!CA_FarRead(file,(void far *)&gamestate,sizeof(gamestate)))
		return(false);

	SetupGameLevel ();		// load in and cache the base old level

	expanded = mapwidth * mapheight * 2;
	MM_GetPtr (&bigbuffer,expanded);

	for (i = 0;i < 3;i+=2)	// Read planes 0 and 2
	{
		if (!CA_FarRead(file,(void far *)&compressed,sizeof(compressed)) )
		{
			MM_FreePtr (&bigbuffer);
			return(false);
		}

		if (!CA_FarRead(file,(void far *)bigbuffer,compressed) )
		{
			MM_FreePtr (&bigbuffer);
			return(false);
		}

		CA_RLEWexpand ((unsigned huge *)bigbuffer,
			(unsigned huge *)mapsegs[i],expanded,RLETAG);
	}

	MM_FreePtr (&bigbuffer);
//
// copy the wall data to a data segment array again, to handle doors and
// bomb walls that are allready opened
//
	memset (tilemap,0,sizeof(tilemap));
	memset (actorat,0,sizeof(actorat));
	map = mapsegs[0];
	for (y=0;y<mapheight;y++)
		for (x=0;x<mapwidth;x++)
		{
			tile = *map++;
			if (tile<NUMFLOORS)
			{
				tilemap[x][y] = tile;
				if (tile>0)
					(unsigned)actorat[x][y] = tile;
			}
		}


	// Read the object list back in - assumes at least one object in list

	InitObjList ();
	new = player;
	while (true)
	{
		prev = new->prev;
		next = new->next;
		if (!CA_FarRead(file,(void far *)new,sizeof(objtype)))
			return(false);
		followed = new->next;
		new->prev = prev;
		new->next = next;
		actorat[new->tilex][new->tiley] = new;	// drop a new marker

		if (followed)
			GetNewObj (false);
		else
			break;
	}

	return(true);
}

//===========================================================================

/*
==================
=
= ResetGame
=
==================
*/

void ResetGame(void)
{
	NewGame ();

	ca_levelnum--;
	ca_levelbit>>=1;
	CA_ClearMarks();
	ca_levelbit<<=1;
	ca_levelnum++;
}

//===========================================================================


/*
==========================
=
= ShutdownId
=
= Shuts down all ID_?? managers
=
==========================
*/

void ShutdownId (void)
{
  US_Shutdown ();
#ifndef PROFILE
  SD_Shutdown ();
  IN_Shutdown ();
#endif
  VW_Shutdown ();
  CA_Shutdown ();
  MM_Shutdown ();
}


//===========================================================================

/*
==========================
=
= InitGame
=
= Load a few things right away
=
==========================
*/

void InitGame (void)
{
	unsigned	segstart,seglength;
	int			i,x,y;
	unsigned	*blockstart;

//	US_TextScreen();

	MM_Startup ();
	VW_Startup ();
#ifndef PROFILE
	IN_Startup ();
	SD_Startup ();
#endif
	US_Startup ();

//	US_UpdateTextScreen();

	CA_Startup ();
	US_Setup ();

	US_SetLoadSaveHooks(LoadTheGame,SaveTheGame,ResetGame);

//
// load in and lock down some basic chunks
//

	CA_ClearMarks ();

	CA_MarkGrChunk(STARTFONT);
	CA_MarkGrChunk(STARTTILE8);
	CA_MarkGrChunk(STARTTILE8M);
	CA_MarkGrChunk(HAND1PICM);
	CA_MarkGrChunk(HAND2PICM);
	CA_MarkGrChunk(ENTERPLAQUEPIC);

	CA_CacheMarks (NULL);

	MM_SetLock (&grsegs[STARTFONT],true);
	MM_SetLock (&grsegs[STARTTILE8],true);
	MM_SetLock (&grsegs[STARTTILE8M],true);
	MM_SetLock (&grsegs[HAND1PICM],true);
	MM_SetLock (&grsegs[HAND2PICM],true);
	MM_SetLock (&grsegs[ENTERPLAQUEPIC],true);

	fontcolor = WHITE;


//
// build some tables
//
	for (i=0;i<MAPSIZE;i++)
		nearmapylookup[i] = &tilemap[0][0]+MAPSIZE*i;

	for (i=0;i<PORTTILESHIGH;i++)
		uwidthtable[i] = UPDATEWIDE*i;

	blockstart = &blockstarts[0];
	for (y=0;y<UPDATEHIGH;y++)
		for (x=0;x<UPDATEWIDE;x++)
			*blockstart++ = SCREENWIDTH*16*y+x*TILEWIDTH;

	BuildTables ();			// 3-d tables

	SetupScaling ();

#ifndef PROFILE
//	US_FinishTextScreen();
#endif

//
// reclaim the memory from the linked in text screen
//
	segstart = FP_SEG(&introscn);
	seglength = 4000/16;
	if (FP_OFF(&introscn))
	{
		segstart++;
		seglength--;
	}

	MML_UseSpace (segstart,seglength);

	VW_SetScreenMode (GRMODE);
	VW_ColorBorder (3);
	VW_ClearVideo (BLACK);

//
// initialize variables
//
	updateptr = &update[0];
	*(unsigned *)(updateptr + UPDATEWIDE*PORTTILESHIGH) = UPDATETERMINATE;
	bufferofs = 0;
	displayofs = 0;
	VW_SetLineWidth(SCREENWIDTH);
}

//===========================================================================

void clrscr (void);		// can't include CONIO.H because of name conflicts...

/*
==========================
=
= Quit
=
==========================
*/

void Quit (char *error)
{
	unsigned	finscreen;

#if 0
	if (!error)
	{
		CA_SetAllPurge ();
		CA_CacheGrChunk (PIRACY);
		finscreen = (unsigned)grsegs[PIRACY];
	}
#endif

	ShutdownId ();
	if (error && *error)
	{
	  puts(error);
	  exit(1);
	}

#if 0
	if (!NoWait)
	{
		movedata (finscreen,0,0xb800,0,4000);
		bioskey (0);
		clrscr();
	}
#endif

	exit(0);
}

//===========================================================================

/*
==================
=
= TEDDeath
=
==================
*/

void	TEDDeath(void)
{
	ShutdownId();
	execlp("TED5.EXE","TED5.EXE","/LAUNCH",NULL);
}

//===========================================================================

/*
=====================
=
= DemoLoop
=
=====================
*/

static	char *ParmStrings[] = {"easy","normal","hard",""};

void	DemoLoop (void)
{
	int	i,level;

//
// check for launch from ted
//
	if (tedlevel)
	{
		NewGame();
		gamestate.mapon = tedlevelnum;
		restartgame = gd_Normal;
		for (i = 1;i < _argc;i++)
		{
			if ( (level = US_CheckParm(_argv[i],ParmStrings)) == -1)
				continue;

			restartgame = gd_Easy+level;
			break;
		}
		GameLoop();
		TEDDeath();
	}


//
// main game cycle
//
	displayofs = bufferofs = 0;
	VW_Bar (0,0,320,200,0);

	while (1)
	{
		CA_CacheGrChunk (TITLEPIC);
		bufferofs = SCREEN2START;
		displayofs = SCREEN1START;
		VWB_DrawPic (0,0,TITLEPIC);
		MM_SetPurge (&grsegs[TITLEPIC],3);
		UNMARKGRCHUNK(TITLEPIC);
		FizzleFade (bufferofs,displayofs,320,200,true);

		if (!IN_UserInput(TickBase*3,false))
		{
			CA_CacheGrChunk (CREDITSPIC);
			VWB_DrawPic (0,0,CREDITSPIC);
			MM_SetPurge (&grsegs[CREDITSPIC],3);
			UNMARKGRCHUNK(CREDITSPIC);
			FizzleFade (bufferofs,displayofs,320,200,true);

		}

		if (!IN_UserInput(TickBase*3,false))
		{
highscores:
			DrawHighScores ();
			FizzleFade (bufferofs,displayofs,320,200,true);
			IN_UserInput(TickBase*3,false);
		}

		if (IN_IsUserInput())
		{
			US_ControlPanel ();

			if (restartgame || loadedgame)
			{
				GameLoop ();
				goto highscores;
			}
		}

	}
}

//===========================================================================

/*
==========================
=
= SetupScalePic
=
==========================
*/

void SetupScalePic (unsigned picnum)
{
	unsigned	scnum;

	scnum = picnum-FIRSTSCALEPIC;

	if (shapedirectory[scnum])
	{
		MM_SetPurge (&(memptr)shapedirectory[scnum],0);
		return;					// allready in memory
	}

	CA_CacheGrChunk (picnum);
	DeplanePic (picnum);
	shapesize[scnum] = BuildCompShape (&shapedirectory[scnum]);
	grneeded[picnum]&= ~ca_levelbit;
	MM_FreePtr (&grsegs[picnum]);
}

//===========================================================================

/*
==========================
=
= SetupScaleWall
=
==========================
*/

void SetupScaleWall (unsigned picnum)
{
	int		x,y;
	unsigned	scnum;
	byte	far *dest;

	scnum = picnum-FIRSTWALLPIC;

	if (walldirectory[scnum])
	{
		MM_SetPurge (&walldirectory[scnum],0);
		return;					// allready in memory
	}

	CA_CacheGrChunk (picnum);
	DeplanePic (picnum);
	MM_GetPtr(&walldirectory[scnum],64*64);
	dest = (byte far *)walldirectory[scnum];
	for (x=0;x<64;x++)
		for (y=0;y<64;y++)
			*dest++ = spotvis[y][x];
	grneeded[picnum]&= ~ca_levelbit;
	MM_FreePtr (&grsegs[picnum]);
}

//===========================================================================

/*
==========================
=
= SetupScaling
=
==========================
*/

void SetupScaling (void)
{
	int		i,x,y;
	byte	far *dest;

//
// build the compiled scalers
//
	for (i=1;i<=VIEWWIDTH/2;i++)
		BuildCompScale (i*2,&scaledirectory[i]);
}

//===========================================================================

int	showscorebox;

void RF_FixOfs (void)
{
}

void HelpScreens (void)
{
}


/*
==================
=
= CheckMemory
=
==================
*/

#define MINMEMORY	335000l

void	CheckMemory(void)
{
	unsigned	finscreen;

	if (mminfo.nearheap+mminfo.farheap+mminfo.EMSmem+mminfo.XMSmem
		>= MINMEMORY)
		return;

	CA_CacheGrChunk (OUTOFMEM);
	finscreen = (unsigned)grsegs[OUTOFMEM];
	ShutdownId ();
	movedata (finscreen,7,0xb800,0,4000);
	gotoxy (1,24);
	exit(1);
}

//===========================================================================


/*
==========================
=
= main
=
==========================
*/

void main (void)
{
	short i;

	if (stricmp(_argv[1], "/VER") == 0)
	{
		printf("Catacomb 3-D version 1.22  (Rev 1)\n");
		printf("Copyright 1991-93 Softdisk Publishing\n");
		printf("Developed for use with 100%% IBM compatibles\n");
		printf("that have 640K memory and DOS version 3.3 or later\n");
		printf("and EGA graphics or better.\n");
		exit(0);
	}

	if (stricmp(_argv[1], "/?") == 0)
	{
		printf("Catacomb 3-D version 1.22\n");
		printf("Copyright 1991-93 Softdisk Publishing\n\n");
		printf("Syntax:\n");
		printf("CAT3D [/<switch>]\n\n");
		printf("Switch       What it does\n");
		printf("/?           This Information\n");
		printf("/VER         Display Program Version Information\n");
		printf("/COMP        Fix problems with SVGA screens\n");
		printf("/NOAL        No AdLib or SoundBlaster detection\n");
		printf("/NOJOYS      Tell program to ignore joystick\n");
		printf("/NOMOUSE     Tell program to ignore mouse\n");
		printf("/HIDDENCARD  Overrides video detection\n\n");
		printf("Each switch must include a '/' and multiple switches\n");
		printf("must be seperated by at least one space.\n\n");

		exit(0);
	}

	jabhack();

	InitGame ();

	CheckMemory ();

	LoadLatchMem ();

#ifdef PROFILE
	NewGame ();
	GameLoop ();
#endif

//NewGame ();
//GameLoop ();

	DemoLoop();
	Quit("Demo loop exited???");
}
