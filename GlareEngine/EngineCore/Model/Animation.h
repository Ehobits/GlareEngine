#pragma once

#include <vector>
#include <cstdint>

// An animation curve describes how a value (or values) change over time.
// Key frames punctuate the curve, and times between key frames are interpolated
// using the selected method. Note that a curve does not have to be defined for
// the entire animation. Some property might only be animated for a portion of time.

//Animation Curve
struct AnimationCurve
{
	enum { eTranslation, eRotation, eScale, eWeights };			// targetTransform
	enum { eLinear, eStep, eCatmullRomSpline, eCubicSpline };	// interpolation
	enum { eSNorm8, eUNorm8, eSNorm16, eUNorm16, eFloat };		// format

	uint32_t TargetNode : 28;           // Which node is being animated
	uint32_t TargetTransform : 2;       // What aspect of the transform is animated
	uint32_t Interpolation : 2;         // The method of interpolation
	uint32_t KeyFrameOffset : 26;       // Byte offset to first key frame
	uint32_t KeyFrameFormat : 3;        // Data format for the key frames
	uint32_t KeyFrameStride : 3;        // Number of 4-byte words for one key frame
	float NumSegments;                  // Number of evenly-spaced gaps between key frames
	float StartTime;                    // Time stamp of the first key frame
	float RangeScale;                   // numSegments / (endTime - startTime)
};



// An animation is composed of multiple animation curves.
struct AnimationSet
{
	float duration;             // Time to play entire animation
	uint32_t firstCurve;        // Index of the first curve in this set (stored separately)
	uint32_t numCurves;         // Number of curves in this set
};


// Animation state indicates whether an animation is playing and keeps track of current
// position within the animation's playback.
struct AnimationState
{
	enum eMode { kStopped, kPlaying, kLooping };
	eMode state;
	float time;
	AnimationState() : state(kStopped), time(0.0f) {}
};
