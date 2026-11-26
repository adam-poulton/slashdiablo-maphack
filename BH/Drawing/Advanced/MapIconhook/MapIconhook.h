#pragma once
#include <vector>
#include "../../Hook.h"
#include "../../MapIcon/MapIconStyle.h"

namespace Drawing {
	class Texthook;
	class Sliderhook;
	class Colorhook;
	class Checkhook;

	// The icons a style can be given, side by side, each drawn in the style's own
	// colour; a click picks one.
	class MapIconStriphook : public Hook {
		private:
			MapIconStyle* style;
			std::vector<MapIcon::Icon> icons;

			// The position in the strip under a point, or -1 for none.
			int CellAt(unsigned int x, unsigned int y);
		public:
			MapIconStriphook(HookGroup* group, unsigned int x, unsigned int y,
				MapIconStyle* style, std::vector<MapIcon::Icon> icons);

			bool OnLeftClick(bool up, unsigned int x, unsigned int y);
			void OnDraw();

			unsigned int GetXSize();
			unsigned int GetYSize();
	};

	// The style as it will look on the automap: the icon at its real size on a
	// patch of map, with its line running in to it from the corner.
	class MapIconPreviewhook : public Hook {
		private:
			MapIconStyle* style;
		public:
			MapIconPreviewhook(HookGroup* group, unsigned int x, unsigned int y,
				MapIconStyle* style);

			void OnDraw();

			unsigned int GetXSize();
			unsigned int GetYSize();
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

			void Layout();

			// Brings the controls up to date with the style: which are greyed out,
			// and what the colour swatch is drawn as.
			void Refresh();
		public:
			MapIconhook(HookGroup* group, unsigned int x, unsigned int y,
				MapIconStyle* style, MapIconOptions options);
			~MapIconhook();

			unsigned int GetXSize() { return xSize; };
			unsigned int GetYSize() { return ySize; };

			void OnDraw();
	};
};
