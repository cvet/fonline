#ifndef __FLEX_RECT__
#define __FLEX_RECT__

template<typename Ty>
struct FlexRect
{
	Ty L,T,R,B;

	FlexRect():L(0),T(0),R(0),B(0){}
	FlexRect(const FlexRect& fr):L(fr.L),T(fr.T),R(fr.R),B(fr.B){}
	FlexRect(Ty l, Ty t, Ty r, Ty b):L(l),T(t),R(r),B(b){}
	FlexRect(Ty l, Ty t, Ty r, Ty b, Ty ox, Ty oy):L(l+ox),T(t+oy),R(r+ox),B(b+oy){}
	FlexRect(const FlexRect& fr, Ty ox, Ty oy):L(fr.L+ox),T(fr.T+oy),R(fr.R+ox),B(fr.B+oy){}
	FlexRect& operator=(const FlexRect& fr){L=fr.L; T=fr.T; R=fr.R; B=fr.B; return *this;}
	void Clear(){L=0; T=0; R=0; B=0;}
	bool IsZero(){return !L && !T && !R && !B;}
	Ty W(){return R-L+1;}
	Ty H(){return B-T+1;}
	Ty CX(){return L+W()/2;}
	Ty CY(){return T+H()/2;}

	Ty& operator[](int index)
	{ 
		switch(index)
		{
		case 0: return L;
		case 1: return T;
		case 2: return R;
		case 3: return B;
		default: break;
		}
		return L;
	}

	FlexRect& operator()(Ty l, Ty t, Ty r, Ty b){L=l; T=t; R=r; B=b; return *this;}
	FlexRect& operator()(Ty ox, Ty oy){L+=ox; T+=oy; R+=ox; B+=oy; return *this;}
};

typedef FlexRect<int> INTRECT;
typedef FlexRect<float> FLTRECT;

template<typename Ty>
struct FlexPoint
{
	Ty X,Y;

	FlexPoint():X(0),Y(0){}
	FlexPoint(const FlexPoint& r):X(r.X),Y(r.Y){}
	FlexPoint(Ty x, Ty y):X(x),Y(y){}
	FlexPoint(const FlexPoint& fp, Ty ox, Ty oy):X(fp.X+ox),Y(fp.Y+oy){}
	FlexPoint& operator=(const FlexPoint& fp){X=fp.X; Y=fp.Y; return *this;}
	void Clear(){X=0; Y=0;}
	bool IsZero(){return !X && !Y;}

	Ty& operator[](int index)
	{ 
		switch(index)
		{
		case 0: return X;
		case 1: return Y;
		default: break;
		}
		return X;
	}

	FlexPoint& operator()(Ty x, Ty y){X=x; Y=y; return *this;}
};

typedef FlexPoint<int> INTPOINT;
typedef FlexPoint<float> FLTPOINT;


template<typename Ty>
FlexRect<Ty> AverageFlexRect(const FlexRect<Ty>& r1, const FlexRect<Ty> r2, int procent)
{
	FlexRect<Ty> result=r1;
	result.L+=(r2.L-r1.L)*procent/100;
	result.T+=(r2.T-r1.T)*procent/100;
	result.R+=(r2.R-r1.R)*procent/100;
	result.B+=(r2.B-r1.B)*procent/100;
	return result;
}

#endif // __FLEX_RECT__