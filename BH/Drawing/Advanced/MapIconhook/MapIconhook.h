#pragma once
#include <vector>
#include "../../Hook.h"
#include "../Combohook/Combohook.h"
#include "../../MapIcon/MapIconStyle.h"

namespace Drawing {
	class Texthook;
	class Sliderhook;
	class Colorhook;
	class Checkhook;

	// The pitch the editor's rows are laid out on, the icon's row apart, how far
	// down a row its text sits, and the space between a label and the control it
	// names. Public so that whoever shows the editor can lay rows of its own under
	// it to the same rhythm.
	#define MAP_ICON_ROW_HEIGHT		22
	#define MAP_ICON_ROW_TEXT_TOP	6
	#define MAP_ICON_LABEL_GAP		10

	// The icons a style can be given, as a dropdown: each option is the icon,
	// drawn as the style draws it, and its name. Past a handful the list scrolls,
	// so it stays the size it is however many icons there come to be.
	//
	// For a style laid over another, the first option is to leave the icon as it
	// is, shown as the icon the base has.
	class MapIconCombohook : public Combohook {
		private:
			MapIconStyle* style;
			std::vector<MapIcon::Icon> icons;
			const MapIconOverride* partial;

			// How many options stand ahead of the icons.
			unsigned int Lead() const { return partial ? 1 : 0; };

			// Where the style's icon is among those offered, which is what the box
			// shows and the list picks; kept in step with the style as it draws.
			unsigned int index;

			static std::vector<std::string> Labels(const std::vector<MapIcon::Icon>& icons,
				bool unchanged);
			static unsigned int Width(const std::vector<MapIcon::Icon>& icons, bool unchanged);
			void DrawIcon(MapIcon::Icon shown, const MapIcon::Look& look, int cx, int cy);
		protected:
			void DrawOption(unsigned int option, unsigned int x, unsigned int y,
				unsigned int width, TextColor color);
			unsigned int GetRowHeight();
			void OnSelected(unsigned int option);
		public:
			MapIconCombohook(HookGroup* group, unsigned int x, unsigned int y,
				MapIconStyle* style, std::vector<MapIcon::Icon> icons,
				const MapIconOverride* partial = NULL);

			unsigned int GetYSize();
			unsigned int GetTextInset();
			void OnDraw();
	};

	// The style as it will look on the automap: the icon on a patch of map, with
	// its line running in to it from where the player might be. A shape is shown
	// at its real size and, beside that, magnified.
	class MapIconPreviewhook : public Hook {
		private:
			MapIconStyle* style;
			unsigned int xSize, ySize;

			void DrawBox(int left, int top, int width, unsigned int scale);
			void DrawPane(int left, int top, int width, int height, unsigned int scale);
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
			bool IsCoveredAt(unsigned int x, unsigned int y);
	};

	// Edits how one kind of marker is drawn: its icon and size, the colours of the
	// icon's fill and border and how wide the border is, and the line to it. One
	// hook holding its controls, so it can be dropped into any panel, moved and put
	// away as one, and is drawn by whatever draws the panel.
	//
	// The preview runs across the top, under its name, at whatever width the
	// editor is given, and the controls are a form under it, with labels in a
	// column of their own.
	//
	// It shows only what the options offer, and greys out what the style in hand
	// has no use for: an icon drawn in the game's own colours has no size or
	// colour to choose, a border of no colour has no width, a line that is off has
	// no colour either.
	//
	// Given an override, it edits a style laid over another: the icon can be left
	// as it is, the line is left, taken away or drawn, and the preview shows the
	// two together.
	class MapIconhook : public Hook {
		private:
			MapIconStyle* style;
			MapIconOptions options;
			MapIconOverride partial;
			bool overrides;
			MapIconStyle shown;		// what the preview draws
			MapIconContent* content;

			Texthook* iconLabel;
			MapIconCombohook* icon;
			Texthook* sizeLabel;
			Sliderhook* size;
			Colorhook* fill;
			Colorhook* border;
			Texthook* widthLabel;
			Sliderhook* width;
			Checkhook* line;
			Texthook* lineLabel;
			Combohook* lineChange;
			Colorhook* lineColor;
			Texthook* previewLabel;
			MapIconPreviewhook* preview;

			unsigned int xSize, ySize;
			unsigned int labelWidth;

			// The most the sliders' readouts take, so they can share one rail.
			unsigned int ReadoutRoom();

			void Build();
			void Layout();

			// Brings the controls up to date with the style: which are greyed out,
			// and what the preview shows.
			void Refresh();
		public:
			MapIconhook(HookGroup* group, unsigned int x, unsigned int y,
				MapIconStyle* style, MapIconOptions options);
			MapIconhook(HookGroup* group, unsigned int x, unsigned int y,
				MapIconStyle* style, MapIconOptions options, MapIconOverride partial);
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
