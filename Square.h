#pragma once
class Square
{
public:
	Square::Square(int x,int y,double v,int s,int valid);
	~Square(void);
	int x;
	int y;
	double velocity;
	int sqrsize;
	bool isValid;
	bool initialized;
	bool isHitted(int inx,int iny);
	int isReachedBottom(int height);
	void goDown(double pixel);
	void randAppear(int width);

};

