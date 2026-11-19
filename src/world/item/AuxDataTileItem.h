#ifndef NET_MINECRAFT_WORLD_ITEM__AuxDataTileItem_H__
#define NET_MINECRAFT_WORLD_ITEM__AuxDataTileItem_H__

//package net.minecraft.world.item;

#include "TileItem.h"
#include "ItemInstance.h"
#include <string>
#include "../level/tile/Tile.h"

class AuxDataTileItem: public TileItem
{
    typedef TileItem super;
public:
    AuxDataTileItem(int id, Tile* parentTile)
    :   super(id),
        parentTile(parentTile)
    {
        setMaxDamage(0);
        setStackedByData(true);
    }

    /*@Override*/
    int getIcon(int itemAuxValue) {
        return parentTile->getTexture(2, itemAuxValue);
    }

    /*@Override*/
    int getLevelDataForAuxValue(int auxValue) {
        return auxValue;
    }
private:
    Tile* parentTile;
};

/** AuxDataTileItem whose name depends on the data value: "<tile id>.<suffix>". */
class VariantTileItem: public AuxDataTileItem
{
public:
    VariantTileItem(int id, Tile* parentTile, const char* const* suffixes, int count)
    :   AuxDataTileItem(id, parentTile), suffixes(suffixes), count(count) {}

    std::string getDescriptionId(const ItemInstance* instance) const {
        const int aux = instance ? instance->getAuxValue() : 0;
        std::string base = AuxDataTileItem::getDescriptionId(instance);
        if (aux >= 0 && aux < count && suffixes[aux] && suffixes[aux][0])
            return base + "." + suffixes[aux];
        return base;
    }
private:
    const char* const* suffixes;
    int count;
};

#endif /*NET_MINECRAFT_WORLD_ITEM__AuxDataTileItem_H__*/
