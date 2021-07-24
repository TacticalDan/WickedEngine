#include "globals.hlsli"
#include "ShaderInterop_Renderer.h"

RWRAWBUFFER(surfelStatsBuffer, 0);

[numthreads(1, 1, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
	uint surfel_count = surfelStatsBuffer.Load(SURFEL_STATS_OFFSET_COUNT);
	surfel_count = min(surfel_count, SURFEL_CAPACITY);
	surfelStatsBuffer.Store(SURFEL_STATS_OFFSET_COUNT, surfel_count);

	surfelStatsBuffer.Store3(SURFEL_STATS_OFFSET_INDIRECT, uint3((surfel_count + 63) / 64, 1, 1));
}
