/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */


#include "GameInfo.h"
#include "MouseHandler.h"
#include "Game/GameSetup.h"
#include "Game/GameVersion.h"
#include "Map/MapInfo.h"
#include "Map/ReadMap.h"
#include "Rendering/GL/myGL.h"
#include "Rendering/GL/glExtra.h"
#include "Rendering/Fonts/glFont.h"
#include "Sim/Misc/Team.h"
#include "Sim/Misc/Wind.h"
#include "Sim/Misc/ModInfo.h"
#include "Sim/Path/IPathManager.h"
#include "System/FileSystem/FileSystem.h"
#include "System/StringUtil.h"

#include <SDL_keycode.h>
#include <cstdio>

using std::string;
using std::vector;



static const char* boolString(bool value)
{
	return (value)? "True": "False";
}

static const char* floatString(float value)
{
	static char buf[16];
	sprintf(buf, "%8.3f", value);
	return buf;
}

static void StringListStats(
	const vector<CGameInfo::FontString>& list,
	float& maxWidth,
	float& maxHeight
) {
	maxWidth = 0.0f;
	maxHeight = 0.0f;

	for (int i = 0; i < (int)list.size(); i++) {
		const CGameInfo::FontString& fs = list[i];

		maxWidth = std::max(maxWidth, fs.width);
		maxHeight = std::max(maxHeight, fs.height);
	}
}



static CGameInfo* instance = nullptr;

void CGameInfo::Enable()
{
	if (instance == nullptr)
		instance = new CGameInfo;
}

void CGameInfo::Disable()
{
	delete instance;
	instance = nullptr;
}

bool CGameInfo::IsActive()
{
	return (instance != nullptr);
}



CGameInfo::CGameInfo()
{
	box.x1 = 0.5f;
	box.y1 = 0.5f;
	box.x2 = 0.5f;
	box.y2 = 0.5f;

	labels.reserve(16);
	values.reserve(16);

	char buf[1024];

	if (gameSetup != nullptr && gameSetup->hostDemo) {
		labels.push_back("Playback:");
		values.push_back(FileSystem::GetBasename(gameSetup->demoName));
	}

	labels.push_back("Spring Version:");
	values.push_back(SpringVersion::GetFull());

	labels.push_back("Game Speed:");
	values.push_back(gs->speedFactor);

	labels.push_back("Map Gravity:");
	sprintf(buf, "%.2f (%.2f e/f^2)", -(mapInfo->map.gravity * GAME_SPEED * GAME_SPEED), -mapInfo->map.gravity);
	values.push_back(buf);

	labels.push_back("Map Hardness:");
	sprintf(buf, "%.2f", mapInfo->map.hardness);
	values.push_back(buf);

	labels.push_back("Map Tidal:");
	values.push_back(mapInfo->map.tidalStrength);

	labels.push_back("Map Wind:");
	sprintf(buf, "%.2f - %.2f (%.2f)", wind.GetMinWind(), wind.GetMaxWind(), (wind.GetMinWind() + wind.GetMaxWind()) * 0.5f);
	values.push_back(buf);

	labels.push_back("Map Size:");
	sprintf(buf, "%ix%i", mapDims.mapx / 64, mapDims.mapy / 64);
	values.push_back(buf);

	labels.push_back("Map Name:");
	values.push_back(gameSetup->mapName);

	labels.push_back("Game Name:");
	values.push_back(gameSetup->modName);

	labels.push_back("PFS Name:");
	switch (pathManager->GetPathFinderType()) {
		case PFS_TYPE_DEFAULT: { values.push_back("Default"); } break;
		case PFS_TYPE_QTPFS:   { values.push_back("QTPFS"  ); } break;
		default:               { values.push_back("UNKNOWN"); } break; // not reachable
	}

	labels.push_back("CHEATS:");
	values.push_back(""); // dynamic, set in Draw
}

CGameInfo::~CGameInfo()
{
	instance = NULL;
}



CGameInfo::FontString::FontString(const char* c): msg(c)        { CalcDimensions(); }
CGameInfo::FontString::FontString(const std::string& s): msg(s) { CalcDimensions(); }
CGameInfo::FontString::FontString(bool b): msg(boolString(b))   { CalcDimensions(); }
CGameInfo::FontString::FontString(float f): msg(floatString(f)) { CalcDimensions(); }

void CGameInfo::FontString::CalcDimensions() {
	width  = font->GetSize() * font->GetTextWidth(msg) * globalRendering->pixelX;
	height = font->GetSize() * font->GetLineHeight() * globalRendering->pixelY;
}



/******************************************************************************/

std::string CGameInfo::GetTooltip(int x,int y)
{
	return "Game Information";
}


bool CGameInfo::IsAbove(int x, int y)
{
	const float mx = MouseX(x);
	const float my = MouseY(y);
	return InBox(mx, my, box);
}


bool CGameInfo::MousePress(int x, int y, int button)
{
	return (IsAbove(x, y));
}


void CGameInfo::MouseRelease(int x, int y, int button)
{
	if (activeReceiver != this)
		return;

	if (!IsAbove(x, y))
	return;

	delete this;
}


bool CGameInfo::KeyPressed(int key, bool isRepeat)
{
	if (key == SDLK_ESCAPE) {
		delete this;
		return true;
	}
	return false;
}


void CGameInfo::Draw()
{
	if (gs->cheatEnabled) {
		values[values.size() - 1] = "ENABLED";
	} else {
		values[values.size() - 1] = "DISABLED";
	}

	GL::RenderDataBufferC* buffer = GL::GetRenderBufferC();
	Shader::IProgramObject* shader = buffer->GetShader();


	// in screen fractions
	const float   split = 10.0f * globalRendering->pixelX;
	const float xBorder =  5.0f * globalRendering->pixelX;
	const float yBorder =  5.0f * globalRendering->pixelY;

	float labelsWidth, labelsHeight;
	float valuesWidth, valuesHeight;
	StringListStats(labels, labelsWidth, labelsHeight);
	StringListStats(values, valuesWidth, valuesHeight);

	const float    width = labelsWidth + valuesWidth + split + 2.0f * xBorder;
	const float rowHeight = std::max(labelsHeight, valuesHeight) + 2.0f * yBorder;
	const float    height = rowHeight * labels.size();

	const float dy = height / labels.size();

	box.x1 = 0.5f - ( width * 0.5f);
	box.x2 = 0.5f + ( width * 0.5f);
	box.y1 = 0.5f - (height * 0.5f);
	box.y2 = 0.5f + (height * 0.5f);


	glEnable(GL_BLEND);
	glDisable(GL_ALPHA_TEST);

	// draw the boxes
	for (size_t i = 0; i < labels.size(); i++) {
		constexpr float colors[][4]  = {
			{0.6f, 0.3f, 0.3f, 0.8f}, // red
			{0.3f, 0.6f, 0.3f, 0.8f}, // green
			{0.3f, 0.3f, 0.6f, 0.8f}, // blue
		};

		TRectangle<float> b;
		b.x1 = box.x1;
		b.x2 = box.x2;
		b.y2 = box.y2 - (dy * i);
		b.y1 =   b.y2 -  dy;

		gleDrawQuadC(b, SColor(colors[i % 3]), buffer);
	}
	{
		shader->Enable();
		shader->SetUniformMatrix4x4<const char*, float>("u_movi_mat", false, CMatrix44f::Identity());
		shader->SetUniformMatrix4x4<const char*, float>("u_proj_mat", false, CMatrix44f::ClipOrthoProj01(globalRendering->supportClipSpaceControl * 1.0f));
		buffer->Submit(GL_QUADS);
		shader->Disable();
	}


	font->SetTextColor(1.0f, 1.0f, 1.0f, 1.0f);

	// draw the strings
	for (size_t i = 0; i < labels.size(); i++) {
		const FontString& lfs = labels[i];
		const FontString& vfs = values[i];

		const float y = box.y2 - (dy * (i + 1)) + yBorder;

		font->glPrint(box.x1 + xBorder, y, 1.0f, FONT_SCALE | FONT_NORM | FONT_BUFFERED, lfs.msg);
		font->glPrint(box.x2 - xBorder, y, 1.0f, FONT_RIGHT | FONT_SCALE | FONT_NORM | FONT_BUFFERED, vfs.msg);
	}

	font->DrawBufferedGL4();
}
