#include "GameTimer.h"
#include<windows.h>

GameTimer::GameTimer()
	:mSecondsPerCount(0.0), mDeltaTime(-1.0), mBaseTime(0),
	mPausedTime(0), mPrevTime(0), mCurrTime(0), mStopped(false)
{
	__int64	countPerSec;
	QueryPerformanceFrequency((LARGE_INTEGER*)&countPerSec);
	mSecondsPerCount = 1.0 / (double)countPerSec;
}
float GameTimer::TotalTime()const {


	if (mStopped) {
		//现在是暂停状态 游戏总时间等于现在时间减去暂停的时间和基础时间
		//  (mCurrTime - mPausedTime) - mBaseTime 
//
//                     |<--paused time-->|
// ----*---------------*-----------------*------------*------> time
//  mBaseTime       mStopTime        startTime     mCurrTime

		return (float)(((mStopTime - mPausedTime) - mBaseTime) * mSecondsPerCount);

	}
	else {
		return(float)(((mCurrTime-mPausedTime)-mBaseTime) * mSecondsPerCount);
	}
}

float GameTimer::DeltaTime()const {
	return (float)mDeltaTime;
}
void GameTimer::Reset() {
	__int64	currTime;
	QueryPerformanceCounter((LARGE_INTEGER*)&currTime);

	mBaseTime = currTime;
	mPrevTime = currTime;
	mStopTime = 0;
	mStopped = false;

}

void GameTimer::Start() {
	__int64 startTime;
	QueryPerformanceCounter((LARGE_INTEGER*)&startTime);

	if (mStopped)
	{
		mPausedTime += (startTime - mStopTime);

		mPrevTime = startTime;
		mStopTime = 0;
		mStopped = false;

	}
}

void GameTimer::Stop()
{
	if (!mStopped)
	{
		__int64 currTime;
		QueryPerformanceCounter((LARGE_INTEGER*)&currTime);

		mStopTime = currTime;
		mStopped = true;
	}
}

void GameTimer::Tick()
{
	if (mStopped)
	{
		mDeltaTime = 0.0;
		return;
	}

	__int64 currTime;
	QueryPerformanceCounter((LARGE_INTEGER*)&currTime);
	mCurrTime = currTime;

	mDeltaTime = (mCurrTime - mPrevTime) * mSecondsPerCount;

	mPrevTime = mCurrTime;

	if (mDeltaTime < 0.0)
	{
		mDeltaTime;
	}

	
}