#pragma once
#include "BaseShader.h"
class HeightMapTerrainShader :
	public BaseShader
{
public:
	HeightMapTerrainShader(std::wstring VSShaderPath, std::wstring PSShaderPath, std::wstring HSShaderPath = L"", std::wstring DSShaderPath = L"", std::wstring GSShaderPath = L"", _In_reads_opt_(_Inexpressible_(defines->Name != NULL)) const D3D_SHADER_MACRO* defines = nullptr);
	~HeightMapTerrainShader();
	virtual std::vector<D3D12_INPUT_ELEMENT_DESC> GetInputLayout();
};

