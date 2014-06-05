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

// C3_DRAW.C

#include "C3_DEF.H"
#pragma hdrstop

//#define DRAWEACH				// draw walls one at a time for debugging

unsigned	highest;
unsigned	mostwalls,numwalls;

/*
=============================================================================

						 LOCAL CONSTANTS

=============================================================================
*/

#define PI	3.141592657
#define ANGLEQUAD	(ANGLES/4)

unsigned	oldend;

#define FINEANGLES	3600

#define MINRATIO	16


const	unsigned	MAXSCALEHEIGHT	= (VIEWWIDTH/2);
const	unsigned	MAXVISHEIGHT	= (VIEWHEIGHT/2);
const	unsigned	BASESCALE		= 32;

/*
=============================================================================

						 GLOBAL VARIABLES

=============================================================================
*/

//
// calculate location of screens in video memory so they have the
// maximum possible distance seperating them (for scaling overflow)
//

unsigned screenloc[3]= {0x900,0x2000,0x3700};
unsigned freelatch = 0x4e00;

boolean		fizzlein;

long	scaleshapecalll;
long	scaletablecall;

/*
=============================================================================

						 LOCAL VARIABLES

=============================================================================
*/

long 	bytecount,endcount;		// for profiling
int		animframe;
int		pixelangle[VIEWWIDTH];
int		far finetangent[FINEANGLES+1];
int		fineviewangle;
unsigned	viewxpix,viewypix;

/*
============================================================================

			   3 - D  DEFINITIONS

============================================================================
*/

fixed	tileglobal	= TILEGLOBAL;
fixed	focallength	= FOCALLENGTH;
fixed	mindist		= MINDIST;
int		viewheight	= VIEWHEIGHT;
fixed scale;


tilept	tile,lasttile,		// tile of wall being followed
	focal,			// focal point in tiles
	left,mid,right;		// rightmost tile in view

globpt edge,view;

int	segstart[VIEWHEIGHT],	// addline tracks line segment and draws
	segend[VIEWHEIGHT],
	segcolor[VIEWHEIGHT];	// only when the color changes


walltype	walls[MAXWALLS],*leftwall,*rightwall;


//==========================================================================

//
// refresh stuff
//

int screenpage;

long lasttimecount;

//
// rendering stuff
//

int firstangle,lastangle;

fixed prestep;

fixed sintable[ANGLES+ANGLES/4],*costable = sintable+(ANGLES/4);

fixed	viewx,viewy;			// the focal point
int	viewangle;
fixed	viewsin,viewcos;

int	zbuffer[VIEWXH+1];	// holds the height of the wall at that point

//==========================================================================

void	DrawLine (int xl, int xh, int y,int color);
void	DrawWall (walltype *wallptr);
void	TraceRay (unsigned angle);
fixed	FixedByFrac (fixed a, fixed b);
fixed	FixedAdd (void);
fixed	TransformX (fixed gx, fixed gy);
int		FollowTrace (fixed tracex, fixed tracey, long deltax, long deltay, int max);
int		BackTrace (int finish);
void	ForwardTrace (void);
int		TurnClockwise (void);
int		TurnCounterClockwise (void);
void	FollowWall (void);

void	NewScene (void);
void	BuildTables (void);

//==========================================================================


/*
==================
=
= DrawLine
=
= Must be in write mode 2 with all planes enabled
= The bit mask is left set to the end value, so clear it after all lines are
= drawn
=
= draws a black dot at the left edge of the line
=
==================
*/

unsigned static	char dotmask[8] = {0x80,0x40,0x20,0x10,8,4,2,1};
unsigned static	char leftmask[8] = {0xff,0x7f,0x3f,0x1f,0xf,7,3,1};
unsigned static	char rightmask[8] = {0x80,0xc0,0xe0,0xf0,0xf8,0xfc,0xfe,0xff};

void DrawLine (int xl, int xh, int y,int color)
{
  unsigned dest,xlp,xlb,xhb,maskleft,maskright,maskdot,mid;

  xlb=xl/8;
  xhb=xh/8;

  if (xh<xl)
	Quit("DrawLine: xh<xl");
  if (y<VIEWY)
	Quit("DrawLine: y<VIEWY");
  if (y>VIEWYH)
	Quit("DrawLine: y>VIEWYH");

	xlp = xl&7;
	maskleft = leftmask[xlp];
	maskright = rightmask[xh&7];

  mid = xhb-xlb-1;
  dest = bufferofs+ylookup[y]+xlb;

	//
	// set the GC index register to point to the bit mask register
	//
	asm	mov	al,GC_BITMASK
	asm	mov	dx,GC_INDEX
	asm	out	dx,al

  if (xlb==xhb)
  {
  //
  // entire line is in one byte
  //

	maskleft&=maskright;

	asm	mov	es,[screenseg]
	asm	mov	di,[dest]
	asm	mov	dx,GC_INDEX+1

	asm	mov	al,[BYTE PTR maskleft]
	asm	out	dx,al		// mask off pixels

	asm	mov	al,[BYTE PTR color]
	asm	xchg	al,[es:di]	// load latches and write pixels

	return;
  }

asm	mov	es,[screenseg]
asm	mov	di,[dest]
asm	mov	dx,GC_INDEX+1
asm	mov	bh,[BYTE PTR color]

//
// draw left side
//
asm	mov	al,[BYTE PTR maskleft]
asm	out	dx,al		// mask off pixels

asm	mov	al,bh
asm	xchg	al,[es:di]	// load latches and write pixels
asm	inc	di

//
// draw middle
//
asm	mov	al,255
asm	out	dx,al		// no masking

asm	mov	al,bh
asm	mov	cx,[mid]
asm	rep	stosb

//
// draw right side
//
asm	mov	al,[BYTE PTR maskright]
asm	out	dx,al		// mask off pixels
asm	xchg	bh,[es:di]	// load latches and write pixels

}

//==========================================================================

void DrawLineDot (int xl, int xh, int y,int color)
{
  unsigned dest,xlp,xlb,xhb,maskleft,maskright,maskdot,mid;

  xlb=xl/8;
  xhb=xh/8;

  if (xh<xl)
	Quit("DrawLine: xh<xl");
  if (y<VIEWY)
	Quit("DrawLine: y<VIEWY");
  if (y>VIEWYH)
	Quit("DrawLine: y>VIEWYH");

	xlp = xl&7;
	maskdot = dotmask[xlp];
	maskleft = leftmask[xlp];
	maskright = rightmask[xh&7];

  mid = xhb-xlb-1;
  dest = bufferofs+ylookup[y]+xlb;

	//
	// set the GC index register to point to the bit mask register
	//
	asm	mov	al,GC_BITMASK
	asm	mov	dx,GC_INDEX
	asm	out	dx,al

  if (xlb==xhb)
  {
  //
  // entire line is in one byte
  //

	maskleft&=maskright;

	asm	mov	es,[screenseg]
	asm	mov	di,[dest]
	asm	mov	dx,GC_INDEX+1

	asm	mov	al,[BYTE PTR maskleft]
	asm	out	dx,al		// mask off pixels

	asm	mov	al,[BYTE PTR color]
	asm	xchg	al,[es:di]	// load latches and write pixels


	//
	// write the black dot at the start
	//
	asm	mov	al,[BYTE PTR maskdot]
	asm	out	dx,al		// mask off pixels

	asm	xor	al,al
	asm	xchg	al,[es:di]	// load latches and write pixels


	return;
  }

asm	mov	es,[screenseg]
asm	mov	di,[dest]
asm	mov	dx,GC_INDEX+1
asm	mov	bh,[BYTE PTR color]

//
// draw left side
//
asm	mov	al,[BYTE PTR maskleft]
asm	out	dx,al		// mask off pixels

asm	mov	al,bh
asm	xchg	al,[es:di]	// load latches and write pixels

//
// write the black dot at the start
//
asm	mov	al,[BYTE PTR maskdot]
asm	out	dx,al		// mask off pixels
asm	xor	al,al
asm	xchg	al,[es:di]	// load latches and write pixels
asm	inc	di

//
// draw middle
//
asm	mov	al,255
asm	out	dx,al		// no masking

asm	mov	al,bh
asm	mov	cx,[mid]
asm	rep	stosb

//
// draw right side
//
asm	mov	al,[BYTE PTR maskright]
asm	out	dx,al		// mask off pixels
asm	xchg	bh,[es:di]	// load latches and write pixels

}

//==========================================================================


long		wallscalesource;

#ifdef DRAWEACH
/*
====================
=
= ScaleOneWall
=
====================
*/

void near ScaleOneWall (int xl, int xh)
{
	int	x,pixwidth,height;

	*(((unsigned *)&wallscalesource)+1) = wallseg[xl];

	for (x=xl;x<=xh;x+=pixwidth)
	{
		height = wallheight[x];
		pixwidth = wallwidth[x];
		(unsigned)wallscalesource = wallofs[x];

		*(((unsigned *)&scaletablecall)+1) = (unsigned)scaledirectory[height];
		(unsigned)scaletablecall = scaledirectory[height]->codeofs[0];

		//
		// scale a byte wide strip of wall
		//
		asm	mov	bx,[x]
		asm	mov	di,bx
		asm	shr	di,1
		asm	shr	di,1
		asm	shr	di,1						// X in bytes
		asm	add	di,[bufferofs]
		asm	and	bx,7
		asm	shl	bx,1
		asm	shl	bx,1
		asm	shl	bx,1
		asm	add	bx,[pixwidth]				// bx = pixel*8+pixwidth-1
		asm	dec	bx
		asm	mov	al,BYTE PTR [bitmasks1+bx]
		asm	mov	dx,GC_INDEX+1
		asm	out	dx,al						// set bit mask register
		asm	mov	es,[screenseg]
		asm	lds	si,[wallscalesource]
		asm	call [DWORD PTR ss:scaletablecall]		// scale the line of pixels

		asm	mov	al,BYTE PTR [ss:bitmasks2+bx]
		asm	or	al,al
		asm	jz	nosecond

		//
		// draw a second byte for vertical strips that cross two bytes
		//
		asm	inc	di
		asm	out	dx,al						// set bit mask register
		asm	call [DWORD PTR ss:scaletablecall]	// scale the line of pixels
	nosecond:
		asm	mov	ax,ss
		asm	mov	ds,ax
	}
}

#endif

int	walllight1[NUMFLOORS] = {0,
	WALL1LPIC,WALL2LPIC,WALL3LPIC,WALL4LPIC,WALL5LPIC,WALL6LPIC,WALL7LPIC,
	WALL1LPIC,WALL2LPIC,WALL3LPIC,WALL4LPIC,WALL5LPIC,WALL6LPIC,WALL7LPIC,
	EXPWALL1PIC,EXPWALL2PIC,EXPWALL3PIC,
	RDOOR1PIC,RDOOR2PIC,RDOOR1PIC,RDOOR2PIC,
	YDOOR1PIC,YDOOR2PIC,YDOOR1PIC,YDOOR2PIC,
	GDOOR1PIC,GDOOR2PIC,GDOOR1PIC,GDOOR2PIC,
	BDOOR1PIC,BDOOR2PIC,BDOOR1PIC,BDOOR2PIC};

int	walldark1[NUMFLOORS] = {0,
	WALL1DPIC,WALL2DPIC,WALL3DPIC,WALL4DPIC,WALL5DPIC,WALL6DPIC,WALL7DPIC,
	WALL1DPIC,WALL2DPIC,WALL3DPIC,WALL4DPIC,WALL5DPIC,WALL6DPIC,WALL7DPIC,
	EXPWALL1PIC,EXPWALL2PIC,EXPWALL3PIC,
	RDOOR1PIC,RDOOR2PIC,RDOOR1PIC,RDOOR2PIC,
	YDOOR1PIC,YDOOR2PIC,YDOOR1PIC,YDOOR2PIC,
	GDOOR1PIC,GDOOR2PIC,GDOOR1PIC,GDOOR2PIC,
	BDOOR1PIC,BDOOR2PIC,BDOOR1PIC,BDOOR2PIC};

int	walllight2[NUMFLOORS] = {0,
	WALL1LPIC,WALL2LPIC,WALL3LPIC,WALL4LPIC,WALL5LPIC,WALL6LPIC,WALL7LPIC,
	WALL1LPIC,WALL2LPIC,WALL3LPIC,WALL4LPIC,WALL5LPIC,WALL6LPIC,WALL7LPIC,
	EXPWALL1PIC,EXPWALL2PIC,EXPWALL3PIC,
	RDOOR2PIC,RDOOR1PIC,RDOOR2PIC,RDOOR1PIC,
	YDOOR2PIC,YDOOR1PIC,YDOOR2PIC,YDOOR1PIC,
	GDOOR2PIC,GDOOR1PIC,GDOOR2PIC,GDOOR1PIC,
	BDOOR2PIC,BDOOR1PIC,BDOOR2PIC,BDOOR1PIC};

int	walldark2[NUMFLOORS] = {0,
	WALL1DPIC,WALL2DPIC,WALL3DPIC,WALL4DPIC,WALL5DPIC,WALL6DPIC,WALL7DPIC,
	WALL1DPIC,WALL2DPIC,WALL3DPIC,WALL4DPIC,WALL5DPIC,WALL6DPIC,WALL7DPIC,
	EXPWALL1PIC,EXPWALL2PIC,EXPWALL3PIC,
	RDOOR2PIC,RDOOR1PIC,RDOOR2PIC,RDOOR1PIC,
	YDOOR2PIC,YDOOR1PIC,YDOOR2PIC,YDOOR1PIC,
	GDOOR2PIC,GDOOR1PIC,GDOOR2PIC,GDOOR1PIC,
	BDOOR2PIC,BDOOR1PIC,BDOOR2PIC,BDOOR1PIC};

/*
=====================
=
= DrawVWall
=
= Draws a wall by vertical segments, for texture mapping!
=
= wallptr->side is true for east/west walls (constant x)
=
= fracheight and fracstep are 16.16 bit fractions
=
=====================
*/

void DrawVWall (walltype *wallptr)
{
	int			x,i;
	unsigned	source;
	unsigned	width,sourceint;
	unsigned	wallpic,wallpicseg;
	unsigned	skip;
	long		fracheight,fracstep,longheightchange;
	unsigned	height;
	int			heightchange;
	unsigned	slope,distance;
	int			traceangle,angle;
	int			mapadd;
	unsigned	lastpix,lastsource,lastwidth;


	if (wallptr->rightclip < wallptr->leftclip)
		Quit ("DrawVWall: Right < Left");

//
// setup for height calculation
//
	wallptr->height1 >>= 1;
	wallptr->height2 >>= 1;
	wallptr->planecoord>>=10;			// remove non significant bits

	width = wallptr->x2 - wallptr->x1;
	if (width)
	{
		heightchange = wallptr->height2 - wallptr->height1;
		asm	mov	ax,[heightchange]
		asm	mov	WORD PTR [longheightchange+2],ax
		asm	mov	WORD PTR [longheightchange],0	// avoid long shift by 16
		fracstep = longheightchange/width;
	}

	fracheight = ((long)wallptr->height1<<16)+0x8000;
	skip = wallptr->leftclip - wallptr->x1;
	if (skip)
		fracheight += fracstep*skip;

//
// setup for texture mapping
//
// mapadd is 64*64 (to keep source positive) + the origin wall intercept
// distance has 6 unit bits, and 6 frac bits
// traceangle is the center view angle in FINEANGLES, moved to be in
// the +-90 degree range (to thew right of origin)
//
	traceangle = fineviewangle;
	//
	// find wall picture to map from
	//
	if (wallptr->side)
	{	// east or west wall
		if (animframe)
			wallpic = walllight2[wallptr->color];
		else
			wallpic = walllight1[wallptr->color];

		if (wallptr->planecoord < viewxpix)
		{
			distance = viewxpix-wallptr->planecoord;
			traceangle -= FINEANGLES/2;
			mapadd = (64-viewypix&63);		// the pixel spot of the origin
		}
		else
		{
			distance = wallptr->planecoord-viewxpix;
			// traceangle is correct
			mapadd = viewypix&63;		// the pixel spot of the origin
		}
	}
	else
	{	// north or south wall
		if (animframe)
			wallpic = walldark2[wallptr->color];
		else
			wallpic = walldark1[wallptr->color];

		if (wallptr->planecoord < viewypix)
		{
			distance = viewypix-wallptr->planecoord;
			traceangle -= FINEANGLES/4;
			mapadd = viewxpix&63;		// the pixel spot of the origin
		}
		else
		{
			distance = wallptr->planecoord-viewypix;
			traceangle -= FINEANGLES*3/4;
			mapadd = (64-viewxpix&63);		// the pixel spot of the origin
		}
	}

	mapadd = 64*64-mapadd;				// make sure it stays positive

	wallpicseg = (unsigned)walldirectory[wallpic-FIRSTWALLPIC];
	if (traceangle > FINEANGLES/2)
		traceangle -= FINEANGLES;

//
// calculate everything
//
// IMPORTANT!  This loop is executed around 5000 times / second!
//
	lastpix = lastsource = (unsigned)-1;

	for (x = wallptr->leftclip ; x <= wallptr->rightclip ; x++)
	{
		//
		// height
		//
		asm	mov	ax,WORD PTR [fracheight]
		asm	mov	dx,WORD PTR [fracheight+2]
		asm	mov	cx,dx
		asm	add	ax,WORD PTR [fracstep]
		asm	adc	dx,WORD PTR [fracstep+2]
		asm	mov	WORD PTR [fracheight],ax
		asm	mov	WORD PTR [fracheight+2],dx
		asm	mov	bx,[x]
		asm	shl	bx,1
		asm	cmp	cx,MAXSCALEHEIGHT
		asm	jbe	storeheight
		asm	mov	cx,MAXSCALEHEIGHT
storeheight:
		asm	mov WORD PTR [wallheight+bx],cx
		asm	mov WORD PTR [zbuffer+bx],cx

//		height = fracheight>>16;
//		fracheight += fracstep;
//		if (height > MAXSCALEHEIGHT)
//			height = MAXSCALEHEIGHT;
//		wallheight[x] = zbuffer[x] = height;

		//
		// texture map
		//
		angle = pixelangle[x]+traceangle;
		if (angle<0)
			angle+=FINEANGLES;

		slope = finetangent[angle];

//
// distance is an unsigned 6.6 bit number (12 pixel bits)
// slope is a signed 5.10 bit number
// result is a signed 11.16 bit number
//

#if 0
		source = distance*slope;
		source >>=20;

		source += mapadd;
		source &= 63;				// mask off the unused units
		source = 63-source;
		source <<= 6;				// multiply by 64 for offset into pic
#endif
		asm	mov	ax,[distance]
		asm	imul	[slope]			// ax is the source pixel
		asm	mov	al,ah
		asm	shr	al,1
		asm	shr	al,1				// low 6 bits is now pixel number
		asm	add	ax,[mapadd]
		asm	and ax,63
		asm	mov	dx,63
		asm	sub	dx,ax				// otherwise it is backwards
		asm	shl	dx,1
		asm	shl	dx,1
		asm	shl	dx,1
		asm	shl	dx,1
		asm	shl	dx,1
		asm	shl	dx,1				// *64 to index into shape
		asm	mov	[source],dx

		if (source != lastsource)
		{
			if (lastpix != (unsigned)-1)
			{
				wallofs[lastpix] = lastsource;
				wallseg[lastpix] = wallpicseg;
				wallwidth[lastpix] = lastwidth;
			}
			lastpix = x;
			lastsource = source;
			lastwidth = 1;
		}
		else
			lastwidth++;			// optimized draw, same map as last one
	}
	wallofs[lastpix] = lastsource;
	wallseg[lastpix] = wallpicseg;
	wallwidth[lastpix] = lastwidth;
}


//==========================================================================


/*
=================
=
= TraceRay
=
= Used to find the left and rightmost tile in the view area to be traced from
= Follows a ray of the given angle from viewx,viewy in the global map until
= it hits a solid tile
= sets:
=   tile.x,tile.y	: tile coordinates of contacted tile
=   tilecolor	: solid tile's color
=
==================
*/

int tilecolor;

void TraceRay (unsigned angle)
{
  long tracex,tracey,tracexstep,traceystep,searchx,searchy;
  fixed fixtemp;
  int otx,oty,searchsteps;

  tracexstep = costable[angle];
  traceystep = sintable[angle];

//
// advance point so it is even with the view plane before we start checking
//
  fixtemp = FixedByFrac(prestep,tracexstep);
  tracex = viewx+fixtemp;
  fixtemp = FixedByFrac(prestep,traceystep);
  tracey = viewy-fixtemp;

  tile.x = tracex>>TILESHIFT;	// starting point in tiles
  tile.y = tracey>>TILESHIFT;


  if (tracexstep<0)			// use 2's complement, not signed magnitude
	tracexstep = -(tracexstep&0x7fffffff);

  if (traceystep<0)			// use 2's complement, not signed magnitude
	traceystep = -(traceystep&0x7fffffff);

//
// we assume viewx,viewy is not inside a solid tile, so go ahead one step
//

  do	// until a solid tile is hit
  {
    otx = tile.x;
	oty = tile.y;
	spotvis[otx][oty] = true;
	tracex += tracexstep;
    tracey -= traceystep;
    tile.x = tracex>>TILESHIFT;
	tile.y = tracey>>TILESHIFT;

	if (tile.x!=otx && tile.y!=oty && (tilemap[otx][tile.y] || tilemap[tile.x][oty]) )
    {
      //
	  // trace crossed two solid tiles, so do a binary search along the line
	  // to find a spot where only one tile edge is crossed
      //
      searchsteps = 0;
      searchx = tracexstep;
      searchy = traceystep;
      do
      {
	searchx/=2;
	searchy/=2;
	if (tile.x!=otx && tile.y!=oty)
	{
	 // still too far
	  tracex -= searchx;
	  tracey += searchy;
	}
	else
	{
	 // not far enough, no tiles crossed
	  tracex += searchx;
	  tracey -= searchy;
	}

	//
	// if it is REAL close, go for the most clockwise intersection
	//
	if (++searchsteps == 16)
	{
	  tracex = (long)otx<<TILESHIFT;
	  tracey = (long)oty<<TILESHIFT;
	  if (tracexstep>0)
	  {
		if (traceystep<0)
		{
		  tracex += TILEGLOBAL-1;
		  tracey += TILEGLOBAL;
		}
		else
		{
		  tracex += TILEGLOBAL;
		}
	  }
	  else
	  {
		if (traceystep<0)
		{
		  tracex --;
		  tracey += TILEGLOBAL-1;
		}
		else
		{
		  tracey --;
		}
	  }
	}

	tile.x = tracex>>TILESHIFT;
	tile.y = tracey>>TILESHIFT;

	  } while (( tile.x!=otx && tile.y!=oty) || (tile.x==otx && tile.y==oty) );
	}
  } while (!(tilecolor = tilemap[tile.x][tile.y]) );

}

//==========================================================================


/*
========================
=
= FixedByFrac
=
= multiply a 16/16 bit, 2's complement fixed point number by a 16 bit
= fraction, passed as a signed magnitude 32 bit number
=
========================
*/

#pragma warn -rvl			// I stick the return value in with ASMs

fixed FixedByFrac (fixed a, fixed b)
{
  fixed value;

//
// setup
//
asm	mov	si,[WORD PTR b+2]	// sign of result = sign of fraction

asm	mov	ax,[WORD PTR a]
asm	mov	cx,[WORD PTR a+2]

asm	or	cx,cx
asm	jns	aok:				// negative?
asm	not	ax
asm	not	cx
asm	add	ax,1
asm	adc	cx,0
asm	xor	si,0x8000			// toggle sign of result
aok:

//
// multiply  cx:ax by bx
//
asm	mov	bx,[WORD PTR b]
asm	mul	bx					// fraction*fraction
asm	mov	di,dx				// di is low word of result
asm	mov	ax,cx				//
asm	mul	bx					// units*fraction
asm add	ax,di
asm	adc	dx,0

//
// put result dx:ax in 2's complement
//
asm	test	si,0x8000		// is the result negative?
asm	jz	ansok:
asm	not	ax
asm	not	dx
asm	add	ax,1
asm	adc	dx,0

ansok:;

}

#pragma warn +rvl

//==========================================================================


/*
========================
=
= TransformPoint
=
= Takes paramaters:
=   gx,gy		: globalx/globaly of point
=
= globals:
=   viewx,viewy		: point of view
=   viewcos,viewsin	: sin/cos of viewangle
=
=
= defines:
=   CENTERX		: pixel location of center of view window
=   TILEGLOBAL		: size of one
=   FOCALLENGTH		: distance behind viewx/y for center of projection
=   scale		: conversion from global value to screen value
=
= returns:
=   screenx,screenheight: projected edge location and size
=
========================
*/

void TransformPoint (fixed gx, fixed gy, int *screenx, unsigned *screenheight)
{
  int ratio;
  fixed gxt,gyt,nx,ny;

//
// translate point to view centered coordinates
//
  gx = gx-viewx;
  gy = gy-viewy;

//
// calculate newx
//
  gxt = FixedByFrac(gx,viewcos);
  gyt = FixedByFrac(gy,viewsin);
  nx = gxt-gyt;

//
// calculate newy
//
  gxt = FixedByFrac(gx,viewsin);
  gyt = FixedByFrac(gy,viewcos);
  ny = gyt+gxt;

//
// calculate perspective ratio
//
  if (nx<0)
	nx = 0;

  ratio = nx*scale/FOCALLENGTH;

  if (ratio<=MINRATIO)
	ratio = MINRATIO;

  *screenx = CENTERX + ny/ratio;

  *screenheight = TILEGLOBAL/ratio;

}


//
// transform actor
//
void TransformActor (objtype *ob)
{
  int ratio;
  fixed gx,gy,gxt,gyt,nx,ny;

//
// translate point to view centered coordinates
//
  gx = ob->x-viewx;
  gy = ob->y-viewy;

//
// calculate newx
//
  gxt = FixedByFrac(gx,viewcos);
  gyt = FixedByFrac(gy,viewsin);
  nx = gxt-gyt-ob->size;

//
// calculate newy
//
  gxt = FixedByFrac(gx,viewsin);
  gyt = FixedByFrac(gy,viewcos);
  ny = gyt+gxt;

//
// calculate perspective ratio
//
  if (nx<0)
	nx = 0;

  ratio = nx*scale/FOCALLENGTH;

  if (ratio<=MINRATIO)
	ratio = MINRATIO;

  ob->viewx = CENTERX + ny/ratio;

  ob->viewheight = TILEGLOBAL/ratio;
}

//==========================================================================

fixed TransformX (fixed gx, fixed gy)
{
  int ratio;
  fixed gxt,gyt,nx,ny;

//
// translate point to view centered coordinates
//
  gx = gx-viewx;
  gy = gy-viewy;

//
// calculate newx
//
  gxt = FixedByFrac(gx,viewcos);
  gyt = FixedByFrac(gy,viewsin);

  return gxt-gyt;
}

//==========================================================================

/*
==================
=
= BuildTables
=
= Calculates:
=
= scale			projection constant
= sintable/costable	overlapping fractional tables
= firstangle/lastangle	angles from focalpoint to left/right view edges
= prestep		distance from focal point before checking for tiles
=
==================
*/

void BuildTables (void)
{
  int 		i;
  long		intang;
  long		x;
  float 	angle,anglestep,radtoint;
  double	tang;
  fixed 	value;

//
// calculate the angle offset from view angle of each pixel's ray
//
	radtoint = (float)FINEANGLES/2/PI;
	for (i=0;i<VIEWWIDTH/2;i++)
	{
	// start 1/2 pixel over, so viewangle bisects two middle pixels
		x = (TILEGLOBAL*i+TILEGLOBAL/2)/VIEWWIDTH;
		tang = (float)x/(FOCALLENGTH+MINDIST);
		angle = atan(tang);
		intang = angle*radtoint;
		pixelangle[VIEWWIDTH/2-1-i] = intang;
		pixelangle[VIEWWIDTH/2+i] = -intang;
	}

//
// calculate fine tangents
// 1 sign bit, 5 units (clipped to), 10 fracs
//
#define MININT	(-MAXINT)

	for (i=0;i<FINEANGLES/4;i++)
	{
		intang = tan(i/radtoint)*(1l<<10);

		//
		// if the tangent is not reprentable in this many bits, bound the
		// units part ONLY
		//
		if (intang>MAXINT)
			intang = 0x8f00 | (intang & 0xff);
		else if (intang<MININT)
			intang = 0xff00 | (intang & 0xff);

		finetangent[i] = intang;
//		finetangent[FINEANGLES/2+i] = intang;
//		finetangent[FINEANGLES/2-i-1] = -intang;
		finetangent[FINEANGLES-i-1] = -intang;
	}

//
// calculate scale value so one tile at mindist allmost fills the view horizontally
//
  scale = GLOBAL1/VIEWWIDTH;
  scale *= focallength;
  scale /= (focallength+mindist);

//
// costable overlays sintable with a quarter phase shift
// ANGLES is assumed to be divisable by four
//
// The low word of the value is the fraction, the high bit is the sign bit,
// bits 16-30 should be 0
//

  angle = 0;
  anglestep = PI/2/ANGLEQUAD;
  for (i=0;i<=ANGLEQUAD;i++)
  {
	value=GLOBAL1*sin(angle);
	sintable[i]=
	  sintable[i+ANGLES]=
	  sintable[ANGLES/2-i] = value;
	sintable[ANGLES-i]=
	  sintable[ANGLES/2+i] = value | 0x80000000l;
	angle += anglestep;
  }

//
// figure trace angles for first and last pixel on screen
//
  angle = atan((float)VIEWWIDTH/2*scale/FOCALLENGTH);
  angle *= ANGLES/(PI*2);

  intang = (int)angle+1;
  firstangle = intang;
  lastangle = -intang;

  prestep = GLOBAL1*((float)FOCALLENGTH/costable[firstangle]);

//
// misc stuff
//
  walls[0].x2 = VIEWX-1;
  walls[0].height2 = 32000;
}


//==========================================================================

/*
=====================
=
= ClearScreen
=
=====================
*/

void ClearScreen (void)
{
  //
  // clear the screen
  //
asm	mov	dx,GC_INDEX
asm	mov	ax,GC_MODE + 256*2		// read mode 0, write mode 2
asm	out	dx,ax
asm	mov	ax,GC_BITMASK + 255*256
asm	out	dx,ax

asm	mov	dx,40-VIEWWIDTH/8
asm	mov	bl,VIEWWIDTH/16
asm	mov	bh,CENTERY+1

asm	xor	ax,ax
asm	mov	es,[screenseg]
asm	mov	di,[bufferofs]

toploop:
asm	mov	cl,bl
asm	rep	stosw
asm	stosb
asm	add	di,dx
asm	dec	bh
asm	jnz	toploop

asm	mov	bh,CENTERY+1
asm	mov	ax,0x0808

bottomloop:
asm	mov	cl,bl
asm	rep	stosw
asm	stosb
asm	add	di,dx
asm	dec	bh
asm	jnz	bottomloop


asm	mov	dx,GC_INDEX
asm	mov	ax,GC_MODE + 256*10		// read mode 1, write mode 2
asm	out	dx,ax
asm	mov	al,GC_BITMASK
asm	out	dx,al

}

//==========================================================================

/*
=====================
=
= DrawWallList
=
= Clips and draws all the walls traced this refresh
=
=====================
*/

void DrawWallList (void)
{
	int i,leftx,newleft,rightclip;
	walltype *wall, *check;

asm	mov	ax,ds
asm	mov	es,ax
asm	mov	di,OFFSET wallwidth
asm	xor	ax,ax
asm	mov	cx,VIEWWIDTH/2
asm	rep	stosw

	ClearScreen ();

	rightwall->x1 = VIEWXH+1;
	rightwall->height1 = 32000;
	(rightwall+1)->x1 = 32000;

	leftx = -1;

	for (wall=&walls[1];wall<rightwall && leftx<=VIEWXH ;wall++)
	{
	  if (leftx >= wall->x2)
		continue;

	  rightclip = wall->x2;

	  check = wall+1;
	  while (check->x1 <= rightclip && check->height1 >= wall->height2)
	  {
		rightclip = check->x1-1;
		check++;
	  }

	  if (rightclip>VIEWXH)
		rightclip=VIEWXH;

	  if (leftx < wall->x1 - 1)
		newleft = wall->x1-1;		// there was black space between walls
	  else
		newleft = leftx;

	  if (rightclip > newleft)
	  {
		wall->leftclip = newleft+1;
		wall->rightclip = rightclip;
		DrawVWall (wall);
		leftx = rightclip;
	  }
	}

#ifndef DRAWEACH
	ScaleWalls ();					// draw all the walls
#endif
}

//==========================================================================

/*
=====================
=
= DrawScaleds
=
= Draws all objects that are visable
=
=====================
*/

objtype *depthsort[MAXACTORS];

void DrawScaleds (void)
{
	int 		i,j,least,numvisable,height;
	objtype 	*obj,**vislist,*farthest;
	memptr		shape;
	byte		*tilespot,*visspot;

	numvisable = 0;

//
// calculate base positions of all objects
//
	vislist = &depthsort[0];

	for (obj = player->next;obj;obj=obj->next)
	{
		if (!obj->state->shapenum)
			continue;

		tilespot = &tilemap[0][0]+(obj->tilex<<6)+obj->tiley;
		visspot = &spotvis[0][0]+(obj->tilex<<6)+obj->tiley;
		//
		// could be in any of the nine surrounding tiles
		//
		if (*visspot
		|| ( *(visspot-1) && !*(tilespot-1) )
		|| ( *(visspot+1) && !*(tilespot+1) )
		|| ( *(visspot-65) && !*(tilespot-65) )
		|| ( *(visspot-64) && !*(tilespot-64) )
		|| ( *(visspot-63) && !*(tilespot-63) )
		|| ( *(visspot+65) && !*(tilespot+65) )
		|| ( *(visspot+64) && !*(tilespot+64) )
		|| ( *(visspot+63) && !*(tilespot+63) ) )
		{
			obj->active = true;
			TransformActor (obj);
			if (!obj->viewheight || obj->viewheight > VIEWWIDTH)
				continue;			// too close or far away

			*vislist++ = obj;
			numvisable++;
		}
	}

	if (vislist == &depthsort[0])
		return;						// no visable objects

//
// draw from back to front
//
	for (i = 0; i<numvisable; i++)
	{
		least = 32000;
		for (j=0;j<numvisable;j++)
		{
			height = depthsort[j]->viewheight;
			if (height < least)
			{
				least = height;
				farthest = depthsort[j];
			}
		}
		//
		// draw farthest
		//
		shape = shapedirectory[farthest->state->shapenum-FIRSTSCALEPIC];
		ScaleShape(farthest->viewx,shape,farthest->viewheight);
		farthest->viewheight = 32000;
	}
}

//==========================================================================


/*
=====================
=
= CalcTics
=
=====================
*/

void CalcTics (void)
{
	long	newtime,oldtimecount;


#ifdef PROFILE
	tics = 1;
	return;
#endif

//
// calculate tics since last refresh for adaptive timing
//
	if (lasttimecount > TimeCount)
		TimeCount = lasttimecount;		// if the game was paused a LONG time

	if (DemoMode)					// demo recording and playback needs
	{								// to be constant
//
// take DEMOTICS or more tics, and modify Timecount to reflect time taken
//
		oldtimecount = lasttimecount;
		while (TimeCount<oldtimecount+DEMOTICS*2)
		;
		lasttimecount = oldtimecount + DEMOTICS;
		TimeCount = lasttimecount + DEMOTICS;
		tics = DEMOTICS;
	}
	else
	{
//
// non demo, so report actual time
//
		newtime = TimeCount;
		tics = newtime-lasttimecount;
		lasttimecount = newtime;

#ifdef FILEPROFILE
			strcpy (scratch,"\tTics:");
			itoa (tics,str,10);
			strcat (scratch,str);
			strcat (scratch,"\n");
			write (profilehandle,scratch,strlen(scratch));
#endif

		if (tics>MAXTICS)
		{
			TimeCount -= (tics-MAXTICS);
			tics = MAXTICS;
		}
	}
}


//==========================================================================


/*
========================
=
= DrawHand
=
========================
*/

void	DrawHand (void)
{
	int	picnum;
	memptr source;
	unsigned dest,width,height;

	picnum = HAND1PICM;
	if (gamestate.shotpower || boltsleft)
		picnum += (((unsigned)TimeCount>>3)&1);

	source = grsegs[picnum];
	dest = ylookup[VIEWHEIGHT-handheight]+12+bufferofs;
	width = picmtable[picnum-STARTPICM].width;
	height = picmtable[picnum-STARTPICM].height;

	VW_MaskBlock(source,0,dest,width,handheight,width*height);
	EGAMAPMASK(15);
}

//==========================================================================


/*
========================
=
= ThreeDRefresh
=
========================
*/

void	ThreeDRefresh (void)
{
	int tracedir;

restart:
	aborttrace = false;

//
// clear out the traced array
//
asm	mov	ax,ds
asm	mov	es,ax
asm	mov	di,OFFSET spotvis
asm	xor	ax,ax
asm	mov	cx,[mapwidth]		// mapheight*32 words
asm	shl	cx,1
asm	shl	cx,1
asm	shl	cx,1
asm	shl	cx,1
asm	shl	cx,1
asm	rep stosw


//
// set up variables for this view
//

	viewangle = player->angle;
	fineviewangle = viewangle*(FINEANGLES/ANGLES);
	viewsin = sintable[viewangle];
	viewcos = costable[viewangle];
	viewx = player->x - FixedByFrac(FOCALLENGTH,viewcos);
	viewy = player->y + FixedByFrac(FOCALLENGTH,viewsin);
	viewx &= 0xfffffc00;		// stop on a pixel boundary
	viewy &= 0xfffffc00;
	viewx += 0x180;
	viewy += 0x180;
	viewxpix = viewx>>10;
	viewypix = viewy>>10;

	focal.x = viewx>>TILESHIFT;
	focal.y = viewy>>TILESHIFT;

//
// find the rightmost visable tile in view
//
	tracedir = viewangle + lastangle;
	if (tracedir<0)
	  tracedir+=ANGLES;
	else if (tracedir>=ANGLES)
	  tracedir-=ANGLES;
	TraceRay( tracedir );
	right.x = tile.x;
	right.y = tile.y;

//
// find the leftmost visable tile in view
//
	tracedir = viewangle + firstangle;
	if (tracedir<0)
	  tracedir+=ANGLES;
	else if (tracedir>=ANGLES)
	  tracedir-=ANGLES;
	TraceRay( tracedir );

//
// follow the walls from there to the right
//
	rightwall = &walls[1];
	FollowWalls ();

	if (aborttrace)
		goto restart;

//
// actually draw stuff
//
	if (++screenpage == 3)
		screenpage = 0;

	bufferofs = screenloc[screenpage];

	EGAWRITEMODE(2);
	EGAMAPMASK(15);

//
// draw the wall list saved be FollowWalls ()
//
	animframe = (TimeCount&8)>>3;

//
// draw all the scaled images
//
	asm	mov	dx,GC_INDEX

	asm	mov	ax,GC_COLORDONTCARE
	asm	out	dx,ax						// don't look at any of the planes

	asm	mov	ax,GC_MODE + 256*(10)		// read mode 1, write mode 2
	asm	out	dx,ax

	asm	mov	al,GC_BITMASK
	asm	out	dx,al

	DrawWallList();
	DrawScaleds();

	EGAWRITEMODE(0);
	EGABITMASK(0xff);

//
// draw hand
//
	if (handheight)
		DrawHand ();

//
// show screen and time last cycle
//
	if (fizzlein)
	{
		fizzlein = false;
		FizzleFade(bufferofs,displayofs,VIEWWIDTH,VIEWHEIGHT,true);
		lasttimecount = TimeCount;
		if (MousePresent) Mouse(MDelta);	// Clear accumulated mouse movement
	}

asm	cli
asm	mov	cx,[bufferofs]
asm	mov	dx,3d4h		// CRTC address register
asm	mov	al,0ch		// start address high register
asm	out	dx,al
asm	inc	dx
asm	mov	al,ch
asm	out	dx,al   	// set the high byte
asm	dec	dx
asm	mov	al,0dh		// start address low register
asm	out	dx,al
asm	inc	dx
asm	mov	al,cl
asm	out	dx,al		// set the low byte
asm	sti

	displayofs = bufferofs;

	CalcTics ();

}

