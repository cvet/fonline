#define IMGUI_DEFINE_MATH_OPERATORS 1

#include "efkMat.GradientEditor.h"

#include <ImGradientHDR.h>
#include <efkMat.Models.h>
#include <imgui.h>

#include <algorithm>
#include <string>
#include <unordered_map>

namespace EffekseerMaterial
{
namespace
{

std::unordered_map<ImGuiID, ImGradientHDRTemporaryState> temporaryStates;

void CopyToState(ImGradientHDRState& dst, const Gradient& src)
{
	dst.ColorCount = std::min(src.ColorCount, static_cast<int>(dst.Colors.size()));
	for (int i = 0; i < dst.ColorCount; i++)
	{
		dst.Colors[i].Color = src.Colors[i].Color;
		dst.Colors[i].Intensity = src.Colors[i].Intensity;
		dst.Colors[i].Position = src.Colors[i].Position;
	}

	dst.AlphaCount = std::min(src.AlphaCount, static_cast<int>(dst.Alphas.size()));
	for (int i = 0; i < dst.AlphaCount; i++)
	{
		dst.Alphas[i].Alpha = src.Alphas[i].Alpha;
		dst.Alphas[i].Position = src.Alphas[i].Position;
	}
}

void CopyFromState(Gradient& dst, const ImGradientHDRState& src)
{
	dst.ColorCount = std::min(src.ColorCount, static_cast<int>(dst.Colors.size()));
	for (int i = 0; i < dst.ColorCount; i++)
	{
		dst.Colors[i].Color = src.Colors[i].Color;
		dst.Colors[i].Intensity = src.Colors[i].Intensity;
		dst.Colors[i].Position = src.Colors[i].Position;
	}

	dst.AlphaCount = std::min(src.AlphaCount, static_cast<int>(dst.Alphas.size()));
	for (int i = 0; i < dst.AlphaCount; i++)
	{
		dst.Alphas[i].Alpha = src.Alphas[i].Alpha;
		dst.Alphas[i].Position = src.Alphas[i].Position;
	}
}

bool UpdateAlphaEditor(ImGradientHDRState& state, ImGradientHDRTemporaryState& temporaryState)
{
	bool changed = false;
	const auto selectedIndex = temporaryState.selectedIndex;
	const auto selected = selectedIndex >= 0 && temporaryState.selectedMarkerType == ImGradientHDRMarkerType::Alpha;

	ImGui::BeginDisabled(!selected);

	if (ImGui::BeginTable("##AlphaTable", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings))
	{
		ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed, ImGui::GetContentRegionAvail().x * 0.3f);
		ImGui::TableSetupColumn("Parameter");

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::TextUnformatted("Position");
		ImGui::TableNextColumn();

		auto selectedMarker = selected ? state.GetAlphaMarker(selectedIndex) : nullptr;
		float position = selectedMarker != nullptr ? selectedMarker->Position * 100.0f : 0.0f;
		const auto deleteWidth = 64.0f * ImGui::GetIO().FontGlobalScale;
		ImGui::SetNextItemWidth(std::max(1.0f, ImGui::GetContentRegionAvail().x - deleteWidth - ImGui::GetStyle().ItemSpacing.x));
		if (ImGui::DragFloat("##AlphaPosition", &position, 1.0f, 0.0f, 100.0f, "%.2f %%") && selectedMarker != nullptr)
		{
			selectedMarker->Position = position / 100.0f;
			changed = true;
		}

		ImGui::SameLine();
		if (ImGui::Button("Delete##Alpha", ImVec2(deleteWidth, 0.0f)) && selectedMarker != nullptr)
		{
			if (state.RemoveAlphaMarker(selectedIndex))
			{
				temporaryState = ImGradientHDRTemporaryState{};
				changed = true;
			}
		}

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::TextUnformatted("Alpha");
		ImGui::TableNextColumn();

		selectedMarker = selected ? state.GetAlphaMarker(selectedIndex) : nullptr;
		float alpha = selectedMarker != nullptr ? selectedMarker->Alpha : 0.0f;
		ImGui::SetNextItemWidth(-1.0f);
		if (ImGui::DragFloat("##AlphaValue", &alpha, 0.01f, 0.0f, 1.0f) && selectedMarker != nullptr)
		{
			selectedMarker->Alpha = alpha;
			changed = true;
		}

		ImGui::EndTable();
	}

	ImGui::EndDisabled();

	return changed;
}

bool UpdateColorEditor(ImGradientHDRState& state, ImGradientHDRTemporaryState& temporaryState)
{
	bool changed = false;
	const auto selectedIndex = temporaryState.selectedIndex;
	const auto selected = selectedIndex >= 0 && temporaryState.selectedMarkerType == ImGradientHDRMarkerType::Color;

	ImGui::BeginDisabled(!selected);

	if (ImGui::BeginTable("##ColorTable", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings))
	{
		ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed, ImGui::GetContentRegionAvail().x * 0.3f);
		ImGui::TableSetupColumn("Parameter");

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::TextUnformatted("Position");
		ImGui::TableNextColumn();

		auto selectedMarker = selected ? state.GetColorMarker(selectedIndex) : nullptr;
		float position = selectedMarker != nullptr ? selectedMarker->Position * 100.0f : 0.0f;
		const auto deleteWidth = 64.0f * ImGui::GetIO().FontGlobalScale;
		ImGui::SetNextItemWidth(std::max(1.0f, ImGui::GetContentRegionAvail().x - deleteWidth - ImGui::GetStyle().ItemSpacing.x));
		if (ImGui::DragFloat("##ColorPosition", &position, 1.0f, 0.0f, 100.0f, "%.2f %%") && selectedMarker != nullptr)
		{
			selectedMarker->Position = position / 100.0f;
			changed = true;
		}

		ImGui::SameLine();
		if (ImGui::Button("Delete##Color", ImVec2(deleteWidth, 0.0f)) && selectedMarker != nullptr)
		{
			if (state.RemoveColorMarker(selectedIndex))
			{
				temporaryState = ImGradientHDRTemporaryState{};
				changed = true;
			}
		}

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::TextUnformatted("Color");
		ImGui::TableNextColumn();

		selectedMarker = selected ? state.GetColorMarker(selectedIndex) : nullptr;
		std::array<float, 3> color = selectedMarker != nullptr ? selectedMarker->Color : std::array<float, 3>{0.0f, 0.0f, 0.0f};
		ImGui::SetNextItemWidth(-1.0f);
		if (ImGui::ColorEdit3("##ColorValue", color.data(), ImGuiColorEditFlags_Float) && selectedMarker != nullptr)
		{
			selectedMarker->Color = color;
			changed = true;
		}

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::TextUnformatted("Intensity");
		ImGui::TableNextColumn();

		selectedMarker = selected ? state.GetColorMarker(selectedIndex) : nullptr;
		float intensity = selectedMarker != nullptr ? selectedMarker->Intensity : 0.0f;
		ImGui::SetNextItemWidth(-1.0f);
		if (ImGui::DragFloat("##ColorIntensity", &intensity, 0.01f, 0.0f, 100.0f) && selectedMarker != nullptr)
		{
			selectedMarker->Intensity = intensity;
			changed = true;
		}

		ImGui::EndTable();
	}

	ImGui::EndDisabled();

	return changed;
}

} // namespace

bool UpdateGradientEditor(int32_t gradientID, const char* label, Gradient& gradient)
{
	bool changed = false;

	ImGui::PushID(gradientID);
	ImGui::PushID(label);

	const auto baseID = ImGui::GetID("GradientEditor");
	auto& previewTemporaryState = temporaryStates[ImGui::GetID("Preview")];
	auto& popupTemporaryState = temporaryStates[ImGui::GetID("Popup")];

	ImGradientHDRState state;
	CopyToState(state, gradient);

	ImGui::TextUnformatted(label);
	if (ImGradientHDR(static_cast<int32_t>(ImGui::GetID("PreviewGradient")), state, previewTemporaryState, false))
	{
		changed = true;
	}

	const auto popupID = std::string("Gradient Editor##") + std::to_string(baseID);
	if (ImGui::IsItemClicked(0))
	{
		ImGui::OpenPopup(popupID.c_str());
	}

	const auto windowPos = ImGui::GetWindowPos();
	const auto cursorY = ImGui::GetCursorScreenPos().y;
	const auto popupWidth = std::max(520.0f, ImGui::GetWindowSize().x);
	ImGui::SetNextWindowPos(ImVec2(windowPos.x, cursorY), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(popupWidth, 0.0f), ImGuiCond_Always);

	if (ImGui::BeginPopup(popupID.c_str(), ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_MenuBar))
	{
		if (ImGui::BeginMenuBar())
		{
			ImGui::TextUnformatted(label);
			ImGui::EndMenuBar();
		}

		if (UpdateAlphaEditor(state, popupTemporaryState))
		{
			changed = true;
		}

		if (ImGradientHDR(static_cast<int32_t>(ImGui::GetID("PopupGradient")), state, popupTemporaryState, true))
		{
			changed = true;
		}

		if (UpdateColorEditor(state, popupTemporaryState))
		{
			changed = true;
		}

		ImGui::EndPopup();
	}

	if (changed)
	{
		CopyFromState(gradient, state);
	}

	ImGui::PopID();
	ImGui::PopID();

	return changed;
}

} // namespace EffekseerMaterial
