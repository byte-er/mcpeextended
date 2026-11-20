#include "OptionsItem.h"
#include "../../Minecraft.h"
#include "../../../util/Mth.h"
#include <cstdio>
OptionsItem::OptionsItem( std::string label, GuiElement* element, const Options::Option* valueOption )
: GuiElementContainer(false, true, 0, 0, 24, 12),
  label(label),
  valueOption(valueOption) {
	  addChild(element);
}

void OptionsItem::setupPositions() {
	int currentHeight = 0;
	for(std::vector<GuiElement*>::iterator it = children.begin(); it != children.end(); ++it) {
		(*it)->x = x + width - (*it)->width - 15;
		(*it)->y = y + currentHeight;
		currentHeight += (*it)->height;
	}
	height = currentHeight;
}

void OptionsItem::render( Minecraft* minecraft, int xm, int ym ) {
	int yOffset = (height - 8) / 2;
	std::string text = label;
	if (valueOption) {
		char buf[32];
		sprintf(buf, ": %d", (int)(minecraft->options.getProgressValue(valueOption) + 0.5f));
		text += buf;
	}
	minecraft->font->draw(text, (float)x, (float)y + yOffset, 0x909090, false);
	super::render(minecraft, xm, ym);
}