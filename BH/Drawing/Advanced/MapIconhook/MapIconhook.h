#pragma once
#include <vector>
#include "../../Hook.h"
#include "../../MapIcon/MapIconStyle.h"

namespace Drawing {
	class Texthook;
	class Sliderhook;
	class Colorhook;
	class Checkhook;

	// The pitch every row of the editor is laid out on, how far down a row its
	// text sits, and the space between a label and the control it names. Public
	// so that whoever shows the editor can lay rows of its own on the same lines:
	// next to it, and under it.
	#define MAP_ICON_ROW_HEIGHT		22
	#define MAP_ICON_ROW_TEXT_TOP	6
	#define MAP_ICON_LABEL_GAP		10

	// The icons a style can be given, side by side, each drawn in the style's own
	// colour; a click picks one. The name of the icon under the pointer, or else
	// of the one chosen, is written after them, as a slider writes its value.
	class MapIconStriphook : public Hook {
		private:
			MapIconStyle* style;
			std::vector<MapIcon::Icon> icons;
			unsigned int readoutWidth;	// the widest name, measured once

			// The position in the strip under a point, or -1 for none.
			int CellAt(unsigned int x, unsigned int y);
			unsigned int CellsWidth();
		public:
			MapIconStriphook(HookGroup* group, unsigned int x, unsigned int y,
				MapIconStyle* style, std::vector<MapIcon::Icon> icons);

			bool OnLeftClick(bool up, unsigned int x, unsigned int y);
			void OnDraw();

			unsigned int GetXSize();
			unsigned int GetYSize();
			unsigned int GetTextInset();
	};

	// The style as it will look on the automap: the icon at its real size on a
	// patch of map, with its line running in to it from where the player might be.
	class MapIconPreviewhook : public Hook {
		private:
			MapIconStyle* style;
			unsigned int xSize, ySize;
		public:
			MapIconPreviewhook(HookGroup* group, unsigned int x, unsigned int y,
				MapIconStyle* style);

			void SetSize(unsigned int width, unsigned int height);
			void OnDraw();

			unsigned int GetXSize() { return xSize; };
			unsigned int GetYSize() { return ySize; };
	};

	class MapIconhook;

	// The corner the editor's controls are laid out from, which is the editor's
	// own. A group of its own rather than the editor itself, as ScrollContent is:
	// a Hook and a HookGroup both answer to GetX and GetY.
	class MapIconContent : public HookGroup {
		private:
			MapIconhook* editor;
		public:
			MapIconContent(MapIconhook* owner) : editor(owner) {};

			unsigned int GetX();
			unsigned int GetY();
			unsigned int GetXSize();
			unsigned int GetYSize();
			bool IsActive();
			HookVisibility GetVisibility();
	};

	// Edits how one kind of marker is drawn: its icon, size and colour, and the
	// line to it. One hook holding its controls, so it can be dropped into any
	// panel, moved and put away as one, and is drawn by whatever draws the panel.
	//
	// The preview runs across the top at whatever width the editor is given, and
	// the controls are a form under it: labels in a column of their own, every row
	// on the same pitch.
	//
	// It shows only what the options offer, and greys out what the style in hand
	// has no use for: an icon drawn in the game's own colours has no colour to
	// choose, a line that is off has none either.
	class MapIconhook : public Hook {
		private:
			MapIconStyle* style;
			MapIconOptions options;
			MapIconContent* content;

			Texthook* iconLabel;
			MapIconStriphook* strip;
			Texthook* sizeLabel;
			Sliderhook* size;
			Colorhook* color;
			Checkhook* line;
			Colorhook* lineColor;
			MapIconPreviewhook* preview;

			unsigned int xSize, ySize;
			unsigned int labelWidth;

			void Layout();

			// Brings the controls up to date with the style: which are greyed out,
			// and what the colour swatch is drawn as.
			void Refresh();
		public:
			MapIconhook(HookGroup* group, unsigned int x, unsigned int y,
				MapIconStyle* style, MapIconOptions options);
			~MapIconhook();

			// The width the editor is laid out to. The preview takes all of it;
			// the controls take what they need of it.
			unsigned int GetXSize() { return xSize; };
			void SetXSize(unsigned int width);
			unsigned int GetYSize() { return ySize; };

			// The narrowest the controls fit in, label column included.
			unsigned int GetMinXSize();

			// The column the labels are written in, ahead of the controls they name.
			// The editor measures what its own labels need; a panel with labelled
			// rows of its own under the editor widens it, so every control starts at
			// the same place.
			unsigned int GetNaturalLabelWidth();
			unsigned int GetLabelWidth() { return labelWidth; };
			void SetLabelWidth(unsigned int width);

			// Where a hook goes in a row that starts at rowTop, so its text sits on
			// the row's line whatever the hook draws around it.
			static unsigned int RowY(unsigned int rowTop, Hook* hook);

			void OnDraw();
	};
};
