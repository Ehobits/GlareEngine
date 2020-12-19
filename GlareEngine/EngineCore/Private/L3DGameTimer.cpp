#include <windows.h>
#include "L3DGameTimer.h"

GameTimer::GameTimer()
: mSecondsPerCount(0.0), mDeltaTime(-1.0), mBaseTime(0), 
  mPausedTime(0), mPrevTime(0), mCurrTime(0), mStopped(false)
{
	__int64 countsPerSec;
	QueryPerformanceFrequency((LARGE_INTEGER*)&countsPerSec);
	mSecondsPerCount = 1.0 / (double)countsPerSec;
}

//�����Ե���Reset����������������ʱ�䣬������ʱ��ֹͣ���κ�ʱ�䡣
float GameTimer::TotalTime()const
{
	// �������ֹͣ�ˣ���Ҫ����������ֹͣ�����Ѿ���ȥ��ʱ�䡣
	//���⣬�������֮ǰ�Ѿ���ͣ�������mStopTime - mBaseTime������ͣʱ�䣬
	//���ǲ�����㡣 Ҫ����������⣬���ǿ��Դ�mStopTime�м�ȥ��ͣʱ�䣺
	//
	//                     |<--paused time-->|
	// ----*---------------*-----------------*------------*------------*------> time
	//  mBaseTime       mStopTime        startTime     mStopTime    mCurrTime

	if( mStopped )
	{
		return (float)(((mStopTime - mPausedTime)-mBaseTime)*mSecondsPerCount);
	}

	//����mCurrTime - mBaseTime������ͣʱ�䣬���ǲ�����㡣
	//Ҫ����������⣬���ǿ��Դ�mCurrTime�м�ȥ��ͣʱ�䣺
	//
	//  (mCurrTime - mPausedTime) - mBaseTime 
	//
	//                     |<--paused time-->|
	// ----*---------------*-----------------*------------*------> time
	//  mBaseTime       mStopTime        startTime     mCurrTime
	
	else
	{
		return (float)(((mCurrTime-mPausedTime)-mBaseTime)*mSecondsPerCount);
	}
}

float GameTimer::DeltaTime()const
{
	return (float)mDeltaTime;
}

void GameTimer::Reset()
{
	__int64 currTime;
	QueryPerformanceCounter((LARGE_INTEGER*)&currTime);

	mBaseTime = currTime;
	mPrevTime = currTime;
	mStopTime = 0;
	mStopped  = false;
}

void GameTimer::Start()
{
	__int64 startTime;
	QueryPerformanceCounter((LARGE_INTEGER*)&startTime);


	// �ۼ�ֹͣ��������֮�侭����ʱ�䡣
	//
	//                     |<-------d------->|
	// ----*---------------*-----------------*------------> time
	//  mBaseTime       mStopTime        startTime     

	if( mStopped )
	{
		mPausedTime += (startTime - mStopTime);	

		mPrevTime = startTime;
		mStopTime = 0;
		mStopped  = false;
	}
}

void GameTimer::Stop()
{
	if( !mStopped )
	{
		__int64 currTime;
		QueryPerformanceCounter((LARGE_INTEGER*)&currTime);

		mStopTime = currTime;
		mStopped  = true;
	}
}

void GameTimer::Tick()
{
	if( mStopped )
	{
		mDeltaTime = 0.0;
		return;
	}

	__int64 currTime;
	QueryPerformanceCounter((LARGE_INTEGER*)&currTime);
	mCurrTime = currTime;

	//��֡��ǰһ֮֡���ʱ��
	mDeltaTime = (mCurrTime - mPrevTime)*mSecondsPerCount;

	// ׼����һ֡
	mPrevTime = mCurrTime;

	//ǿ�ƷǸ��� DXSDK��CDXUTTimer�ᵽ��
	//�������������ʡ��ģʽ�������Ǳ��ϵ���һ����������
	//��ômDeltaTime�����Ǹ�����
	if(mDeltaTime < 0.0)
	{
		mDeltaTime = 0.0;
	}
}

