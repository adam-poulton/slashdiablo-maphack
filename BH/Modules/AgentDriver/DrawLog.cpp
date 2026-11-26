#include "DrawLog.h"

#include "../../StringUtil.h"
#include "../../Drawing/Basic/Texthook/TextMetrics.h"

namespace Agent {

namespace {

bool recording = false;
std::vector<DrawRecord> records;

}  // namespace

nlohmann::json ToJson(const DrawRecord& record) {
	nlohmann::json json = {{"kind", record.kind}};
	if (record.kind != "frame")
		json["color"] = record.color;
	if (record.kind == "line") {
		json["from"] = {record.x, record.y};
		json["to"] = {record.x2, record.y2};
	} else {
		json["rect"] = {record.x, record.y, record.width, record.height};
	}
	if (record.font >= 0)
		json["font"] = record.font;
	if (record.trans >= 0)
		json["trans"] = record.trans;
	if (record.kind == "text")
		json["text"] = ToUtf8(record.text);
	return json;
}

namespace DrawLog {

bool Recording() {
	return recording;
}

void Begin() {
	records.clear();
	recording = true;
}

std::vector<DrawRecord> End() {
	recording = false;
	std::vector<DrawRecord> taken;
	taken.swap(records);
	return taken;
}

void Text(int x, int baseline, int width, int font, unsigned int color, const std::wstring& text) {
	if (!recording)
		return;
	DrawRecord record;
	record.kind = "text";
	record.height = (int)Drawing::BaselineOffset((unsigned int)font);
	record.x = x;
	record.y = baseline - record.height;
	record.width = width;
	record.font = font;
	record.color = color;
	record.text = text;
	records.push_back(std::move(record));
}

void Box(int x1, int y1, int x2, int y2, unsigned int color, int trans) {
	if (!recording)
		return;
	DrawRecord record;
	record.kind = "box";
	record.x = x1;
	record.y = y1;
	record.width = x2 - x1;
	record.height = y2 - y1;
	record.color = color;
	record.trans = trans;
	records.push_back(std::move(record));
}

void Line(int x1, int y1, int x2, int y2, unsigned int color) {
	if (!recording)
		return;
	DrawRecord record;
	record.kind = "line";
	record.x = x1;
	record.y = y1;
	record.x2 = x2;
	record.y2 = y2;
	record.color = color;
	records.push_back(std::move(record));
}

void Frame(int left, int top, int right, int bottom) {
	if (!recording)
		return;
	DrawRecord record;
	record.kind = "frame";
	record.x = left;
	record.y = top;
	record.width = right - left;
	record.height = bottom - top;
	records.push_back(std::move(record));
}

}  // namespace DrawLog

}
