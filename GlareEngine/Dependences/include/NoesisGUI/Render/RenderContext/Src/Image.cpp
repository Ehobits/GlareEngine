////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#include <NsRender/Image.h>
#include <NsCore/Memory.h>


using namespace NoesisApp;


////////////////////////////////////////////////////////////////////////////////////////////////////
Image::Image(uint32_t width, uint32_t height): mData((uint8_t*)Noesis::Alloc(width * height * 4)),
    mWidth(width), mHeight(height)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////
Image::~Image()
{
    Noesis::Dealloc(mData);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
uint32_t Image::Width() const
{
    return mWidth;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
uint32_t Image::Height() const
{
    return mHeight;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
uint32_t Image::Stride() const
{
    return mWidth * 4;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
uint8_t* Image::Data()
{
    return mData;
}
