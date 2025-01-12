struct PSInput
{
    float4 color : COLOR;
};

float4 main(PSInput i) : SV_Target
{
    return i.color;
}