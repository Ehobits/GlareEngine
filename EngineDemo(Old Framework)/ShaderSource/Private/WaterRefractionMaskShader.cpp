#include "WaterRefractionMaskShader.h"
WaterRefractionMaskShader::WaterRefractionMaskShader(std::wstring VSShaderPath, std::wstring PSShaderPath, std::wstring HSShaderPath, std::wstring DSShaderPath, std::wstring GSShaderPath, const D3D_SHADER_MACRO* defines)
	:BaseShader(VSShaderPath, PSShaderPath, HSShaderPath, DSShaderPath, GSShaderPath, defines)
{
}

WaterRefractionMaskShader::~WaterRefractionMaskShader()
{
}

std::vector<D3D12_INPUT_ELEMENT_DESC> WaterRefractionMaskShader::GetInputLayout()
{
	return InputLayout::Pos;
}
