#include "ItemFactsPacket.h"
#include <stdexcept>
#include <string>
#include "../../BitReader.h"
#include "../../Constants.h"
#include "../../ItemTables.h"
#include "ItemFacts.h"

namespace ItemFactsPacket {

const StatProperties& Reader::Widths(unsigned int stat) const {
	StatProperties *widths = tables.Stat(stat);
	if (!widths) {
		// Throwing rather than guessing: a width that is not the game's own puts
		// every field after it in the wrong place, and reading gives out here.
		throw std::out_of_range("no widths for stat");
	}
	return *widths;
}

bool Reader::Read(const unsigned char *data, ItemFacts *item) const {
	bool success = true;
	try {
		BitReader reader(data);
		unsigned long packet = reader.read(8);
		item->action = reader.read(8);
		unsigned long messageSize = reader.read(8);
		item->category = reader.read(8); // item type
		item->id = reader.read(32);

		if (packet == 0x9d) {
			reader.read(32);
			reader.read(8);
		}

		item->equipped = reader.readBool();
		reader.readBool();
		reader.readBool();
		item->inSocket = reader.readBool();
		item->identified = reader.readBool();
		reader.readBool();
		item->switchedIn = reader.readBool();
		item->switchedOut = reader.readBool();

		item->broken = reader.readBool();
		reader.readBool();
		item->potion = reader.readBool();
		item->hasSockets = reader.readBool();
		reader.readBool();
		item->inStore = reader.readBool();
		item->notInSocket = reader.readBool();
		reader.readBool();

		item->ear = reader.readBool();
		item->startItem = reader.readBool();
		reader.readBool();
		reader.readBool();
		reader.readBool();
		item->simpleItem = reader.readBool();
		item->ethereal = reader.readBool();
		reader.readBool();

		item->personalized = reader.readBool();
		item->gambling = reader.readBool();
		item->runeword = reader.readBool();
		reader.read(5);

		item->version = static_cast<unsigned int>(reader.read(8));

		reader.read(2);
		unsigned long destination = reader.read(3);

		item->ground = (destination == 0x03);

		if (item->ground) {
			item->x = reader.read(16);
			item->y = reader.read(16);
		} else {
			item->directory = reader.read(4);
			item->x = reader.read(4);
			item->y = reader.read(3);
			item->container = static_cast<unsigned int>(reader.read(4));
		}

		item->unspecifiedDirectory = false;

		if (item->action == ITEM_ACTION_TO_STORE || item->action == ITEM_ACTION_FROM_STORE) {
			long container = static_cast<long>(item->container);
			container |= 0x80;
			if (container & 1) {
				container--; //remove first bit
				item->y += 8;
			}
			item->container = static_cast<unsigned int>(container);
		} else if (item->container == CONTAINER_UNSPECIFIED) {
			if (item->directory == EQUIP_NONE) {
				if (item->inSocket) {
					//y is ignored for this container type, x tells you the index
					item->container = CONTAINER_ITEM;
				} else if (item->action == ITEM_ACTION_PLACE_BELT || item->action == ITEM_ACTION_REMOVE_BELT) {
					item->container = CONTAINER_BELT;
					item->y = item->x / 4;
					item->x %= 4;
				}
			} else {
				item->unspecifiedDirectory = true;
			}
		}

		if (item->ear) {
			item->earClass = static_cast<unsigned char>(reader.read(3));
			item->earLevel = reader.read(7);
			item->code[0] = 'e';
			item->code[1] = 'a';
			item->code[2] = 'r';
			item->code[3] = 0;
			for (std::size_t i = 0; i < 16; i++) {
				char letter = static_cast<char>(reader.read(7));
				if (letter == 0) {
					break;
				}
				item->earName.push_back(letter);
			}
			item->attrs = tables.Attributes(item->code);
			item->name = item->attrs->name;
			item->width = item->attrs->width;
			item->height = item->attrs->height;
			//PrintText(1, "Ear packet: %s, %s, %d, %d", item->earName.c_str(), item->code, item->earClass, item->earLevel);
			return success;
		}

		for (std::size_t i = 0; i < 4; i++) {
			item->code[i] = static_cast<char>(reader.read(8));
		}
		item->code[3] = 0;

		if (tables.Attributes(item->code) == NULL) {
			diagnostics.UnknownItemCode(item->code);
			success = false;
			return success;
		}
		item->attrs = tables.Attributes(item->code);
		item->name = item->attrs->name;
		item->width = item->attrs->width;
		item->height = item->attrs->height;

		item->isGold = (item->code[0] == 'g' && item->code[1] == 'l' && item->code[2] == 'd');

		if (item->isGold) {
			bool big_pile = reader.readBool();
			if (big_pile) {
				item->amount = reader.read(32);
			} else {
				item->amount = reader.read(12);
			}
			return success;
		}

		item->usedSockets = (unsigned char)reader.read(3);

		if (item->simpleItem || item->gambling) {
			return success;
		}

		item->level = (unsigned char)reader.read(7);
		item->quality = static_cast<unsigned int>(reader.read(4));

		item->hasGraphic = reader.readBool();;
		if (item->hasGraphic) {
			item->graphic = reader.read(3);
		}

		item->hasColor = reader.readBool();
		if (item->hasColor) {
			item->color = reader.read(11);
		}

		if (item->identified) {
			switch(item->quality) {
			case ITEM_QUALITY_INFERIOR:
				item->prefix = reader.read(3);
				break;

			case ITEM_QUALITY_SUPERIOR:
				item->superiority = static_cast<unsigned int>(reader.read(3));
				break;

			case ITEM_QUALITY_MAGIC:
				item->prefix = reader.read(11);
				item->suffix = reader.read(11);
				break;

			case ITEM_QUALITY_CRAFT:
			case ITEM_QUALITY_RARE:
				item->prefix = reader.read(8) - 156;
				item->suffix = reader.read(8) - 1;
				break;

			case ITEM_QUALITY_SET:
				item->setCode = reader.read(12);
				break;

			case ITEM_QUALITY_UNIQUE:
				if (item->code[0] != 's' || item->code[1] != 't' || item->code[2] != 'd') { //standard of heroes exception?
					item->uniqueCode = reader.read(12);
				}
				break;
			}
		}

		if (item->quality == ITEM_QUALITY_RARE || item->quality == ITEM_QUALITY_CRAFT) {
			for (unsigned long i = 0; i < 3; i++) {
				if (reader.readBool()) {
					item->prefixes.push_back(reader.read(11));
				}
				if (reader.readBool()) {
					item->suffixes.push_back(reader.read(11));
				}
			}
		}

		if (item->runeword) {
			item->runewordId = reader.read(12);
			item->runewordParameter = reader.read(4);
		}

		if (item->personalized) {
			for (std::size_t i = 0; i < 16; i++) {
				char letter = static_cast<char>(reader.read(7));
				if (letter == 0) {
					break;
				}
				item->personalizedName.push_back(letter);
			}
			//PrintText(1, "Personalized packet: %s, %s", item->personalizedName.c_str(), item->code);
		}

		item->isArmor = (item->attrs->flags & ITEM_GROUP_ALLARMOR) > 0;
		item->isWeapon = (item->attrs->flags & ITEM_GROUP_ALLWEAPON) > 0;

		if (item->isArmor) {
			item->defense = reader.read(11) - 10;
		}

		/*if(entry.throwable)
		{
			reader.read(9);
			reader.read(17);
		} else */
		//special case: indestructible phase blade
		if (item->code[0] == '7' && item->code[1] == 'c' && item->code[2] == 'r') {
			reader.read(8);
		} else if (item->isArmor || item->isWeapon) {
			item->maxDurability = reader.read(8);
			item->indestructible = item->maxDurability == 0;
			/*if (!item->indestructible) {
				item->durability = reader.read(8);
				reader.readBool();
			}*/
			//D2Hackit always reads it, hmmm. Appears to work.
			item->durability = reader.read(8);
			reader.readBool();
		}

		if (item->hasSockets) {
			item->sockets = (unsigned char)reader.read(4);
		}

		if (!item->identified) {
			return success;
		}

		if (item->attrs->stackable) {
			if (item->attrs->useable) {
				reader.read(5);
			}
			item->amount = reader.read(9);
		}

		if (item->quality == ITEM_QUALITY_SET) {
			unsigned long set_mods = reader.read(5);
		}

		while (true) {
			unsigned long stat_id = reader.read(9);
			if (stat_id == 0x1ff) {
				break;
			}
			ItemProperty prop = {};
			if (!ReadStat(stat_id, reader, prop)) {
				diagnostics.UnreadableStat(stat_id, item->code);
				if (stopOnUnreadableStat) {
					success = false;
					break;
				}
			}
			item->properties.push_back(prop);
		}
	} catch (int e) {
		// Only the unnamed catch below reports failure, which is how this has
		// always behaved: an item abandoned here is still filtered on whatever
		// was read before it gave out.
		diagnostics.Failed(item->code, "int " + std::to_string(e));
	} catch (std::exception const & ex) {
		diagnostics.Failed(item->code, ex.what());
	} catch(...) {
		diagnostics.Failed(item->code, "unknown");
		success = false;
	}
	return success;
}

bool Reader::ReadStat(unsigned int stat, BitReader &reader, ItemProperty &itemProp) const {
	StatProperties *bits = tables.Stat(stat);
	if (!bits) {
		return false;
	}
	unsigned int saveBits = bits->saveBits;
	unsigned int saveParamBits = bits->saveParamBits;
	unsigned int saveAdd = bits->saveAdd;
	itemProp.stat = stat;

	if (saveParamBits > 0) {
		switch (stat) {
			case STAT_CLASSSKILLS:
			{
				itemProp.characterClass = reader.read(saveParamBits);
				itemProp.value = reader.read(saveBits);
				return true;
			}
			case STAT_NONCLASSSKILL:
			case STAT_SINGLESKILL:
			{
				itemProp.skill = reader.read(saveParamBits);
				itemProp.value = reader.read(saveBits);
				return true;
			}
			case STAT_ELEMENTALSKILLS:
			{
				ulong element = reader.read(saveParamBits);
				itemProp.value = reader.read(saveBits);
				return true;
			}
			case STAT_AURA:
			{
				itemProp.skill = reader.read(saveParamBits);
				itemProp.value = reader.read(saveBits);
				return true;
			}
			case STAT_REANIMATE:
			{
				itemProp.monster = reader.read(saveParamBits);
				itemProp.value = reader.read(saveBits);
				return true;
			}
			case STAT_SKILLTAB:
			{
				itemProp.tab = reader.read(3);
				itemProp.characterClass = reader.read(3);
				ulong unknown = reader.read(10);
				itemProp.value = reader.read(saveBits);
				return true;
			}
			case STAT_SKILLONDEATH:
			case STAT_SKILLONHIT:
			case STAT_SKILLONKILL:
			case STAT_SKILLONLEVELUP:
			case STAT_SKILLONSTRIKING:
			case STAT_SKILLWHENSTRUCK:
			{
				itemProp.level = reader.read(6);
				itemProp.skill = reader.read(10);
				itemProp.skillChance = reader.read(saveBits);
				return true;
			}
			case STAT_CHARGED:
			{
				itemProp.level = reader.read(6);
				itemProp.skill = reader.read(10);
				itemProp.charges = reader.read(8);
				itemProp.maximumCharges = reader.read(8);
				return true;
			}
			case STAT_STATE:
			case STAT_ATTCKRTNGVSMONSTERTYPE:
			case STAT_DAMAGETOMONSTERTYPE:
			{
				// For some reason heroin_glands doesn't read these, even though
				// they have saveParamBits; maybe they don't occur in practice?
				itemProp.value = reader.read(saveBits) - saveAdd;
				return true;
			}
			default:
				reader.read(saveParamBits);
				reader.read(saveBits);
				return true;
		}
	}

	if (bits->op >= 2 && bits->op <= 5) {
		itemProp.perLevel = reader.read(saveBits);
		return true;
	}

	switch (stat) {
		case STAT_ENHANCEDMAXIMUMDAMAGE:
		case STAT_ENHANCEDMINIMUMDAMAGE:
		{
			itemProp.minimum = reader.read(saveBits);
			itemProp.maximum = reader.read(saveBits);
			return true;
		}
		case STAT_MINIMUMFIREDAMAGE:
		{
			itemProp.minimum = reader.read(saveBits);
			itemProp.maximum = reader.read(Widths(STAT_MAXIMUMFIREDAMAGE).saveBits);
			return true;
		}
		case STAT_MINIMUMLIGHTNINGDAMAGE:
		{
			itemProp.minimum = reader.read(saveBits);
			itemProp.maximum = reader.read(Widths(STAT_MAXIMUMLIGHTNINGDAMAGE).saveBits);
			return true;
		}
		case STAT_MINIMUMMAGICALDAMAGE:
		{
			itemProp.minimum = reader.read(saveBits);
			itemProp.maximum = reader.read(Widths(STAT_MAXIMUMMAGICALDAMAGE).saveBits);
			return true;
		}
		case STAT_MINIMUMCOLDDAMAGE:
		{
			itemProp.minimum = reader.read(saveBits);
			itemProp.maximum = reader.read(Widths(STAT_MAXIMUMCOLDDAMAGE).saveBits);
			itemProp.length = reader.read(Widths(STAT_COLDDAMAGELENGTH).saveBits);
			return true;
		}
		case STAT_MINIMUMPOISONDAMAGE:
		{
			itemProp.minimum = reader.read(saveBits);
			itemProp.maximum = reader.read(Widths(STAT_MAXIMUMPOISONDAMAGE).saveBits);
			itemProp.length = reader.read(Widths(STAT_POISONDAMAGELENGTH).saveBits);
			return true;
		}
		case STAT_REPAIRSDURABILITY:
		case STAT_REPLENISHESQUANTITY:
		{
			itemProp.value = reader.read(saveBits);
			return true;
		}
		default:
		{
			itemProp.value = reader.read(saveBits) - saveAdd;
			return true;
		}
	}
}
}  // namespace ItemFactsPacket
