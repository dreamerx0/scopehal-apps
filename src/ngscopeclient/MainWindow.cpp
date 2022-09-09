/***********************************************************************************************************************
*                                                                                                                      *
* glscopeclient                                                                                                        *
*                                                                                                                      *
* Copyright (c) 2012-2022 Andrew D. Zonenberg                                                                          *
* All rights reserved.                                                                                                 *
*                                                                                                                      *
* Redistribution and use in source and binary forms, with or without modification, are permitted provided that the     *
* following conditions are met:                                                                                        *
*                                                                                                                      *
*    * Redistributions of source code must retain the above copyright notice, this list of conditions, and the         *
*      following disclaimer.                                                                                           *
*                                                                                                                      *
*    * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the       *
*      following disclaimer in the documentation and/or other materials provided with the distribution.                *
*                                                                                                                      *
*    * Neither the name of the author nor the names of any contributors may be used to endorse or promote products     *
*      derived from this software without specific prior written permission.                                           *
*                                                                                                                      *
* THIS SOFTWARE IS PROVIDED BY THE AUTHORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED   *
* TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL *
* THE AUTHORS BE HELD LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES        *
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR       *
* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT *
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE       *
* POSSIBILITY OF SUCH DAMAGE.                                                                                          *
*                                                                                                                      *
***********************************************************************************************************************/

/**
	@file
	@author Andrew D. Zonenberg
	@brief Implementation of MainWindow
 */
#include "ngscopeclient.h"
#include "MainWindow.h"

//Dock builder API is not yet public, so might change...
#include "imgui_internal.h"

#include "AddScopeDialog.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

MainWindow::MainWindow(vk::raii::Queue& queue)
	: VulkanWindow("ngscopeclient", queue)
	, m_showDemo(true)
{
}

MainWindow::~MainWindow()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Rendering

void MainWindow::DoRender(vk::raii::CommandBuffer& /*cmdBuf*/)
{

}

void MainWindow::RenderUI()
{
	//Menu for main window
	MainMenu();

	//Docking area to put all of the groups in
	DockingArea();

	//Waveform groups
	WaveformGroups();

	//Dialog boxes
	set< shared_ptr<Dialog> > dlgsToClose;
	for(auto& dlg : m_dialogs)
	{
		if(!dlg->Render())
			dlgsToClose.emplace(dlg);
	}
	for(auto& dlg : dlgsToClose)
		m_dialogs.erase(dlg);

	//DEBUG: draw the demo window
	ImGui::ShowDemoWindow(&m_showDemo);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Top level menu

/**
	@brief Run the top level menu bar
 */
void MainWindow::MainMenu()
{
	if(ImGui::BeginMainMenuBar())
	{
		FileMenu();
		ViewMenu();
		AddMenu();
		HelpMenu();
		ImGui::EndMainMenuBar();
	}
}

/**
	@brief Run the File menu
 */
void MainWindow::FileMenu()
{
	if(ImGui::BeginMenu("File"))
	{
		if(ImGui::MenuItem("Exit"))
			glfwSetWindowShouldClose(m_window, 1);

		ImGui::EndMenu();
	}
}

/**
	@brief Run the View menu
 */
void MainWindow::ViewMenu()
{
	if(ImGui::BeginMenu("View"))
	{
		if(ImGui::MenuItem("Fullscreen"))
			SetFullscreen(!m_fullscreen);

		ImGui::EndMenu();
	}
}

/**
	@brief Run the Add menu
 */
void MainWindow::AddMenu()
{
	if(ImGui::BeginMenu("Add"))
	{
		AddOscilloscopeMenu();
		ImGui::EndMenu();
	}
}

/**
	@brief Run the Add | Oscilloscope menu
 */
void MainWindow::AddOscilloscopeMenu()
{
	if(ImGui::BeginMenu("Oscilloscope"))
	{
		if(ImGui::MenuItem("Connect..."))
			m_dialogs.emplace(make_shared<AddScopeDialog>(m_session));
		ImGui::Separator();

		//TODO: recent instruments

		ImGui::EndMenu();
	}
}

/**
	@brief Run the Help menu
 */
void MainWindow::HelpMenu()
{
	if(ImGui::BeginMenu("Help"))
	{
		ImGui::EndMenu();
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Waveform views etc

void MainWindow::DockingArea()
{
	//Provide a space we can dock windows into
	auto viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->WorkPos);
	ImGui::SetNextWindowSize(viewport->WorkSize);
	ImGui::SetNextWindowViewport(viewport->ID);

	ImGuiWindowFlags host_window_flags = 0;
	host_window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDocking;
	host_window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

	char label[32];
	ImFormatString(label, IM_ARRAYSIZE(label), "DockSpaceViewport_%08X", viewport->ID);

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::Begin(label, NULL, host_window_flags);
	ImGui::PopStyleVar(3);

	auto dockspace_id = ImGui::GetID("DockSpace");
	ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), /*dockspace_flags*/0, /*window_class*/nullptr);
	ImGui::End();

	static bool first = true;
	if(first)
	{
		ImGui::DockBuilderRemoveNode(dockspace_id);
		ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
		ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->WorkSize);

		ImGuiID idLeft;
		ImGuiID idRight;
		/*auto idParent =*/ ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.5, &idLeft, &idRight);
		ImGui::DockBuilderDockWindow("Waveform Group 1", idLeft);
		ImGui::DockBuilderDockWindow("Waveform Group 2", idRight);
		ImGui::DockBuilderFinish(dockspace_id);

		first = false;
	}
}

void MainWindow::WaveformGroups()
{
	bool open = true;
	ImGui::SetNextWindowSize(ImVec2(320, 240), ImGuiCond_Appearing);
	if(!ImGui::Begin("Waveform Group 1", &open))
		ImGui::End();
	ImVec2 clientArea = ImGui::GetContentRegionAvail();
	WaveformArea(2, "a", clientArea);
	WaveformArea(2, "b", clientArea);
	ImGui::End();

	ImGui::SetNextWindowSize(ImVec2(320, 240), ImGuiCond_Appearing);
	clientArea = ImGui::GetContentRegionAvail();
	if(!ImGui::Begin("Waveform Group 2", &open))
		ImGui::End();
	WaveformArea(1, "c", clientArea);
	ImGui::End();
}

void MainWindow::WaveformArea(int numAreas, const char* id, ImVec2 clientArea)
{
	auto height = (clientArea.y / numAreas) - ImGui::GetFrameHeightWithSpacing();
	if(ImGui::BeginChild(id, ImVec2(clientArea.x, height)))
	{
		auto csize = ImGui::GetContentRegionAvail();

		//Draw background texture
		ImTextureID my_tex_id = ImGui::GetIO().Fonts->TexID;
		ImGui::Image(my_tex_id, ImVec2(csize.x, csize.y),
			ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f));
		ImGui::SetItemAllowOverlap();

		//Draw control widgets
		ImGui::SetCursorPos(ImGui::GetWindowContentRegionMin());
		ImGui::BeginGroup();
			ImGui::Button("hai");
			ImGui::Button("asdf");
		ImGui::EndGroup();
		ImGui::SetItemAllowOverlap();
	}
	ImGui::EndChild();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Other GUI handlers