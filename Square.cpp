#include "Square.h"
#include <time.h>
#include <stdlib.h>
#include <windows.h>
#include <mmsystem.h>
#pragma comment(lib,"WINMM.LIB")

Square::Square(int inx,int iny,double inv,int ins,int valid)
{
	this->x=inx;
	this->y=iny;
	this->velocity=inv;
	this->sqrsize=ins;
	this->isValid=valid;
	this->initialized = false;
}

bool Square::isHitted(int inx,int iny){
	if (this->isValid == true &&((inx >= this->x) && (inx <= this->x + this->sqrsize)) && ((iny >= this->y) && (iny <= this->y + this->sqrsize))){
		this->isValid = false;
		PlaySound(L"C:\\hit.wav", NULL, SND_ASYNC|SND_FILENAME);
		//PlaySound(L"hit.wav", NULL, SND_ASYNC|SND_FILENAME|SND_LOOP);
		return true;
	}
	return false;
}

int Square::isReachedBottom(int height){
	if ((this->y   > height)&&(this->isValid == true)){
		return -1;
	}
	if ((this->y > height)&&(this->isValid == false)){
		return 1;
	}
	return 0;
}

void Square::goDown(double pixel){
	this->y = (int)((double)this->y + pixel*this->velocity);
	int i = 1;
}

void Square::randAppear(int width){
	//srand((unsigned)time(NULL));//
	this->x = rand()%(width+1);
	this->y = 0;
	this->isValid = true;
	this->initialized = true;
}

Square::~Square(void)
{
}
