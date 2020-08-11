#include "Common.hlsli"

struct VertexOut
{
	float4 PosH    : SV_POSITION;
	float2 TexC    : TEXCOORD;
};



//�������alpha���м��Σ��Ա���Ӱ��ȷ��ʾ�� 
//����Ҫ��������в����ļ��������ʹ��NULL������ɫ��������ȴ��ݡ�
void PS(VertexOut pin)
{
	// Fetch the material data.
	MaterialData matData = gMaterialData[gMaterialIndex];
	float4 diffuseAlbedo = matData.DiffuseAlbedo;
	uint diffuseMapIndex = matData.DiffuseMapIndex;

	// Dynamically look up the texture in the array.
	diffuseAlbedo *= gSRVMap[diffuseMapIndex].Sample(gsamAnisotropicWrap, pin.TexC);

#ifdef ALPHA_TEST
	// Discard pixel if texture alpha < 0.1.  We do this test as soon 
	// as possible in the shader so that we can potentially exit the
	// shader early, thereby skipping the rest of the shader code.
	clip(diffuseAlbedo.a - 0.1f);
#endif
}