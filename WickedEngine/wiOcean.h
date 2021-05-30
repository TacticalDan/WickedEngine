#pragma once
#include "CommonInclude.h"
#include "wiGraphicsDevice.h"
#include "wiFFTGenerator.h"
#include "wiScene_Decl.h"

#include <vector>

class wiOcean
{
public:
	struct OceanParameters
	{
		// Must be power of 2.
		int dmap_dim = 512;
		// Typical value is 1000 ~ 2000
		float patch_length = 50.0f;

		// Adjust the time interval for simulation.
		float time_scale = 0.3f;
		// Amplitude for transverse wave. Around 1.0
		float wave_amplitude = 1000.0f;
		// Wind direction. Normalization not required.
		XMFLOAT2 wind_dir = XMFLOAT2(0.8f, 0.6f);
		// Around 100 ~ 1000
		float wind_speed = 600.0f;
		// This value damps out the waves against the wind direction.
		// Smaller value means higher wind dependency.
		float wind_dependency = 0.07f;
		// The amplitude for longitudinal wave. Must be positive.
		float choppy_scale = 1.3f;


		XMFLOAT4 waterColor = XMFLOAT4(0.0f, 3.0f / 255.0f, 31.0f / 255.0f, 1);
		float waterHeight = 0.0f;
		uint32_t surfaceDetail = 4;
		float surfaceDisplacementTolerance = 2;
	};
	void Create(const OceanParameters& params);

	void UpdateDisplacementMap(const OceanParameters& params, wiGraphics::CommandList cmd) const;
	void Render(const wiScene::CameraComponent& camera, const OceanParameters& params, wiGraphics::CommandList cmd) const;

	const wiGraphics::Texture* getDisplacementMap() const;
	const wiGraphics::Texture* getGradientMap() const;

	static void Initialize();

	bool IsValid() const { return displacementMap.IsValid(); }

protected:
	wiGraphics::Texture displacementMap;		// (RGBA32F)
	wiGraphics::Texture gradientMap;			// (RGBA16F)


	void initHeightMap(const OceanParameters& params, XMFLOAT2* out_h0, float* out_omega);


	// Initial height field H(0) generated by Phillips spectrum & Gauss distribution.
	wiGraphics::GPUBuffer buffer_Float2_H0;

	// Angular frequency
	wiGraphics::GPUBuffer buffer_Float_Omega;

	// Height field H(t), choppy field Dx(t) and Dy(t) in frequency domain, updated each frame.
	wiGraphics::GPUBuffer buffer_Float2_Ht;

	// Height & choppy buffer in the space domain, corresponding to H(t), Dx(t) and Dy(t)
	wiGraphics::GPUBuffer buffer_Float_Dxyz;


	wiGraphics::GPUBuffer immutableCB;
	wiGraphics::GPUBuffer perFrameCB;
};
