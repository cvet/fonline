#pragma once

#include <array>
#include <stdint.h>

const int32_t MarkerMax = 8;

struct ImGradientHDRState
{
	struct ColorMarker
	{
		float Position;
		std::array<float, 3> Color;
		float Intensity;
	};

	struct AlphaMarker
	{
		float Position;
		float Alpha;
	};

	int ColorCount = 0;
	int AlphaCount = 0;
	std::array<ColorMarker, MarkerMax> Colors;
	std::array<AlphaMarker, MarkerMax> Alphas;

	ColorMarker* GetColorMarker(int32_t index);

	AlphaMarker* GetAlphaMarker(int32_t index);

	bool AddColorMarker(float x, std::array<float, 3> color, float intensity);

	bool AddAlphaMarker(float x, float alpha);

	bool RemoveColorMarker(int32_t index);

	bool RemoveAlphaMarker(int32_t index);

	std::array<float, 4> GetCombinedColor(float x) const;

	std::array<float, 4> GetColorAndIntensity(float x) const;

	float GetAlpha(float x) const;
};

enum class ImGradientHDRMarkerType
{
	Color,
	Alpha,
	Unknown,
};

struct ImGradientHDRTemporaryState
{
	ImGradientHDRMarkerType selectedMarkerType = ImGradientHDRMarkerType::Unknown;
	int selectedIndex = -1;

	ImGradientHDRMarkerType draggingMarkerType = ImGradientHDRMarkerType::Unknown;
	int draggingIndex = -1;
};

bool ImGradientHDR(int32_t gradientID, ImGradientHDRState& state, ImGradientHDRTemporaryState& temporaryState, bool isMarkerShown = true);
