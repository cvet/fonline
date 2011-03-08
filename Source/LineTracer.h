#ifndef __LINE_TRACER__
#define __LINE_TRACER__

#include "Common.h"

class LineTracer
{
public:
	LineTracer(WORD hx, WORD hy, WORD tx, WORD ty, WORD maxhx, WORD maxhy, float angle, bool is_square);
	BYTE GetNextHex(WORD& cx, WORD& cy);
	void GetNextSquare(WORD& cx, WORD& cy);

private:
	WORD maxHx;
	WORD maxHy;
	float x1;
	float y1;
	float x2;
	float y2;
	float dir;
	BYTE dir1;
	BYTE dir2;
	float dx;
	float dy;

	void NormalizeDir();
};

void LineTracer::NormalizeDir()
{
	if(dir<=0.0f) dir=360.0f-fmod(-dir,360.0f);
	else if(dir>=0.0f) dir=fmod(dir,360.0f);
}

LineTracer::LineTracer(WORD hx, WORD hy, WORD tx, WORD ty, WORD maxhx, WORD maxhy, float angle, bool is_square)
{
	maxHx=maxhx;
	maxHy=maxhy;

	if(is_square)
	{
		dir=atan2((float)(ty-hy),(float)(tx-hx))+angle;
		dx=cos(dir);
		dy=sin(dir);
		if(abs(dx)>abs(dy))
		{
			dy/=fabs(dx);
			dx=(dx>0?1:-1);
		}
		else
		{
			dx/=fabs(dy);
			dy=(dy>0?1:-1);
		}
		x1=(float)hx+0.5;
		y1=(float)hy+0.5;
	}
	else
	{
		float nx=3.0f*(float(tx)-float(hx));
		float ny=(float(ty)-float(hy))*SQRT3T2_FLOAT-(float(tx&1)-float(hx&1))*SQRT3_FLOAT;
		this->dir=180.0f+RAD2DEG*atan2f(ny,nx);
		if(angle!=0.0f)
		{
			this->dir+=angle;
			NormalizeDir();
		}

		if(dir>=30.0f && dir<90.0f) { dir1=5; dir2=0; }
		else if(dir>=90.0f && dir<150.0f) { dir1=4; dir2=5; }
		else if(dir>=150.0f && dir<210.0f) { dir1=3; dir2=4; }
		else if(dir>=210.0f && dir<270.0f) { dir1=2; dir2=3; }
		else if(dir>=270.0f && dir<330.0f) { dir1=1; dir2=2; }
		else { dir1=0; dir2=1; }

		x1=3.0f*float(hx)+BIAS_FLOAT;
		y1=SQRT3T2_FLOAT*float(hy)-SQRT3_FLOAT*(float(hx&1))+BIAS_FLOAT;
		x2=3.0f*float(tx)+BIAS_FLOAT+BIAS_FLOAT;
		y2=SQRT3T2_FLOAT*float(ty)-SQRT3_FLOAT*(float(tx&1))+BIAS_FLOAT;
		if(angle!=0.0f)
		{
			x2-=x1;
			y2-=y1;
			float xp=cos(angle/RAD2DEG)*x2-sin(angle/RAD2DEG)*y2;
			float yp=sin(angle/RAD2DEG)*x2+cos(angle/RAD2DEG)*y2;
			x2=x1+xp;
			y2=y1+yp;
		}
		dx=x2-x1;
		dy=y2-y1;
	}
}

BYTE LineTracer::GetNextHex(WORD& cx, WORD& cy)
{
	WORD t1x=cx;
	WORD t2x=cx;
	WORD t1y=cy;
	WORD t2y=cy;
	MoveHexByDir(t1x,t1y,dir1,maxHx,maxHy);
	MoveHexByDir(t2x,t2y,dir2,maxHx,maxHy);
	float dist1=dx*(y1-(SQRT3T2_FLOAT*float(t1y)-(float(t1x&1))*SQRT3_FLOAT))-dy*(x1-3*float(t1x));
	float dist2=dx*(y1-(SQRT3T2_FLOAT*float(t2y)-(float(t2x&1))*SQRT3_FLOAT))-dy*(x1-3*float(t2x));
	dist1=(dist1>0?dist1:-dist1);
	dist2=(dist2>0?dist2:-dist2);
	if(dist1<=dist2) // Left hand biased
	{
		cx=t1x;
		cy=t1y;
		return dir1;
	}
	else
	{
		cx=t2x;
		cy=t2y;
		return dir2;
	}
}

void LineTracer::GetNextSquare(WORD& cx, WORD& cy)
{
	x1+=dx;
	y1+=dy;
	cx=(WORD)floor(x1);
	cy=(WORD)floor(y1);
	cx=CLAMP(cx,0,maxHx);
	cy=CLAMP(cy,0,maxHy);
}

#endif // __LINE_TRACER__