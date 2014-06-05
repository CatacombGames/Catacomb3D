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

#include "ID_HEADS.H"
#include <MATH.H>
#include <VALUES.H>

//#define PROFILE

/*
=============================================================================

						 GLOBAL CONSTANTS

=============================================================================
*/

#define NAMESTART	180


#define UNMARKGRCHUNK(chunk)	(grneeded[chunk]&=~ca_levelbit)

#define MOUSEINT	0x33

#define EXPWALLSTART	8
#define NUMEXPWALLS		7
#define WALLEXP			15
#define NUMFLOORS		36

#define NUMFLOORS	36

#define NUMLATCHPICS	100
#define NUMSCALEPICS	100
#define NUMSCALEWALLS	30


#define FLASHCOLOR	5
#define FLASHTICS	4


#define NUMLEVELS	20

#define VIEWX		0		// corner of view window
#define VIEWY		0
#define VIEWWIDTH	(33*8)		// size of view window
#define VIEWHEIGHT	(18*8)
#define VIEWXH		(VIEWX+VIEWWIDTH-1)
#define VIEWYH		(VIEWY+VIEWHEIGHT-1)

#define CENTERX		(VIEWX+VIEWWIDTH/2-1)	// middle of view window
#define CENTERY		(VIEWY+VIEWHEIGHT/2-1)

#define GLOBAL1		(1l<<16)
#define TILEGLOBAL  GLOBAL1
#define TILESHIFT	16l

#define MINDIST		(2*GLOBAL1/5)
#define FOCALLENGTH	(TILEGLOBAL)	// in global coordinates

#define ANGLES		360		// must be divisable by 4

#define MAPSIZE		64		// maps are 64*64 max
#define MAXACTORS	150		// max number of tanks, etc / map

#define NORTH	0
#define EAST	1
#define SOUTH	2
#define WEST	3

#define SIGN(x) ((x)>0?1:-1)
#define ABS(x) ((int)(x)>0?(x):-(x))
#define LABS(x) ((long)(x)>0?(x):-(x))

#define	MAXSCALE	(VIEWWIDTH/2)


#define MAXBODY			64
#define MAXSHOTPOWER	56

#define SCREEN1START	0
#define SCREEN2START	8320

#define PAGE1START		0x900
#define PAGE2START		0x2000
#define	PAGE3START		0x3700
#define	FREESTART		0x4e00

#define PIXRADIUS		512

#define STATUSLINES		(200-VIEWHEIGHT)

enum bonusnumbers {B_BOLT,B_NUKE,B_POTION,B_RKEY,B_YKEY,B_GKEY,B_BKEY,B_SCROLL1,
 B_SCROLL2,B_SCROLL3,B_SCROLL4,B_SCROLL5,B_SCROLL6,B_SCROLL7,B_SCROLL8,
 B_GOAL,B_CHEST};


/*
=============================================================================

						   GLOBAL TYPES

=============================================================================
*/

enum {BLANKCHAR=9,BOLTCHAR,NUKECHAR,POTIONCHAR,KEYCHARS,SCROLLCHARS=17,
	NUMBERCHARS=25};

typedef long fixed;

typedef struct {int x,y;} tilept;
typedef struct {fixed x,y;} globpt;

typedef struct
{
  int	x1,x2,leftclip,rightclip;// first pixel of wall (may not be visable)
  unsigned	height1,height2,color,walllength,side;
	long	planecoord;
} walltype;

typedef enum
  {nothing,playerobj,bonusobj,orcobj,batobj,skeletonobj,trollobj,demonobj,
  mageobj,pshotobj,bigpshotobj,mshotobj,inertobj,bounceobj,grelmobj
  ,gateobj} classtype;

typedef enum {north,east,south,west,northeast,southeast,southwest,
		  northwest,nodir} dirtype;		// a catacombs 2 carryover


typedef struct	statestruct
{
	int		shapenum;
	int		tictime;
	void	(*think) ();
	struct	statestruct	*next;
} statetype;


typedef struct objstruct
{
  enum {no,yes}	active;
  int		ticcount;
  classtype	obclass;
  statetype	*state;

  boolean	shootable;
  boolean	tileobject;		// true if entirely inside one tile

  long		distance;
  dirtype	dir;
  fixed 	x,y;
  unsigned	tilex,tiley;
  int	 	viewx;
  unsigned	viewheight;

  int 		angle;
  int		hitpoints;
  long		speed;

  unsigned	size;			// global radius for hit rect calculation
  fixed		xl,xh,yl,yh;	// hit rectangle

  int		temp1,temp2;
  struct	objstruct	*next,*prev;
} objtype;


typedef	struct
{
	int		difficulty;
	int		mapon;
	int		bolts,nukes,potions,keys[4],scrolls[8];
	long	score;
	int		body,shotpower;
} gametype;

typedef	enum	{ex_stillplaying,ex_died,ex_warped,ex_resetgame
	,ex_loadedgame,ex_victorious,ex_abort} exittype;


/*
=============================================================================

						 C3_MAIN DEFINITIONS

=============================================================================
*/

extern	char		str[80],str2[20];
extern	unsigned	tedlevelnum;
extern	boolean		tedlevel;
extern	gametype	gamestate;
extern	exittype	playstate;


void NewGame (void);
boolean	SaveTheGame(int file);
boolean	LoadTheGame(int file);
void ResetGame(void);
void ShutdownId (void);
void InitGame (void);
void Quit (char *error);
void TEDDeath(void);
void DemoLoop (void);
void SetupScalePic (unsigned picnum);
void SetupScaleWall (unsigned picnum);
void SetupScaling (void);
void main (void);

/*
=============================================================================

						 C3_GAME DEFINITIONS

=============================================================================
*/

extern	unsigned	latchpics[NUMLATCHPICS];
extern	unsigned	tileoffsets[NUMTILE16];
extern	unsigned	textstarts[27];


#define	L_CHARS		0
#define L_NOSHOT	1
#define L_SHOTBAR	2
#define L_NOBODY	3
#define L_BODYBAR	4


void ScanInfoPlane (void);
void ScanText (void);
void SetupGameLevel (void);
void Victory (void);
void Died (void);
void NormalScreen (void);
void DrawPlayScreen (void);
void LoadLatchMem (void);
void FizzleFade (unsigned source, unsigned dest,
	unsigned width,unsigned height, boolean abortable);
void FizzleOut (int showlevel);
void FreeUpMemory (void);
void GameLoop (void);


/*
=============================================================================

						 C3_PLAY DEFINITIONS

=============================================================================
*/

extern	ControlInfo	c;
extern	boolean		running,slowturn;

extern	int			bordertime;

extern	byte		tilemap[MAPSIZE][MAPSIZE];
extern	objtype		*actorat[MAPSIZE][MAPSIZE];
extern	byte		spotvis[MAPSIZE][MAPSIZE];

extern	objtype 	objlist[MAXACTORS],*new,*obj,*player;

extern	unsigned	farmapylookup[MAPSIZE];
extern	byte		*nearmapylookup[MAPSIZE];
extern	byte		update[];

extern	boolean		godmode,singlestep;
extern	int			extravbls;

extern	int			mousexmove,mouseymove;
extern	int			pointcount,pointsleft;


void CenterWindow(word w,word h);
void DebugMemory (void);
void PicturePause (void);
int  DebugKeys (void);
void CheckKeys (void);
void InitObjList (void);
void GetNewObj (boolean usedummy);
void RemoveObj (objtype *gone);
void PollControlls (void);
void PlayLoop (void);


/*
=============================================================================

						 C3_STATE DEFINITIONS

=============================================================================
*/

void SpawnNewObj (unsigned x, unsigned y, statetype *state, unsigned size);
void SpawnNewObjFrac (long x, long y, statetype *state, unsigned size);
boolean CheckHandAttack (objtype *ob);
void T_DoDamage (objtype *ob);
boolean Walk (objtype *ob);
void ChaseThink (objtype *obj, boolean diagonal);
void MoveObj (objtype *ob, long move);
boolean Chase (objtype *ob, boolean diagonal);

extern	dirtype opposite[9];

/*
=============================================================================

						 C3_TRACE DEFINITIONS

=============================================================================
*/

int FollowTrace (fixed tracex, fixed tracey, long deltax, long deltay, int max);
int BackTrace (int finish);
void ForwardTrace (void);
int FinishWall (void);
void InsideCorner (void);
void OutsideCorner (void);
void FollowWalls (void);

extern	boolean	aborttrace;

/*
=============================================================================

						 C3_DRAW DEFINITIONS

=============================================================================
*/

#define MAXWALLS	50
#define DANGERHIGH	45

#define	MIDWALL		(MAXWALLS/2)

//==========================================================================

extern	tilept	tile,lasttile,focal,left,mid,right;

extern	globpt	edge,view;

extern	unsigned screenloc[3];
extern	unsigned freelatch;

extern	int screenpage;

extern	boolean		fizzlein;

extern	long lasttimecount;

extern	int firstangle,lastangle;

extern	fixed prestep;

extern	int traceclip,tracetop;

extern	fixed sintable[ANGLES+ANGLES/4],*costable;

extern	fixed	viewx,viewy,viewsin,viewcos;			// the focal point
extern	int	viewangle;

extern	fixed scale,scaleglobal;
extern	unsigned slideofs;

extern	int zbuffer[VIEWXH+1];

extern	walltype	walls[MAXWALLS],*leftwall,*rightwall;


extern	fixed	tileglobal;
extern	fixed	focallength;
extern	fixed	mindist;
extern	int		viewheight;
extern	fixed scale;

extern	int	walllight1[NUMFLOORS];
extern	int	walldark1[NUMFLOORS];
extern	int	walllight2[NUMFLOORS];
extern	int	walldark2[NUMFLOORS];

//==========================================================================

void	DrawLine (int xl, int xh, int y,int color);
void	DrawWall (walltype *wallptr);
void	TraceRay (unsigned angle);
fixed	FixedByFrac (fixed a, fixed b);
void	TransformPoint (fixed gx, fixed gy, int *screenx, unsigned *screenheight);
fixed	TransformX (fixed gx, fixed gy);
int	FollowTrace (fixed tracex, fixed tracey, long deltax, long deltay, int max);
void	ForwardTrace (void);
int	FinishWall (void);
int	TurnClockwise (void);
int	TurnCounterClockwise (void);
void	FollowWall (void);

void	NewScene (void);
void	BuildTables (void);


/*
=============================================================================

						 C3_SCALE DEFINITIONS

=============================================================================
*/


#define COMPSCALECODESTART	(65*6)		// offset to start of code in comp scaler

typedef struct
{
	unsigned	codeofs[65];
	unsigned	start[65];
	unsigned	width[65];
	byte		code[];
}	t_compscale;

typedef struct
{
	unsigned	width;
	unsigned	codeofs[64];
}	t_compshape;


extern unsigned	scaleblockwidth,
		scaleblockheight,
		scaleblockdest;

extern	byte	plotpix[8];
extern	byte	bitmasks1[8][8];
extern	byte	bitmasks2[8][8];


extern	t_compscale _seg *scaledirectory[MAXSCALE+1];
extern	t_compshape _seg *shapedirectory[NUMSCALEPICS];
extern	memptr			walldirectory[NUMSCALEWALLS];
extern	unsigned	shapesize[MAXSCALE+1];

void 		DeplanePic (int picnum);
void ScaleShape (int xcenter, t_compshape _seg *compshape, unsigned scale);
unsigned	BuildCompShape (t_compshape _seg **finalspot);


/*
=============================================================================

						 C3_ASM DEFINITIONS

=============================================================================
*/

extern	unsigned	wallheight	[VIEWWIDTH];
extern	unsigned	wallwidth	[VIEWWIDTH];
extern	unsigned	wallseg		[VIEWWIDTH];
extern	unsigned	wallofs		[VIEWWIDTH];
extern	unsigned	screenbyte	[VIEWWIDTH];
extern	unsigned	screenbit	[VIEWWIDTH];
extern	unsigned	bitmasks	[64];

extern	long		wallscalecall;

void	ScaleWalls (void);

/*
=============================================================================

						 C3_WIZ DEFINITIONS

=============================================================================
*/

#define MAXHANDHEIGHT	72

extern	long	lastnuke;
extern	int		handheight;
extern	int		boltsleft;

/*
=============================================================================

						 C3_ACT1 DEFINITIONS

=============================================================================
*/

extern	statetype s_trollouch;
extern	statetype s_trolldie1;


extern	statetype s_orcpause;

extern	statetype s_orc1;
extern	statetype s_orc2;
extern	statetype s_orc3;
extern	statetype s_orc4;

extern	statetype s_orcattack1;
extern	statetype s_orcattack2;
extern	statetype s_orcattack3;

extern	statetype s_orcouch;

extern	statetype s_orcdie1;
extern	statetype s_orcdie2;
extern	statetype s_orcdie3;


extern	statetype s_demonouch;
extern	statetype s_demondie1;

extern	statetype s_mageouch;
extern	statetype s_magedie1;

extern	statetype s_grelouch;
extern	statetype s_greldie1;

extern	statetype s_batdie1;
