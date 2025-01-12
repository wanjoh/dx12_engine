struct VSInput
{
    float3 position : POSITION;
    float3 color : COLOR;
};

struct VSOutput
{
    float4 color : COLOR;
    float4 position : SV_Position;
};

struct ModelViewProjection
{
    matrix modelViewProjMatrix;
};

ConstantBuffer<ModelViewProjection> ModelViewProjectionCB : register(b0);

VSOutput main(VSInput i)
{
    VSOutput o;
    o.position = mul(ModelViewProjectionCB.modelViewProjMatrix, float4(i.position, 1.f));
    o.color = float4(i.color, 1.f);
    return o;
}
