#pragma once

/*  This file is part of Imagine.

	Imagine is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	Imagine is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with EmuFramework.  If not, see <http://www.gnu.org/licenses/> */

#include <imagine/gfx/GfxText.hh>
#include <imagine/base/Timer.hh>
#include <imagine/gui/View.hh>
#include <cstdio>
#include <array>

namespace IG
{

class ToastView : public View
{
public:
	ToastView();
	ToastView(ViewAttachParams attach);
	void setFace(Gfx::GlyphTextureSet &face);
	void clear();
	void place() final;
	void post(IG::utf16String msg, int secs, bool error);
	void postError(IG::utf16String msg, int secs);
	void unpost();
	void prepareDraw() final;
	void draw(Gfx::RendererCommands &cmds) final;
	bool inputEvent(const Input::Event &) final;

private:
	Gfx::Text text{};
	Timer unpostTimer{Timer::NullInit{}};
	Gfx::GCRect msgFrame{};
	bool error = false;

	void postContent(int secs);
};

}
