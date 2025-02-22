/***********************************************************************************************************************
*                                                                                                                      *
* glscopeclient                                                                                                        *
*                                                                                                                      *
* Copyright (c) 2012-2023 Andrew D. Zonenberg                                                                          *
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
	@brief Implementation of FilterPropertiesDialog
 */
#include "ngscopeclient.h"
#include "ChannelPropertiesDialog.h"
#include "FilterPropertiesDialog.h"
#include "MainWindow.h"
#include "IGFDFileBrowser.h"
#include "NFDFileBrowser.h"
#include "../scopehal/ActionProvider.h"
#include "../scopeprotocols/TouchstoneImportFilter.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constuction / destruction

FilterPropertiesDialog::FilterPropertiesDialog(Filter* f, MainWindow* parent, bool graphEditorMode)
	: ChannelPropertiesDialog(f, graphEditorMode)
	, m_parent(parent)
{

}

FilterPropertiesDialog::~FilterPropertiesDialog()
{

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Main GUI

bool FilterPropertiesDialog::Render()
{
	RunFileDialog();
	return Dialog::Render();
}

/**
	@brief Spawns the file dialog if it's an import filter
 */
void FilterPropertiesDialog::SpawnFileDialogForImportFilter()
{
	//If the filter is an import filter, show the import dialog
	auto f = dynamic_cast<ImportFilter*>(m_channel);
	if(f)
	{
		auto name = f->GetFileNameParameter();
		auto& param = f->GetParameter(name);

		m_fileDialog = MakeFileBrowser(
			m_parent,
			param.GetFileName(),
			"Select File",
			param.m_fileFilterName,
			param.m_fileFilterMask,
			param.m_fileIsOutput);
		m_fileParamName = name;
	}

	//Special case: TouchstoneImportFilter should be treated as an import filter but is not derived
	//from ImportFilter because it's a SParameterSourceFilter
	auto t = dynamic_cast<TouchstoneImportFilter*>(m_channel);
	if(t)
	{
		auto name = t->GetFileNameParameter();
		auto& param = t->GetParameter(name);

		m_fileDialog = MakeFileBrowser(
			m_parent,
			param.GetFileName(),
			"Select File",
			param.m_fileFilterName,
			param.m_fileFilterMask,
			param.m_fileIsOutput);
		m_fileParamName = name;
	}
}

void FilterPropertiesDialog::RunFileDialog()
{
	//Run file browser dialog
	if(m_fileDialog)
	{
		m_fileDialog->Render();

		if(m_fileDialog->IsClosedOK())
		{
			auto f = dynamic_cast<Filter*>(m_channel);
			auto oldStreamCount = f->GetStreamCount();
			f->GetParameter(m_fileParamName).SetFileName(m_fileDialog->GetFileName());
			m_paramTempValues.erase(m_fileParamName);

			OnReconfigured(f, oldStreamCount);
		}

		if(m_fileDialog->IsClosed())
			m_fileDialog = nullptr;
	}
}

//TODO: some of this code needs to be shared by the trigger dialog

bool FilterPropertiesDialog::DoRender()
{
	//Flags for a header that should be open by default EXCEPT in the graph editor
	ImGuiTreeNodeFlags defaultOpenFlags = m_graphEditorMode ? 0 : ImGuiTreeNodeFlags_DefaultOpen;

	//Update name as we go
	m_title = m_channel->GetHwname();

	if(!ChannelPropertiesDialog::DoRender())
		return false;

	auto f = dynamic_cast<Filter*>(m_channel);

	bool reconfigured = false;

	auto oldStreamCount = m_channel->GetStreamCount();

	//Show inputs (if we have any)
	if( (f->GetInputCount() != 0) && !m_graphEditorMode)
	{
		if(ImGui::CollapsingHeader("Inputs", ImGuiTreeNodeFlags_DefaultOpen))
		{
			//TODO: cache some of this?
			vector<StreamDescriptor> streams;
			FindAllStreams(streams);

			for(size_t i=0; i<f->GetInputCount(); i++)
			{
				//Find the set of legal streams for this input
				vector<StreamDescriptor> matchingInputs;
				vector<string> names;
				int sel = -1;
				for(auto stream : streams)
				{
					if(!f->ValidateChannel(i, stream))
						continue;

					if(f->GetInput(i) == stream)
						sel = matchingInputs.size();

					matchingInputs.push_back(stream);
					names.push_back(stream.GetName());
				}

				//The actual combo box
				if(Combo(f->GetInputName(i).c_str(), names, sel))
				{
					f->SetInput(i, matchingInputs[sel]);
					reconfigured = true;
				}
			}
		}
	}

	//Show parameters (if we have any)
	if(f->GetParamCount() != 0)
	{
		if(ImGui::CollapsingHeader("Parameters", defaultOpenFlags))
		{
			for(auto it = f->GetParamBegin(); it != f->GetParamEnd(); it++)
			{
				//This can never be used in a trigger so special case
				if(it->second.GetType() == FilterParameter::TYPE_FILENAME)
				{
					string name = it->first;
					auto& param = it->second;

					string s = param.GetFileName();
					if(m_paramTempValues.find(name) == m_paramTempValues.end())
						m_paramTempValues[name] = s;

					//Input path
					ImGui::SetNextItemWidth(ImGui::GetFontSize() * 10);
					string tname = string("###path") + name;
					if(TextInputWithImplicitApply(tname.c_str(), m_paramTempValues[name], s))
					{
						param.SetStringVal(s);
						reconfigured = true;
					}

					//Browser button
					ImGui::SameLine();
					string bname = string("...###browse") + name;
					if(ImGui::Button(bname.c_str()))
					{
						if(!m_fileDialog)
						{
							m_fileDialog = MakeFileBrowser(
								m_parent,
								s,
								"Select File",
								param.m_fileFilterName,
								param.m_fileFilterMask,
								param.m_fileIsOutput);
							m_fileParamName = name;
						}
						else
							LogTrace("file dialog is already open, ignoring additional button click\n");
					}
					ImGui::SameLine();
					ImGui::TextUnformatted(name.c_str());
				}

				else if(DoParameter(it->second, it->first, m_paramTempValues))
					reconfigured = true;
			}
		}
	}

	//Show actions (if we have any)
	auto ap = dynamic_cast<ActionProvider*>(f);
	if(ap)
	{
		if(ImGui::CollapsingHeader("Actions", defaultOpenFlags))
		{
			auto actions = ap->EnumActions();
			for(auto a : actions)
			{
				if(ImGui::Button(a.c_str()))
				{
					//Assume that the action requires the filter to get re-rendered
					ap->PerformAction(a);
					reconfigured = true;
				}
			}
		}
	}

	if(reconfigured)
		OnReconfigured(f, oldStreamCount);

	return true;
}

/**
	@brief Handle a single parameter row in the filter (or trigger) properties dialog

	@return True if a change was made
 */
bool FilterPropertiesDialog::DoParameter(FilterParameter& param, string name, map<string, string>& tempValues)
{
	//See what kind of parameter it is
	switch(param.GetType())
	{
		case FilterParameter::TYPE_FLOAT:
			{
				//If we don't have a temporary value, make one
				auto nval = param.GetFloatVal();
				if(tempValues.find(name) == tempValues.end())
					tempValues[name] = param.GetUnit().PrettyPrint(nval);

				//Input path
				ImGui::SetNextItemWidth(ImGui::GetFontSize() * 8);
				if(Dialog::UnitInputWithImplicitApply(name.c_str(), tempValues[name], nval, param.GetUnit()))
				{
					param.SetFloatVal(nval);
					return true;
				}
			}
			break;

		case FilterParameter::TYPE_INT:
			{
				//If we don't have a temporary value, make one
				//TODO: can we figure out how to preserve full int64 precision end to end here?
				//For now, use a double to get as close as we can
				double nval = param.GetIntVal();
				if(tempValues.find(name) == tempValues.end())
					tempValues[name] = param.GetUnit().PrettyPrint(nval);

				//Input path
				ImGui::SetNextItemWidth(ImGui::GetFontSize() * 8);
				if(Dialog::UnitInputWithImplicitApply(name.c_str(), tempValues[name], nval, param.GetUnit()))
				{
					param.SetIntVal(nval);
					return true;
				}
			}
			break;

		case FilterParameter::TYPE_BOOL:
			{
				bool b = param.GetBoolVal();
				if(ImGui::Checkbox(name.c_str(), &b))
				{
					param.SetBoolVal(b);
					return true;
				}
			}
			break;

		case FilterParameter::TYPE_STRING:
			{
				//If we don't have a temporary value, make one
				string s = param.ToString();
				if(tempValues.find(name) == tempValues.end())
					tempValues[name] = s;

				//Input path
				ImGui::SetNextItemWidth(ImGui::GetFontSize() * 8);
				if(Dialog::TextInputWithImplicitApply(name.c_str(), tempValues[name], s))
				{
					param.SetStringVal(s);
					return true;
				}
			}
			break;

		case FilterParameter::TYPE_ENUM:
			{
				vector<string> enumValues;
				param.GetEnumValues(enumValues);

				int nsel = -1;
				string s = param.ToString();
				for(size_t i=0; i<enumValues.size(); i++)
				{
					if(enumValues[i] == s)
					{
						nsel = i;
						break;
					}
				}

				if(Dialog::Combo(name.c_str(), enumValues, nsel))
				{
					param.ParseString(enumValues[nsel]);
					return true;
				}
			}
			break;

		//TODO: TYPE_8B10B_PATTERN

		default:
			ImGui::Text("Parameter %s is unimplemented type", name.c_str());
			break;
	}

	//if we get here, no change made
	return false;
}

/**
	@brief Handle a filter being reconfigured
 */
void FilterPropertiesDialog::OnReconfigured(Filter* f, size_t oldStreamCount)
{
	//Update auto generated name
	if(f->IsUsingDefaultName())
	{
		f->SetDefaultName();
		m_committedDisplayName = f->GetDisplayName();
		m_displayName = m_committedDisplayName;
	}

	m_parent->OnFilterReconfigured(f);

	//If we have more streams than before, add views for them
	//(this is typically the case if we added a filename to a new import filter)
	auto newStreamCount = m_channel->GetStreamCount();
	if(oldStreamCount < newStreamCount)
	{
		for(size_t i=oldStreamCount; i < newStreamCount; i++)
			m_parent->FindAreaForStream(nullptr, StreamDescriptor(m_channel, i));
	}
}

/**
	@brief Get every stream that might be usable as an input to a filter
 */
void FilterPropertiesDialog::FindAllStreams(vector<StreamDescriptor>& streams)
{
	//Null stream always has to be considered
	streams.push_back(StreamDescriptor(nullptr, 0));

	//Then find every channel of every scope
	auto& scopes = m_parent->GetSession().GetScopes();
	for(auto scope : scopes)
	{
		for(size_t i=0; i<scope->GetChannelCount(); i++)
		{
			auto chan = scope->GetOscilloscopeChannel(i);
			if(!chan)
				continue;

			if(scope->CanEnableChannel(i))
			{
				for(size_t j=0; j<chan->GetStreamCount(); j++)
					streams.push_back(StreamDescriptor(chan, j));
			}
		}
	}

	//Then add every stream of every filter
	auto filters = Filter::GetAllInstances();
	for(auto f : filters)
	{
		for(size_t j=0; j<f->GetStreamCount(); j++)
			streams.push_back(StreamDescriptor(f, j));
	}
}
