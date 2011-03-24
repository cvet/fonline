#ifndef __LINE_TRACER__
#define __LINE_TRACER__

#include "Common.h"

class LineTracer
{
public:
	LineTracer(ushort hx, ushort hy, ushort tx, ushort ty, ushort maxhx, ushort maxhy, float angle, bool is_square);
	uchar GetNextHex(ushort& cx, ushort& cy);
	void GetNextSquare(ushort& cx, ushort& cy);

private:
	ushort maxHx;
	ushort maxHy;
	float x1;
	float y1;
	float x2;
	float y2;
	float dir;
	uchar dir1;
	uchar dir2;
	float dx;
	float dy;

	void NormalizeDir();
};

void LineTracer::NormalizeDir()
{
	if(dir<=0.0f) dir=360.0f-fmod(-dir,360.0f);
	else if(dir>=0.0f) dir=fmod(dir,360.0f);
}

LineTracer::LineTracer(ushort hx, ushort hy, ushort tx, ushort ty, ushort maxhx, ushort maxhy, float angle, bool is_square)
{
	maxHx=maxhx;
	maxHy=maxhy;

	if(is_square)
	{
		dir=atan2((float)(ty-hy),(float)(tx-hx))+angle;
		dx=cos(dir);
		dy=sin(dir);
		if(fabs(dx)>fabs(dy))
		{
			dy/=fabs(dx);
			dx=(dx>0?1.0f:-1.0f);
		}
		else
		{
			dx/=fabs(dy);
			dy=(dy>0?1.0f:-1.0f);
		}
		x1=(float)hx+0.5f;
		y1=(float)hy+0.5f;
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

uchar LineTracer::GetNextHex(ushort& cx, ushort& cy)
{
	ushort t1x=cx;
	ushort t2x=cx;
	ushort t1y=cy;
	ushort t2y=cy;
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

void LineTracer::GetNextSquare(ushort& cx, ushort& cy)
{
	x1+=dx;
	y1+=dy;
	cx=(ushort)floor(x1);
	cy=(ushort)floor(y1);
	if(cx>=maxHx) cx=maxHx-1;
	if(cy>=maxHy) cy=maxHy-1;
}

#endif // __LINE_TRACER__
