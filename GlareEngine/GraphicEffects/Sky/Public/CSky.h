#pragma once
#include "RanderObject.h"

class CSky :
    public RanderObject
{
public:
    CSky();
    ~CSky();

    virtual void Draw();
    virtual void SetPSO();

private:
    //��պе�Mesh
    std::unique_ptr<MeshGeometry> mSkyMesh;
};

