#include "ImGradientHDR.h"

#include <algorithm>
#include <cmath>
#include <imgui.h>
#include <string>
#include <vector>

namespace
{
template <typename T>
void AddMarker(std::array<T, MarkerMax>& a, int32_t& count, T value)
{
	const auto lb = std::lower_bound(
		a.begin(),
		a.begin() + count,
		value,
		[&](const T& a, const T& b) -> bool
		{ return a.Position < b.Position; });

	if (lb != a.end())
	{
		const auto ind = lb - a.begin();
		std::copy(a.begin() + ind, a.begin() + count, a.begin() + ind + 1);
		*(a.begin() + ind) = value;
		count++;
	}
}

enum class DrawMarkerMode
{
	Selected,
	Unselected,
	None,
};

void DrawMarker(const ImVec2& pmin, const ImVec2& pmax, const ImU32& color, DrawMarkerMode mode)
{
	auto drawList = ImGui::GetWindowDrawList();
	const auto w = static_cast<int32_t>(pmax.x - pmin.x);
	const auto h = static_cast<int32_t>(pmax.y - pmin.y);
	const auto sign = std::signbit(static_cast<float>(h)) ? -1 : 1;

	const auto margin = 2;
	const auto marginh = margin * sign;

	if (mode != DrawMarkerMode::None)
	{
		const auto outlineColor = mode == DrawMarkerMode::Selected ? ImGui::ColorConvertFloat4ToU32({0.0f, 0.0f, 1.0f, 1.0f}) : ImGui::ColorConvertFloat4ToU32({0.2f, 0.2f, 0.2f, 1.0f});

		drawList->AddTriangleFilled(
			{pmin.x + w / 2, pmin.y},
			{pmin.x + 0, pmin.y + h / 2},
			{pmin.x + w, pmin.y + h / 2},
			outlineColor);

		drawList->AddRectFilled({pmin.x + 0, pmin.y + h / 2}, {pmin.x + w, pmin.y + h}, outlineColor);
	}

	drawList->AddTriangleFilled(
		{pmin.x + w / 2, pmin.y + marginh},
		{pmin.x + 0 + margin, pmin.y + h / 2},
		{pmin.x + w - margin, pmin.y + h / 2},
		color);

	drawList->AddRectFilled({pmin.x + 0 + margin, pmin.y + h / 2}, {pmin.x + w - margin, pmin.y + h - marginh}, color);
};

template <typename T>
void SortMarkers(std::array<T, MarkerMax>& a, int32_t& count, int32_t& selectedIndex, int32_t& draggingIndex)
{
	struct SortedMarker
	{
		int index;
		T marker;
	};

	std::vector<SortedMarker> sortedMarker;

	for (int32_t i = 0; i < count; i++)
	{
		sortedMarker.emplace_back(SortedMarker{i, a[i]});
	}

	std::sort(sortedMarker.begin(), sortedMarker.end(), [](const SortedMarker& a, const SortedMarker& b)
			  { return a.marker.Position < b.marker.Position; });

	for (int32_t i = 0; i < count; i++)
	{
		a[i] = sortedMarker[i].marker;
	}

	if (selectedIndex != -1)
	{
		for (int32_t i = 0; i < count; i++)
		{
			if (sortedMarker[i].index == selectedIndex)
			{
				selectedIndex = i;
				break;
			}
		}
	}

	if (draggingIndex != -1)
	{
		for (int32_t i = 0; i < count; i++)
		{
			if (sortedMarker[i].index == draggingIndex)
			{
				draggingIndex = i;
				break;
			}
		}
	}
}

ImU32 GetMarkerColor(const ImGradientHDRState::ColorMarker& marker)
{
	const auto c = marker.Color;
	return ImGui::ColorConvertFloat4ToU32({c[0], c[1], c[2], 1.0f});
}

ImU32 GetMarkerColor(const ImGradientHDRState::AlphaMarker& marker)
{
	const auto c = marker.Alpha;
	return ImGui::ColorConvertFloat4ToU32({c, c, c, 1.0f});
}

enum class MarkerDirection
{
	ToUpper,
	ToLower,
};

struct UpdateMarkerResult
{
	bool isChanged;
	bool isHovered;
};

template <typename T>
UpdateMarkerResult UpdateMarker(
	std::array<T, MarkerMax>& markerArray,
	int& markerCount,
	ImGradientHDRTemporaryState& temporaryState,
	ImGradientHDRMarkerType markerType,
	const char* keyStr,
	ImVec2 originPos,
	float width,
	float markerWidth,
	float markerHeight,
	MarkerDirection markerDir)
{
	UpdateMarkerResult ret;
	ret.isChanged = false;
	ret.isHovered = false;

	for (int i = 0; i < markerCount; i++)
	{
		const auto x = (int)(markerArray[i].Position * width);
		ImGui::SetCursorScreenPos({originPos.x + x - 5, originPos.y});

		DrawMarkerMode mode;
		if (temporaryState.selectedMarkerType == markerType && temporaryState.selectedIndex == i)
		{
			mode = DrawMarkerMode::Selected;
		}
		else
		{
			mode = DrawMarkerMode::Unselected;
		}

		if (markerDir == MarkerDirection::ToLower)
		{
			DrawMarker(
				{originPos.x + x - 5, originPos.y + markerHeight},
				{originPos.x + x + 5, originPos.y + 0},
				GetMarkerColor(markerArray[i]),
				mode);
		}
		else
		{
			DrawMarker(
				{originPos.x + x - 5, originPos.y + 0},
				{originPos.x + x + 5, originPos.y + markerHeight},
				GetMarkerColor(markerArray[i]),
				mode);
		}

		ImGui::InvisibleButton((keyStr + std::to_string(i)).c_str(), {markerWidth, markerHeight});

		ret.isHovered |= ImGui::IsItemHovered();

		if (temporaryState.draggingIndex == -1 && ImGui::IsItemHovered() && ImGui::IsMouseDown(0))
		{
			temporaryState.selectedMarkerType = markerType;
			temporaryState.selectedIndex = i;
			temporaryState.draggingMarkerType = markerType;
			temporaryState.draggingIndex = i;
		}

		if (!ImGui::IsMouseDown(0))
		{
			temporaryState.draggingIndex = -1;
			temporaryState.draggingMarkerType = ImGradientHDRMarkerType::Unknown;
		}

		if (temporaryState.draggingMarkerType == markerType && temporaryState.draggingIndex == i && ImGui::IsMouseDragging(0))
		{
			const auto diff = ImGui::GetIO().MouseDelta.x / width;
			markerArray[i].Position += diff;
			markerArray[i].Position = std::max(std::min(markerArray[i].Position, 1.0f), 0.0f);

			ret.isChanged |= diff != 0.0f;
		}
	}

	return ret;
}
} // namespace

ImGradientHDRState::ColorMarker* ImGradientHDRState::GetColorMarker(int32_t index)
{
	if (index < 0 ||
		index >= ColorCount)
	{
		return nullptr;
	}

	return &(Colors[index]);
}

ImGradientHDRState::AlphaMarker* ImGradientHDRState::GetAlphaMarker(int32_t index)
{
	if (index < 0 ||
		index >= AlphaCount)
	{
		return nullptr;
	}

	return &(Alphas[index]);
}

bool ImGradientHDRState::AddColorMarker(float x, std::array<float, 3> color, float intensity)
{
	if (ColorCount >= MarkerMax)
	{
		return false;
	}

	x = std::max(std::min(x, 1.0f), 0.0f);

	const auto marker = ColorMarker{x, color, intensity};
	AddMarker(Colors, ColorCount, marker);
	return true;
}

bool ImGradientHDRState::AddAlphaMarker(float x, float alpha)
{
	if (AlphaCount >= MarkerMax)
	{
		return false;
	}

	x = std::max(std::min(x, 1.0f), 0.0f);

	const auto marker = AlphaMarker{x, alpha};

	AddMarker(Alphas, AlphaCount, marker);
	return true;
}

bool ImGradientHDRState::RemoveColorMarker(int32_t index)
{
	if (index >= ColorCount || index < 0)
	{
		return false;
	}

	std::copy(Colors.begin() + index + 1, Colors.end(), Colors.begin() + index);
	ColorCount--;
	return true;
}

bool ImGradientHDRState::RemoveAlphaMarker(int32_t index)
{
	if (index >= AlphaCount || index < 0)
	{
		return false;
	}

	std::copy(Alphas.begin() + index + 1, Alphas.end(), Alphas.begin() + index);
	AlphaCount--;
	return true;
}

std::array<float, 4> ImGradientHDRState::GetCombinedColor(float x) const
{
	const auto c = GetColorAndIntensity(x);
	return std::array<float, 4>{c[0] * c[3], c[1] * c[3], c[2] * c[3], GetAlpha(x)};
}

std::array<float, 4> ImGradientHDRState::GetColorAndIntensity(float x) const
{
	if (ColorCount == 0)
	{
		return std::array<float, 4>{1.0f, 1.0f, 1.0f, 1.0f};
	}

	if (x < Colors[0].Position)
	{
		const auto c = Colors[0].Color;
		return {c[0], c[1], c[2], Colors[0].Intensity};
	}

	if (Colors[ColorCount - 1].Position <= x)
	{
		const auto c = Colors[ColorCount - 1].Color;
		return {c[0], c[1], c[2], Colors[ColorCount - 1].Intensity};
	}

	auto key = ColorMarker();
	key.Position = x;

	auto it = std::lower_bound(Colors.begin(), Colors.begin() + ColorCount, key, [](const ColorMarker& a, const ColorMarker& b)
							   { return a.Position < b.Position; });
	auto ind = static_cast<int32_t>(std::distance(Colors.begin(), it));

	{
		if (Colors[ind].Position != x)
		{
			ind--;
		}

		if (Colors[ind].Position <= x && x <= Colors[ind + 1].Position)
		{
			const auto area = Colors[ind + 1].Position - Colors[ind].Position;
			if (area == 0)
			{
				return std::array<float, 4>{Colors[ind].Color[0], Colors[ind].Color[1], Colors[ind].Color[2], Colors[ind].Intensity};
			}

			const auto alpha = (x - Colors[ind].Position) / area;
			const auto r = Colors[ind + 1].Color[0] * alpha + Colors[ind].Color[0] * (1.0f - alpha);
			const auto g = Colors[ind + 1].Color[1] * alpha + Colors[ind].Color[1] * (1.0f - alpha);
			const auto b = Colors[ind + 1].Color[2] * alpha + Colors[ind].Color[2] * (1.0f - alpha);
			const auto intensity = Colors[ind + 1].Intensity * alpha + Colors[ind].Intensity * (1.0f - alpha);
			return std::array<float, 4>{r, g, b, intensity};
		}
		else
		{
			assert(0);
		}
	}

	return std::array<float, 4>{1.0f, 1.0f, 1.0f, 1.0f};
}

float ImGradientHDRState::GetAlpha(float x) const
{
	if (AlphaCount == 0)
	{
		return 1.0f;
	}

	if (x < Alphas[0].Position)
	{
		return Alphas[0].Alpha;
	}

	if (Alphas[AlphaCount - 1].Position <= x)
	{
		return Alphas[AlphaCount - 1].Alpha;
	}

	auto key = AlphaMarker();
	key.Position = x;

	auto it = std::lower_bound(Alphas.begin(), Alphas.begin() + AlphaCount, key, [](const AlphaMarker& a, const AlphaMarker& b)
							   { return a.Position < b.Position; });
	auto ind = static_cast<int32_t>(std::distance(Alphas.begin(), it));

	{
		if (Alphas[ind].Position != x)
		{
			ind--;
		}

		if (Alphas[ind].Position <= x && x <= Alphas[ind + 1].Position)
		{
			const auto area = Alphas[ind + 1].Position - Alphas[ind].Position;
			if (area == 0)
			{
				return Alphas[ind].Alpha;
			}

			const auto alpha = (x - Alphas[ind].Position) / area;
			return Alphas[ind + 1].Alpha * alpha + Alphas[ind].Alpha * (1.0f - alpha);
		}
		else
		{
			assert(0);
		}
	}

	return 1.0f;
}

bool ImGradientHDR(int32_t gradientID, ImGradientHDRState& state, ImGradientHDRTemporaryState& temporaryState, bool isMarkerShown)
{
	bool changed = false;

	ImGui::PushID(gradientID);
	ImGui::BeginGroup();

	auto originPos = ImGui::GetCursorScreenPos();

	auto drawList = ImGui::GetWindowDrawList();

	const auto margin = 5;

	const auto width = ImGui::GetContentRegionAvail().x - margin * 2;
	const auto barHeight = 20;
	const auto markerWidth = 10;
	const auto markerHeight = 15;

	if (isMarkerShown)
	{
		const auto resultAlpha = UpdateMarker(state.Alphas, state.AlphaCount, temporaryState, ImGradientHDRMarkerType::Alpha, "a", originPos, width, markerWidth, markerHeight, MarkerDirection::ToLower);

		changed |= resultAlpha.isChanged;

		if (temporaryState.draggingMarkerType == ImGradientHDRMarkerType::Alpha)
		{
			SortMarkers(state.Alphas, state.AlphaCount, temporaryState.selectedIndex, temporaryState.draggingIndex);
		}

		ImGui::SetCursorScreenPos(originPos);

		ImGui::InvisibleButton("AlphaArea", {width, static_cast<float>(markerHeight)});

		if (ImGui::IsItemHovered())
		{
			const float x = (ImGui::GetIO().MousePos.x - originPos.x);
			const float xn = (ImGui::GetIO().MousePos.x - originPos.x) / width;
			const auto c = state.GetAlpha(xn);

			if (!resultAlpha.isHovered && state.AlphaCount < state.Alphas.size())
			{
				DrawMarker(
					{originPos.x + x - 5, originPos.y + markerHeight},
					{originPos.x + x + 5, originPos.y + 0},
					ImGui::ColorConvertFloat4ToU32({c, c, c, 0.5f}),
					DrawMarkerMode::None);
			}

			if (!resultAlpha.isHovered && ImGui::IsMouseClicked(0))
			{
				changed |= state.AddAlphaMarker(xn, c);
			}
		}
	}

	const auto barOriginPos = ImGui::GetCursorScreenPos();

	ImGui::Dummy({width, static_cast<float>(barHeight)});

	const int32_t gridSize = 10;

	drawList->AddRectFilled(ImVec2(barOriginPos.x - 2, barOriginPos.y - 2),
							ImVec2(barOriginPos.x + width + 2, barOriginPos.y + barHeight + 2),
							IM_COL32(100, 100, 100, 255));

	for (int y = 0; y * gridSize < barHeight; y += 1)
	{
		for (int x = 0; x * gridSize < width; x += 1)
		{
			int wgrid = std::min(gridSize, static_cast<int>(width) - x * gridSize);
			int hgrid = std::min(gridSize, barHeight - y * gridSize);
			ImU32 color = IM_COL32(100, 100, 100, 255);

			if ((x + y) % 2 == 0)
			{
				color = IM_COL32(50, 50, 50, 255);
			}

			drawList->AddRectFilled(ImVec2(barOriginPos.x + x * gridSize, barOriginPos.y + y * gridSize),
									ImVec2(barOriginPos.x + x * gridSize + wgrid, barOriginPos.y + y * gridSize + hgrid),
									color);
		}
	}

	{
		std::vector<float> xkeys;
		xkeys.reserve(16);

		for (int32_t i = 0; i < state.ColorCount; i++)
		{
			xkeys.emplace_back(state.Colors[i].Position);
		}

		for (int32_t i = 0; i < state.AlphaCount; i++)
		{
			xkeys.emplace_back(state.Alphas[i].Position);
		}

		xkeys.emplace_back(0.0f);
		xkeys.emplace_back(1.0f);

		auto result = std::unique(xkeys.begin(), xkeys.end());
		xkeys.erase(result, xkeys.end());

		std::sort(xkeys.begin(), xkeys.end());

		for (size_t i = 0; i < xkeys.size() - 1; i++)
		{
			const auto c1 = state.GetCombinedColor(xkeys[i]);
			const auto c2 = state.GetCombinedColor(xkeys[i + 1]);

			const auto colorAU32 = ImGui::ColorConvertFloat4ToU32({c1[0], c1[1], c1[2], c1[3]});
			const auto colorBU32 = ImGui::ColorConvertFloat4ToU32({c2[0], c2[1], c2[2], c2[3]});

			drawList->AddRectFilledMultiColor(ImVec2(barOriginPos.x + xkeys[i] * width, barOriginPos.y),
											  ImVec2(barOriginPos.x + xkeys[i + 1] * width, barOriginPos.y + barHeight),
											  colorAU32,
											  colorBU32,
											  colorBU32,
											  colorAU32);
		}
	}

	if (isMarkerShown)
	{

		originPos = ImGui::GetCursorScreenPos();

		const auto resultColor = UpdateMarker(state.Colors, state.ColorCount, temporaryState, ImGradientHDRMarkerType::Color, "c", originPos, width, markerWidth, markerHeight, MarkerDirection::ToUpper);

		changed |= resultColor.isChanged;

		if (temporaryState.draggingMarkerType == ImGradientHDRMarkerType::Color)
		{
			SortMarkers(state.Colors, state.ColorCount, temporaryState.selectedIndex, temporaryState.draggingIndex);
		}

		ImGui::SetCursorScreenPos(originPos);

		ImGui::InvisibleButton("ColorArea", {width, static_cast<float>(markerHeight)});

		if (ImGui::IsItemHovered())
		{
			const float x = (ImGui::GetIO().MousePos.x - originPos.x);
			const float xn = x / width;
			const auto c = state.GetColorAndIntensity(xn);

			if (!resultColor.isHovered && state.ColorCount < state.Colors.size())
			{
				DrawMarker(
					{originPos.x + x - 5, originPos.y + 0},
					{originPos.x + x + 5, originPos.y + markerHeight},
					ImGui::ColorConvertFloat4ToU32({c[0], c[1], c[2], 0.5f}),
					DrawMarkerMode::None);
			}

			if (!resultColor.isHovered && ImGui::IsMouseClicked(0))
			{
				changed |= state.AddColorMarker(xn, {c[0], c[1], c[2]}, c[3]);
			}
		}
	}

	const auto lastOriginPos = ImGui::GetCursorScreenPos();

	ImGui::SetCursorScreenPos(barOriginPos);

	ImGui::Dummy({width, static_cast<float>(barHeight)});

	ImGui::SetCursorScreenPos(lastOriginPos);
	ImGui::Dummy({0.0f, 0.0f});

	ImGui::EndGroup();
	ImGui::PopID();

	return changed;
}
