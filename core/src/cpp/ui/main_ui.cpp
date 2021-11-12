//
// Created by Elec332 on 04/11/2021.
//

#include <ui/main_window.h>
#include <NativeSDRGraphics.h>

bool showEditor = false;

void drawTopRow() {
    ImGui::PushID(ImGui::GetID("editor_switch"));
    if (ImGui::Button(showEditor ? "Show Interface" : "Show Editor", ImVec2(100, 30))) {
        showEditor = !showEditor;
    }
    ImGui::PopID();
}

void drawmain(pipeline::schematic** nm) {
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("Content", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings |
                 ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImVec2 vMin = ImGui::GetWindowContentRegionMin();
    ImVec2 vMax = ImGui::GetWindowContentRegionMax();
    int width = vMax.x - vMin.x;
    int height = vMax.y - vMin.y;
    drawTopRow();

    if (showEditor) {
        editor_ui::draw(*nm);
    } else {
        sdr_ui::draw();
    }

    ImGui::End();
}

void main_window::init() {
    NativeGraphics::setupGraphics();
}

void main_window::deinit() {
    NativeGraphics::destroy();
}

void main_window::start(pipeline::schematic** nodes) {
    NativeGraphics::startMainWindow(drawmain, nodes);
}
