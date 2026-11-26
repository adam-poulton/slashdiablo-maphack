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

	// Edits how one kind of marker is drawn: its icon, size and colour, and the
	// line to it. A group of controls rather than one control, laid out from the
	// group's corner, so it can be dropped into any panel and moved as one.
	//
	// It shows only what the options offer, and greys out what the style in hand
	// has no use for: an icon drawn in the game's own colours has no colour to
	// choose, a line that is off has none either.
	class MapIconhook : public HookGroup {
		private:
			HookGroup* parent;
			unsigned int x, y;
			bool active;

			MapIconStyle* style;
			MapIconOptions options;

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
		public:
			MapIconhook(HookGroup* parent, unsigned int x, unsigned int y,
				MapIconStyle* style, MapIconOptions options);
			~MapIconhook();

			// Brings the controls up to date with the style: which are greyed out,
			// and what the colour swatch is drawn as. Called every frame by whoever
			// shows the editor, before it is drawn.
			void Refresh();

			void SetActive(bool newActive) { active = newActive; };
			void SetBaseX(unsigned int newX) { x = newX; };
			void SetBaseY(unsigned int newY) { y = newY; };

			unsigned int GetX() { return parent->GetX() + x; };
			unsigned int GetY() { return parent->GetY() + y; };
			unsigned int GetXSize() { return xSize; };
			unsigned int GetYSize() { return ySize; };
			bool IsActive() { return active && parent->IsActive(); };
			HookVisibility GetVisibility() { return parent->GetVisibility(); };
	};
};
